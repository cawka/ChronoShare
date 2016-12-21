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
 * Author: Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *         Lijing Wang <wanglj11@mails.tsinghua.edu.cn>
 */

#ifndef STATE_SERVER_H
#define STATE_SERVER_H

#include "digest-computer.h"
#include "object-manager.h"
#include "object-db.h"
#include "action-log.h"
#include <set>
#include <map>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include "executor.h"

#include "../contrib/json_spirit/json_spirit_writer_template.h"
#include "../contrib/json_spirit/json_spirit_value.h"

#ifndef JSON_SPIRIT_VALUE_ENABLED
#error Please define JSON_SPIRIT_VALUE_ENABLED for the Value type to be enabled
#endif

/**
 * @brief Class serving state information from ChronoShare
 *
 * Eventually, the same info/actions can be made available via a global scope prefix
 *
 * Information available:
 *
 * For now serving only locally(using <PREFIX> =
 */localhop/<user's-device-name>/"chronoshare"/<FOLDER>/"info")
 *
 * - state: get list of SyncNodes, their sequence numbers, and forwarding hint(almost the same as
 *RECOVERY interest)
 *
 *   <PREFIX_INFO>/"state"  (@todo: authentification code or authentication code should in addition
 *somewhere)
 *
 * - action
 *
 *   Get list of actions for a folder(for all files under this folder)
 *
 *   <PREFIX_INFO>/"actions"/"folder"/<offset>  (all actions)
 *   or
 *   <PREFIX_INFO>/"actions"/"folder"/<one-component-relative-file-name>/<offset>
 *
 *   Actions are ordered in decreasing order(latest will go first).
 *
 *   Each data packet contains up to 100 actions.
 *
 *   TEMPORARILY LIMIT IS REDUCED TO 10 !(for debug purposes)
 *  (may be even not temporarily...)
 *
 *   If more items are available, application data will specify URL for the next packet
 *
 *   Format of returned data(JSON):
 *   {
 *      "actions": [
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
 *          };
 *      },
 *
 *      // only if there are more actions available
 *      "more": "next segment number"
 *   }
 *
 *
 * - file
 *
 *   <PREFIX_INFO>/"files"/"folder"/<offset>  (full filestate)
 *   or
 *   <PREFIX_INFO>/"files"/"folder"/<one-component-relative-folder-name>/<offset>
 *
 *   Each Data packets contains a list of up to 100 files.
 *   If more items are available, application data will specify URL for the next packet
 *
 *   TEMPORARILY LIMIT IS REDUCED TO 10 !(for debug purposes)
 *  (may be even not temporarily...)
 *
 *   Format of returned data(JSON):
 *   {
 *      "files": [
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
 *      "more": "next segment number"
 *   }
 *
 * Commands available:
 *
 * For now serving only locally(using <PREFIX_CMD> =
 */localhop/<user's-device-name>/"chronoshare"/<FOLDER>/"cmd")
 *
 * - restore version of the file
 *
 *   <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>
 *   or
 *   <PREFIX_CMD>/"restore"/"file"/<one-component-relative-file-name>/<version>/<file-hash>
 *
 * - clean state log
 *  (this may not need to be here, if we implement periodic cleaning)
 * - ? flatten action log(should be supported eventually, but not supported now)
 */
class StateServer {
public:
  StateServer(boost::shared_ptr<ndn::Face> face, ActionLogPtr actionLog,
              const boost::filesystem::path& rootDir, const ndn::Name& userName,
              const std::string& sharedFolderName, const std::string& appName,
              ObjectManager& objectManager, int freshness = -1);
  ~StateServer();

private:
  void
  info_actions_folder(const ndn::InterestFilter&, const ndn::Interest&);

  void
  info_actions_file(const ndn::InterestFilter&, const ndn::Interest&);

  void
  info_actions_fileOrFolder_Execute(const ndn::Name& interest, bool isFolder = true);

  void
  info_files_folder(const ndn::InterestFilter&, const ndn::Interest&);

  void
  info_files_folder_Execute(const ndn::Name& interest);

  void
  cmd_restore_file(const ndn::InterestFilter&, const ndn::Interest&);

  void
  cmd_restore_file_Execute(const ndn::Name& interest);

private:
  void
  registerPrefixes();

  void
  deregisterPrefixes();

  static void
  formatActionJson(json_spirit::Array& actions, const ndn::Name& name, sqlite3_int64 seq_no,
                   const ActionItem& action);

  static void
  formatFilestateJson(json_spirit::Array& files, const FileItem& file);

private:
  boost::shared_ptr<ndn::Face> m_face;
  ActionLogPtr m_actionLog;
  ObjectManager& m_objectManager;

  ndn::Name m_PREFIX_INFO;
  ndn::Name m_PREFIX_CMD;

  const ndn::RegisteredPrefixId* actionsFolderId;
  const ndn::RegisteredPrefixId* actionsFileId;
  const ndn::RegisteredPrefixId* filesFolderId;
  const ndn::RegisteredPrefixId* restoreFileId;

  boost::filesystem::path m_rootDir;
  int m_freshness;

  Executor m_executor;

  ndn::Name m_userName;
  std::string m_sharedFolderName;
  std::string m_appName;
  ndn::KeyChain m_keyChain;
  DigestComputer m_digestComputer;
};
#endif // CONTENT_SERVER_H
