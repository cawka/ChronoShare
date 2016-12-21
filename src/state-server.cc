/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright(c) 2013 University of California, Los Angeles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Lijing Wang <wanglj11@mails.tsinghua.edu.cn>
 */

#include "state-server.h"
#include "logging.h"
#include <boost/make_shared.hpp>
#include <utility>
#include "task.h"
#include "periodic-task.h"
#include "simple-interval-generator.h"
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/fstream.hpp>

INIT_LOGGER("StateServer");

using namespace std;
using namespace boost;
using namespace ndn;

namespace fs = boost::filesystem;

StateServer::StateServer(boost::shared_ptr<ndn::Face> face, ActionLogPtr actionLog,
                         const boost::filesystem::path &rootDir,
                         const ndn::Name &userName, const std::string &sharedFolderName,
                         const std::string &appName,
                         ObjectManager &objectManager,
                         int freshness/* = -1*/)
  : m_face(face)
  , m_actionLog(actionLog)
  , m_objectManager(objectManager)
  , m_rootDir(rootDir)
  , m_freshness(freshness)
  , m_executor(1)
  , m_userName(userName)
  , m_sharedFolderName(sharedFolderName)
  , m_appName(appName)
{
  // may be later /localhost should be replaced with /%C1.M.S.localhost

  // <PREFIX_INFO> = /localhost/<user's-device-name>/"chronoshare"/"info"
  m_PREFIX_INFO = ndn::Name("/localhost");
  m_PREFIX_INFO.append(m_userName).append("chronoshare").append(m_sharedFolderName).append("info");

  // <PREFIX_CMD> = /localhost/<user's-device-name>/"chronoshare"/"cmd"
  m_PREFIX_CMD = ndn::Name("/localhost");
  m_PREFIX_CMD.append(m_userName).append("chronoshare").append(m_sharedFolderName).append("cmd");

  m_executor.start();

  registerPrefixes();
}

StateServer::~StateServer()
{
  m_executor.shutdown();

  deregisterPrefixes();
}

void
StateServer::registerPrefixes()
{
  // currently supporting limited number of command.
  // will be extended to support all planned commands later

   // <PREFIX_INFO>/"actions"/"all"/<segment>  get list of all actions
  ndn::Name actionsFolder = ndn::Name(m_PREFIX_INFO);
  actionsFolder.append("actions").append("folder");
  actionsFolderId = m_face->setInterestFilter(ndn::InterestFilter(actionsFolder), 
                                              bind(&StateServer::info_actions_folder, this, _1, _2),
                                              RegisterPrefixSuccessCallback(),
                                              RegisterPrefixFailureCallback());

  ndn::Name actionsFile = ndn::Name(m_PREFIX_INFO);
  actionsFile.append("actions").append("file");
  actionsFileId = m_face->setInterestFilter(ndn::InterestFilter(actionsFile), 
                                            bind(&StateServer::info_actions_file, this, _1, _2),
                                            RegisterPrefixSuccessCallback(),
                                            RegisterPrefixFailureCallback());                                            

  // <PREFIX_INFO>/"filestate"/"all"/<segment>
  ndn::Name filesFolder = ndn::Name(m_PREFIX_INFO);
  filesFolder.append("files").append("folder");
  filesFolderId = m_face->setInterestFilter(ndn::InterestFilter(filesFolder), 
                                            bind(&StateServer::info_files_folder, this, _1, _2),
                                            RegisterPrefixSuccessCallback(),
                                            RegisterPrefixFailureCallback());

  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
  ndn::Name restoreFile = ndn::Name(m_PREFIX_CMD);
  restoreFile.append("restore").append("file");
  restoreFileId = m_face->setInterestFilter(ndn::InterestFilter(restoreFile), 
                                            bind(&StateServer::cmd_restore_file, this, _1, _2),
                                            RegisterPrefixSuccessCallback(),
                                            RegisterPrefixFailureCallback());
}

void
StateServer::deregisterPrefixes()
{
  m_face->unsetInterestFilter(actionsFolderId);
  m_face->unsetInterestFilter(actionsFileId);
  m_face->unsetInterestFilter(filesFolderId);
  m_face->unsetInterestFilter(restoreFileId);
}

