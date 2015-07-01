/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright(c) 2012-2013 University of California, Los Angeles
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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *	   Lijing Wang <wanglj11@mails.tsinghua.edu.cn>
 */

#ifndef ACTION_LOG_H
#define ACTION_LOG_H

#include "db-helper.h"
#include "file-state.h"
#include "sync-log.h"
#include "action-item.pb.h"
#include "file-item.pb.h"
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

#include <boost/tuple/tuple.hpp>

class ActionLog;
typedef boost::shared_ptr<ActionLog> ActionLogPtr;
typedef boost::shared_ptr<ActionItem> ActionItemPtr;

class ActionLog : public DbHelper {
public:
  typedef boost::function<void(std::string /*filename*/, ndn::Name /*device_name*/,
                               sqlite3_int64 /*seq_no*/, ndn::ConstBufferPtr /*hash*/,
                               time_t /*m_time*/, int /*mode*/, int /*seg_num*/)>
    OnFileAddedOrChangedCallback;

  typedef boost::function<void(std::string /*filename*/)> OnFileRemovedCallback;

public:
  ActionLog(boost::shared_ptr<ndn::Face> face, const boost::filesystem::path& path,
            SyncLogPtr syncLog, const std::string& sharedFolder, const std::string& appName,
            OnFileAddedOrChangedCallback onFileAddedOrChanged, OnFileRemovedCallback onFileRemoved);

  virtual ~ActionLog()
  {
  }

  //////////////////////////
  // Local operations     //
  //////////////////////////
  ActionItemPtr
  AddLocalActionUpdate(const std::string& filename, const ndn::Buffer& hash, time_t wtime, int mode,
                       int seg_num);

  // void
  // AddActionMove(const std::string &oldFile, const std::string &newFile);

  ActionItemPtr
  AddLocalActionDelete(const std::string& filename);

  //////////////////////////
  // Remote operations    //
  //////////////////////////

  ActionItemPtr
  AddRemoteAction(const ndn::Name& deviceName, sqlite3_int64 seqno,
                  ndn::shared_ptr<ndn::Data> actionData);

  /**
   * @brief Add remote action using just action's parsed content object
   *
   * This function extracts device name and sequence number from the content object's and calls the
   *overloaded method
   */
  ActionItemPtr
  AddRemoteAction(ndn::shared_ptr<ndn::Data> actionData);

  ///////////////////////////
  // General operations    //
  ///////////////////////////

  ndn::shared_ptr<ndn::Data>
  LookupActionData(const ndn::Name& deviceName, sqlite3_int64 seqno);

  ndn::shared_ptr<ndn::Data>
  LookupActionData(const ndn::Name& actionName);

  ActionItemPtr
  LookupAction(const ndn::Name& deviceName, sqlite3_int64 seqno);

  ActionItemPtr
  LookupAction(const ndn::Name& actionName);

  FileItemPtr
  LookupAction(const std::string& filename, sqlite3_int64 version, const ndn::Buffer& filehash);

  /**
   * @brief Lookup up to [limit] actions starting [offset] in decreasing order(by timestamp) and
   * calling visitor(device_name,seqno,action) for each action
   */
  bool
  LookupActionsInFolderRecursively(
    const boost::function<void(const ndn::Name& name, sqlite3_int64 seq_no, const ActionItem&)>&
      visitor,
    const std::string& folder, int offset = 0, int limit = -1);

  bool
  LookupActionsForFile(const boost::function<void(const ndn::Name& name, sqlite3_int64 seq_no,
                                                  const ActionItem&)>& visitor,
                       const std::string& file, int offset = 0, int limit = -1);

  void
  LookupRecentFileActions(const boost::function<void(const std::string&, int, int)>& visitor,
                          int limit = 5);

  //
  inline FileStatePtr
  GetFileState();

public:
  // for test purposes
  sqlite3_int64
  LogSize();

private:
  boost::tuple<sqlite3_int64 /*version*/, ndn::BufferPtr /*device name*/, sqlite3_int64 /*seq_no*/>
  GetLatestActionForFile(const std::string& filename);

  static void
  apply_action_xFun(sqlite3_context* context, int argc, sqlite3_value** argv);

  //  static boost::shared_ptr<ActionItem>
  //  deserializeActionItem(const ndn::Block &content)
  //  {
  //  	boost::shared_ptr<ActionItem> retval(new ActionItem());
  //  	if (!retval->ParseFromArray(content.value(), content.value_size()))
  //  	{
  //  		return boost::shared_ptr<ActionItem>();
  //  	}
  //  	return retval;
  //  }

private:
  SyncLogPtr m_syncLog;
  FileStatePtr m_fileState;

  boost::shared_ptr<ndn::Face> m_face;
  std::string m_sharedFolderName;
  std::string m_appName;

  OnFileAddedOrChangedCallback m_onFileAddedOrChanged;
  OnFileRemovedCallback m_onFileRemoved;
  ndn::KeyChain m_keyChain;
};

namespace Error {
struct ActionLog : virtual boost::exception, virtual std::exception {
};
}

inline FileStatePtr
ActionLog::GetFileState()
{
  return m_fileState;
}

#endif // ACTION_LOG_H
