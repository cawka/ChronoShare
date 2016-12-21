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
 *	   Zhenkai Zhu <zhenkai@cs.ucla.edu>
 *	   Lijing Wang <wanglj11@mails.tsinghua.edu.cn>
 */

#ifndef SYNC_LOG_H
#define SYNC_LOG_H

#include "db-helper.h"
#include "digest-computer.h"
#include <sync-state.pb.h>
#include <ndn-cxx/name.hpp>
#include <map>
#include <boost/thread/shared_mutex.hpp>

typedef boost::shared_ptr<SyncStateMsg> SyncStateMsgPtr;

class SyncLog : public DbHelper
{
public:
  SyncLog(const boost::filesystem::path &path, const ndn::Name &localName);

  /**
   * @brief Get local username
   */
  inline const ndn::Name &
  GetLocalName() const;

  sqlite3_int64
  GetNextLocalSeqNo(); // side effect: local seq_no will be increased

  // done
  void
  UpdateDeviceSeqNo(const ndn::Name &name, sqlite3_int64 seqNo);

  void
  UpdateLocalSeqNo(sqlite3_int64 seqNo);

  ndn::Name
  LookupLocator(const ndn::Name &deviceName);

  ndn::Name
  LookupLocalLocator();

  void
  UpdateLocator(const ndn::Name &deviceName, const ndn::Name &locator);

  void
  UpdateLocalLocator(const ndn::Name &locator);

  // done
  /**
   * Create an 1ntry in SyncLog and SyncStateNodes corresponding to the current state of SyncNodes
   */
  ndn::ConstBufferPtr
  RememberStateInStateLog();

  // done
  sqlite3_int64
  LookupSyncLog(const std::string &stateHash);

  // done
  sqlite3_int64
  LookupSyncLog(const ndn::Buffer &stateHash);

  // How difference is exposed will be determined later by the actual protocol
  SyncStateMsgPtr
  FindStateDifferences(const std::string &oldHash, const std::string &newHash, bool includeOldSeq = false);

  SyncStateMsgPtr
  FindStateDifferences(const ndn::Buffer &oldHash, const ndn::Buffer &newHash, bool includeOldSeq = false);

  //-------- only used in test -----------------
  sqlite3_int64
  SeqNo(const ndn::Name &name);

  sqlite3_int64
  LogSize();

protected:
  void
  UpdateDeviceSeqNo(sqlite3_int64 deviceId, sqlite3_int64 seqNo);

protected:
  ndn::Name m_localName;

  sqlite3_int64 m_localDeviceId;

  typedef boost::mutex Mutex;
  typedef boost::unique_lock<Mutex> WriteLock;

  Mutex m_stateUpdateMutex;
  DigestComputer m_digestComputer;
};

typedef boost::shared_ptr<SyncLog> SyncLogPtr;

const ndn::Name &
SyncLog::GetLocalName() const
{
  return m_localName;
}

#endif // SYNC_LOG_H