void
StateServer::formatActionJson(json_spirit::Array &actions,
                               const ndn::Name &name, sqlite3_int64 seq_no, const ActionItem &action)
{
/*
 *      {
 *          "id": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          },
 *          "timestamp": "<ACTION-TIMESTAMP>",
 *          "filename": "<FILENAME>",
 *
 *          "action": "UPDATE | DELETE",
 *
 *          // only if update
 *          "update": {
 *              "hash": "<FILE-HASH>",
 *              "timestamp": "<FILE-TIMESTAMP>",
 *              "chmod": "<FILE-MODE>",
 *              "segNum": "<NUMBER-OF-SEGMENTS(~file size)>"
 *          },
 *
 *          // if parent_device_name is set
 *          "parentId": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          }
 *      }
 */

  using namespace json_spirit;
  using namespace boost::posix_time;

  Object json;
  Object id;

  id.push_back(Pair("userName", boost::lexical_cast<string>(name)));
  id.push_back(Pair("seqNo",    static_cast<int64_t>(seq_no)));

  json.push_back(Pair("id", id));

  json.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(action.timestamp()))));
  json.push_back(Pair("filename",  action.filename()));
  json.push_back(Pair("version",  action.version()));
  json.push_back(Pair("action",(action.action() == 0) ? "UPDATE" : "DELETE"));

  if (action.action() == 0)
    {
      Object update;
      const ndn::Buffer hash(action.file_hash().c_str(), action.file_hash().size());
      update.push_back(Pair("hash", DigestComputer::digestToString(hash)));
      update.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(action.mtime()))));

      ostringstream chmod;
      chmod << setbase(8) << setfill('0') << setw(4) << action.mode();
      update.push_back(Pair("chmod", chmod.str()));

      update.push_back(Pair("segNum", action.seg_num()));
      json.push_back(Pair("update", update));
    }

  if (action.has_parent_device_name())
    {
      Object parentId;
      ndn::Name parent_device_name(action.parent_device_name());
      id.push_back(Pair("userName", boost::lexical_cast<string>(parent_device_name)));
      id.push_back(Pair("seqNo",    action.parent_seq_no()));

      json.push_back(Pair("parentId", parentId));
    }

  actions.push_back(json);
}

void
StateServer::info_actions_folder(const InterestFilter& interesFilter, const Interest& interestTrue)
{
  Name interest = interestTrue.getName();
  if (interest.size() - m_PREFIX_INFO.size() != 3 &&
      interest.size() - m_PREFIX_INFO.size() != 4)
    {
      _LOG_DEBUG("Invalid interest: " << interest);
      return;
    }

  _LOG_DEBUG(">> info_actions_folder: " << interest);
  m_executor.execute(bind(&StateServer::info_actions_fileOrFolder_Execute, this, interest, true));
}

void
StateServer::info_actions_file(const InterestFilter& interesFilter, const Interest& interestTrue)
{
  Name interest = interestTrue.getName();
  if (interest.size() - m_PREFIX_INFO.size() != 3 &&
      interest.size() - m_PREFIX_INFO.size() != 4)
    {
      _LOG_DEBUG("Invalid interest: " << interest);
      return;
    }

  _LOG_DEBUG(">> info_actions_file: " << interest);
  m_executor.execute(bind(&StateServer::info_actions_fileOrFolder_Execute, this, interest, false));
}


void
StateServer::info_actions_fileOrFolder_Execute(const ndn::Name &interest, bool isFolder/* = true*/)
{
  // <PREFIX_INFO>/"actions"/"folder|file"/<folder|file>/<offset>  get list of all actions
	if (interest.size() < 1) {
		// ignore any unexpected interests and errors
		_LOG_ERROR("empty interest name");
		return;
	}
  uint64_t offset = interest.get(-1).toNumber();

   /// @todo !!! add security checking

   string fileOrFolderName;
   if (interest.size() - m_PREFIX_INFO.size() == 4)
     fileOrFolderName = interest.get(-2).toUri();
   else // == 3
     fileOrFolderName = "";
/*
 * {
 *    "actions": [
 *         ...
 *    ],
 *
 *    // only if there are more actions available
 *    "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-ACTION>"
 * }
 */

  using namespace json_spirit;
  Object json;

  Array actions;
  bool more;
  if (isFolder)
    {
      more = m_actionLog->LookupActionsInFolderRecursively
       (boost::bind(StateServer::formatActionJson, boost::ref(actions), _1, _2, _3),
         fileOrFolderName, offset*10, 10);
    }
  else
    {
      more = m_actionLog->LookupActionsForFile
       (boost::bind(StateServer::formatActionJson, boost::ref(actions), _1, _2, _3),
         fileOrFolderName, offset*10, 10);
    }

  json.push_back(Pair("actions", actions));

  if (more)
    {
      json.push_back(Pair("more", lexical_cast<string>(offset + 1)));
      // ndn::Name more = Name(interest.getPartialName(0, interest.size() - 1))(offset + 1);
      // json.push_back(Pair("more", lexical_cast<string>(more)));
    }

  ostringstream os;
	write_stream(Value(json), os, pretty_print | raw_utf8);

  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
	data->setName(interest);
	data->setFreshnessPeriod(time::seconds(60));
	data->setContent(reinterpret_cast<const uint8_t*>(os.str().c_str()), os.str().size());
  m_keyChain.sign(*data);
	m_face->put(*data);
}

