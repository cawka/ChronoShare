/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 *	       Lijing Wang <wanglj11@mails.tsinghua.edu.cn>
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "digest-computer.h"
#include "action-log.h"
#include "sync-core.h"
#include "executor.h"
#include "object-db.h"
#include "object-manager.h"
#include "content-server.h"
#include "state-server.h"
#include "fetch-manager.h"

#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

typedef boost::shared_ptr<ActionItem> ActionItemPtr;

// TODO:
// This class lacks a permanent table to store the files in fetching process
// and fetch the missing pieces for those in the table after the application launches
class Dispatcher
{
public:
  // sharedFolder is the name to be used in NDN name;
  // rootDir is the shared folder dir in local file system;
  Dispatcher(const std::string &localUserName
             , const std::string &sharedFolder
             , const boost::filesystem::path &rootDir
             , boost::shared_ptr<ndn::Face> face 
             , bool enablePrefixDiscovery = true
             );
  ~Dispatcher();

  // ----- Callbacks, they only submit the job to executor and immediately return so that event processing thread won't be blocked for too long -------


  // callback to process local file change
  void
  Did_LocalFile_AddOrModify(const boost::filesystem::path &relativeFilepath);

  void
  Did_LocalFile_Delete(const boost::filesystem::path &relativeFilepath);

  /**
   * @brief Invoked when FileState is detected to have a file which does not exist on a file system
   */
  void
  Restore_LocalFile(FileItemPtr file);

  // for test
  ndn::ConstBufferPtr
  SyncRoot() { return m_core->root(); }

  inline void
  LookupRecentFileActions(const boost::function<void(const std::string &, int, int)> &visitor, int limit) { m_actionLog->LookupRecentFileActions(visitor, limit); }


private:

  void
  listen() {
    printf("m_face start listening ...\n");
    m_face->processEvents();
    printf("m_face listen Over !!! \n");
  }

  void
  listen_other(boost::shared_ptr<ndn::Face> face, std::string name) {
    printf("%s start listening ...\n", name.c_str());
    std::cout << name << "start listening ... " << std::endl;
    face->processEvents();
    printf("%s listen Over !!!\n", name.c_str());
  }

  void
  Did_LocalFile_AddOrModify_Execute(boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault

  void
  Did_LocalFile_Delete_Execute(boost::filesystem::path relativeFilepath); // cannot be const & for Execute event!!! otherwise there will be segfault

  void
  Restore_LocalFile_Execute(FileItemPtr file);

private:
  /**
   * Callbacks:
   *
 x * - from SyncLog: when state changes -> to fetch missing actions
   *
 x * - from FetchManager/Actions: when action is fetched -> to request a file, specified by the action
   *                                                     -> to add action to the action log
   *
   * - from ActionLog/Delete:      when action applied(file state changed, file deleted)           -> to delete local file
   *
   * - from ActionLog/AddOrUpdate: when action applied(file state changes, file added or modified) -> to assemble the file if file is available in the ObjectDb, otherwise, do nothing
   *
 x * - from FetchManager/Files: when file segment is retrieved -> save it in ObjectDb
   *                            when file fetch is completed   -> if file belongs to FileState, then assemble it to filesystem. Don't do anything otherwise
   */

  // callback to process remote sync state change
  void
  Did_SyncLog_StateChange(SyncStateMsgPtr stateMsg);

  void
  Did_SyncLog_StateChange_Execute(SyncStateMsgPtr stateMsg);

  void
  Did_FetchManager_ActionFetch(const ndn::Name &deviceName, const ndn::Name &actionName, uint32_t seqno, ndn::shared_ptr<ndn::Data> actionData);

  void
  Did_ActionLog_ActionApply_Delete(const std::string &filename);

  void
  Did_ActionLog_ActionApply_Delete_Execute(std::string filename);

  // void
  // Did_ActionLog_ActionApply_AddOrModify(const std::string &filename, ndn::Name device_name, sqlite3_int64 seq_no,
  //                                        ndn::ConstBufferPtr hash, time_t m_time, int mode, int seg_num);

  void
  Did_FetchManager_FileSegmentFetch(const ndn::Name &deviceName, const ndn::Name &fileSegmentName, uint32_t segment, ndn::shared_ptr<ndn::Data> fileSegmentData);

  void
  Did_FetchManager_FileSegmentFetch_Execute(ndn::Name deviceName, ndn::Name fileSegmentName, uint32_t segment, ndn::shared_ptr<ndn::Data> fileSegmentData);

  void
  Did_FetchManager_FileFetchComplete(const ndn::Name &deviceName, const ndn::Name &fileBaseName);

  void
  Did_FetchManager_FileFetchComplete_Execute(ndn::Name deviceName, ndn::Name fileBaseName);

  void
  Did_LocalPrefix_Updated(const ndn::Name &prefix);

private:
  void
  AssembleFile_Execute(const ndn::Name &deviceName, const ndn::Buffer &filehash, const boost::filesystem::path &relativeFilepath);

  // void
  // fileChanged(const boost::filesystem::path &relativeFilepath, ActionType type);

  // void
  // syncStateChanged(const SyncStateMsgPtr &stateMsg);

  // void
  // actionReceived(const ActionItemPtr &actionItem);

  // void
  // fileSegmentReceived(const ndn::Name &name, const Ccnx::Bytes &content);

  // void
  // fileReady(const ndn::Name &fileNamePrefix);

private:
  boost::shared_ptr<ndn::Face> m_face;
  SyncCore *m_core;
  SyncLogPtr   m_syncLog;
  ActionLogPtr m_actionLog;
  FileStatePtr m_fileState;

  boost::filesystem::path m_rootDir;
  Executor m_executor;
  ObjectManager m_objectManager;
  ndn::Name m_localUserName;
  // maintain object db ptrs so that we don't need to create them
  // for every fetched segment of a file

  std::map<ndn::Buffer, ObjectDbPtr> m_objectDbMap;

  std::string m_sharedFolder;
  ContentServer *m_server;
  StateServer   *m_stateServer;
  bool m_enablePrefixDiscovery;

  FetchManagerPtr m_actionFetcher;
  FetchManagerPtr m_fileFetcher;
  DigestComputer m_digestComputer;

  boost::shared_ptr<ndn::Face> m_face_server;
  boost::shared_ptr<ndn::Face> m_face_stateServer;
  boost::thread m_faceListening;
  boost::thread m_serverListening;
  boost::thread m_stateServerListening;

};

namespace Error
{
  struct Dispatcher : virtual boost::exception, virtual std::exception {};
  typedef boost::error_info<struct tag_errmsg, std::string> error_info_str;
}

#endif // DISPATCHER_H