void
StateServer::formatFilestateJson(json_spirit::Array &files, const FileItem &file)
{
/**
 *   {
 *      "filestate": [
 *      {
 *          "filename": "<FILENAME>",
 *          "owner": {
 *              "userName": "<NDN-NAME-OF-THE-USER>",
 *              "seqNo": "<SEQ_NO_OF_THE_ACTION>"
 *          },
 *
 *          "hash": "<FILE-HASH>",
 *          "timestamp": "<FILE-TIMESTAMP>",
 *          "chmod": "<FILE-MODE>",
 *          "segNum": "<NUMBER-OF-SEGMENTS(~file size)>"
 *      }, ...,
 *      ]
 *
 *      // only if there are more actions available
 *      "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-FILESTATE>"
 *   }
 */
  using namespace json_spirit;
  using namespace boost::posix_time;

  Object json;

  json.push_back(Pair("filename",  file.filename()));
  json.push_back(Pair("version",   file.version()));
  {
    Object owner;
    ndn::Name device_name(file.device_name());
    owner.push_back(Pair("userName", boost::lexical_cast<string>(device_name)));
    owner.push_back(Pair("seqNo",    file.seq_no()));

    json.push_back(Pair("owner", owner));
  }

  json.push_back(Pair("hash", DigestComputer::digestToString(ndn::Buffer(file.file_hash().c_str(), file.file_hash().size()))));
  json.push_back(Pair("timestamp", to_iso_extended_string(from_time_t(file.mtime()))));

  ostringstream chmod;
  chmod << setbase(8) << setfill('0') << setw(4) << file.mode();
  json.push_back(Pair("chmod", chmod.str()));

  json.push_back(Pair("segNum", file.seg_num()));

  files.push_back(json);
}

void debugFileState(const FileItem &file)
{
  std::cout << file.filename() << std::endl;
}

void
StateServer::info_files_folder(const InterestFilter& interesFilter, const Interest& interestTrue)
{
  Name interest = interestTrue.getName();
  if (interest.size() - m_PREFIX_INFO.size() != 3 &&
      interest.size() - m_PREFIX_INFO.size() != 4)
    {
      _LOG_DEBUG("Invalid interest: " << interest << ", " << interest.size() - m_PREFIX_INFO.size());
      return;
    }

  _LOG_DEBUG(">> info_files_folder: " << interest);
  m_executor.execute(bind(&StateServer::info_files_folder_Execute, this, interest));
}


void
StateServer::info_files_folder_Execute(const ndn::Name &interest)
{
  // <PREFIX_INFO>/"filestate"/"folder"/<one-component-relative-folder-name>/<offset>
	if (interest.size() < 1) {
		// ignore any unexpected interests and errors
		_LOG_ERROR("empty interest name");
		return;
	}
  uint64_t offset = interest.get(-1).toNumber();

  // /// @todo !!! add security checking

  string folder;
  if (interest.size() - m_PREFIX_INFO.size() == 4)
    folder = interest.get(-2).toUri();
  else // == 3
    folder = "";

/*
 *{
 *  "files": [
 *       ...
 *  ],
 *
 *  // only if there are more actions available
 *  "more": "<NDN-NAME-OF-NEXT-SEGMENT-OF-ACTION>"
 *}
 */

  using namespace json_spirit;
  Object json;

  Array files;
  bool more = m_actionLog
    ->GetFileState()
    ->LookupFilesInFolderRecursively
   (boost::bind(StateServer::formatFilestateJson, boost::ref(files), _1),
     folder, offset*10, 10);

  json.push_back(Pair("files", files));

  if (more)
    {
      json.push_back(Pair("more", lexical_cast<string>(offset + 1)));
      // ndn::Name more = Name(interest.getPartialName(0, interest.size() - 1))(offset + 1);
      // json.push_back(Pair("more", lexical_cast<string>(more)));
    }

  ostringstream os;
  write_stream(Value(json), os, pretty_print | raw_utf8);

  ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
	data->setName(interest);
	data->setFreshnessPeriod(time::seconds(60));
	data->setContent(reinterpret_cast<const uint8_t*>(os.str().c_str()), os.str().size());
  m_keyChain.sign(*data);
	m_face->put(*data);

}


void
StateServer::cmd_restore_file(const InterestFilter& interesFilter, const Interest& interestTrue)
{
  Name interest = interestTrue.getName();
  if (interest.size() - m_PREFIX_CMD.size() != 4 &&
      interest.size() - m_PREFIX_CMD.size() != 5)
    {
      _LOG_DEBUG("Invalid interest: " << interest);
      return;
    }

  _LOG_DEBUG(">> cmd_restore_file: " << interest);
  m_executor.execute(bind(&StateServer::cmd_restore_file_Execute, this, interest));
}

void
StateServer::cmd_restore_file_Execute(const ndn::Name &interest)
{
  // <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>

  /// @todo !!! add security checking


      FileItemPtr file;

      if (interest.size() - m_PREFIX_CMD.size() == 5)
        {
          const ndn::Buffer hash(interest.get(-1).value(), interest.get(-1).value_size());
		      uint64_t version = interest.get(-2).toNumber();
		      string  filename = interest.get(-3).toUri(); // should be safe even with full relative path

          file = m_actionLog->LookupAction(filename, version, hash);
          if (!file)
            {
              _LOG_ERROR("Requested file is not found: [" << filename << "] version [" << version << "] hash [" << DigestComputer::digestToString(hash) << "]");
            }
        }
      else
        {
		      uint64_t version = interest.get(-1).toNumber();
		      string  filename = interest.get(-2).toUri();
          file = m_actionLog->LookupAction(filename, version, ndn::Buffer(0,0));
          if (!file)
            {
              _LOG_ERROR("Requested file is not found: [" << filename << "] version [" << version << "]");
            }
        }

      if (!file)
        {
          ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
      		data->setName(interest);
      		data->setFreshnessPeriod(time::seconds(60));
      		string msg = "FAIL: Requested file is not found";
      		data->setContent(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
          m_keyChain.sign(*data);
      		m_face->put(*data);
          return;
        }

      const ndn::Buffer hash(file->file_hash().c_str(), file->file_hash().size());

      ///////////////////
      // now the magic //
      ///////////////////

    	boost::filesystem::path filePath = m_rootDir / file->filename();
    	ndn::Name deviceName = ndn::Name(file->device_name().c_str()).getSubName(0, file->device_name().size());

      try
        {
          if (filesystem::exists(filePath) &&
              filesystem::last_write_time(filePath) == file->mtime() &&
#if BOOST_VERSION >= 104900
              filesystem::status(filePath).permissions() == static_cast<filesystem::perms>(file->mode()) &&
#endif
              *m_digestComputer.digestFromFile(filePath) == hash)
            {
              ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
      		    data->setName(interest);
      		    data->setFreshnessPeriod(time::seconds(60));
      		    string msg = "OK: File already exists";
      		    data->setContent(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
              m_keyChain.sign(*data);
      		    m_face->put(*data);
              _LOG_DEBUG("Asking to assemble a file, but file already exists on a filesystem");
              return;
            }
        }
      catch(filesystem::filesystem_error &error)
        {
          ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
      		data->setName(interest);
      		data->setFreshnessPeriod(time::seconds(60));
      		string msg = "FAIL: File operation failed";
      		data->setContent(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
          m_keyChain.sign(*data);
      		m_face->put(*data);
          _LOG_ERROR("File operations failed on [" << filePath << "](ignoring)");
        }

      _LOG_TRACE("Restoring file [" << filePath << "]");
      if (m_objectManager.objectsToLocalFile(deviceName, hash, filePath))
        {
          last_write_time(filePath, file->mtime());
#if BOOST_VERSION >= 104900
          permissions(filePath, static_cast<filesystem::perms>(file->mode()));
#endif
          ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
      		data->setName(interest);
      		data->setFreshnessPeriod(time::seconds(60));
      		string msg = "OK";
      		data->setContent(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
          m_keyChain.sign(*data);
      		m_face->put(*data);
        }
      else
        {
          ndn::shared_ptr<ndn::Data> data = ndn::make_shared<ndn::Data>();
      		data->setName(interest);
      		data->setFreshnessPeriod(time::seconds(60));
      		string msg = "FAIL: Unknown error while restoring file";
      		data->setContent(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
          m_keyChain.sign(*data);
      		m_face->put(*data);
        }
}

