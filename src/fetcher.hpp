/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2016, Regents of the University of California.
 *
 * This file is part of ChronoShare, a decentralized file sharing application over NDN.
 *
 * ChronoShare is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoShare is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received copies of the GNU General Public License along with
 * ChronoShare, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ChronoShare authors and contributors.
 */

#ifndef FETCHER_H
#define FETCHER_H

#include "ccnx-name.h"
#include "ccnx-wrapper.h"

#include "executor.h"
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/intrusive/list.hpp>
#include <set>

#include <set>

class FetchManager;

class Fetcher
{
public:
  typedef boost::function<void(Ccnx::Name& deviceName, Ccnx::Name& baseName, uint64_t seq, Ccnx::PcoPtr pco)>
    SegmentCallback;
  typedef boost::function<void(Ccnx::Name& deviceName, Ccnx::Name& baseName)> FinishCallback;
  typedef boost::function<void(Fetcher&, const Ccnx::Name& deviceName, const Ccnx::Name& baseName)>
    OnFetchCompleteCallback;
  typedef boost::function<void(Fetcher&)> OnFetchFailedCallback;

  Fetcher(Ccnx::CcnxWrapperPtr ccnx, ExecutorPtr executor,
          const SegmentCallback& segmentCallback, // callback passed by caller of FetchManager
          const FinishCallback& finishCallback,   // callback passed by caller of FetchManager
          OnFetchCompleteCallback onFetchComplete,
          OnFetchFailedCallback onFetchFailed, // callbacks provided by FetchManager
          const Ccnx::Name& deviceName, const Ccnx::Name& name, int64_t minSeqNo, int64_t maxSeqNo,
          boost::posix_time::time_duration timeout =
            boost::posix_time::seconds(30), // this time is not precise, but sets min bound
                                            // actual time depends on how fast Interests timeout
          const Ccnx::Name& forwardingHint = Ccnx::Name());
  virtual ~Fetcher();

  inline bool
  IsActive() const;

  inline bool
  IsTimedWait() const
  {
    return m_timedwait;
  }

  void
  RestartPipeline();

  void
  SetForwardingHint(const Ccnx::Name& forwardingHint);

  const Ccnx::Name&
  GetForwardingHint() const
  {
    return m_forwardingHint;
  }

  const Ccnx::Name&
  GetName() const
  {
    return m_name;
  }

  const Ccnx::Name&
  GetDeviceName() const
  {
    return m_deviceName;
  }

  double
  GetRetryPause() const
  {
    return m_retryPause;
  }

  void
  SetRetryPause(double pause)
  {
    m_retryPause = pause;
  }

  boost::posix_time::ptime
  GetNextScheduledRetry() const
  {
    return m_nextScheduledRetry;
  }

  void
  SetNextScheduledRetry(boost::posix_time::ptime nextScheduledRetry)
  {
    m_nextScheduledRetry = nextScheduledRetry;
  }

private:
  void
  FillPipeline();

  void
  OnData(uint64_t seqno, const Ccnx::Name& name, Ccnx::PcoPtr data);

  void
  OnData_Execute(uint64_t seqno, Ccnx::Name name, Ccnx::PcoPtr data);

  void
  OnTimeout(uint64_t seqno, const Ccnx::Name& name, const Ccnx::Closure& closure,
            Ccnx::Selectors selectors);

  void
  OnTimeout_Execute(uint64_t seqno, Ccnx::Name name, Ccnx::Closure closure,
                    Ccnx::Selectors selectors);

public:
  boost::intrusive::list_member_hook<> m_managerListHook;

private:
  Ndnx::NdnxWrapperPtr m_ndnx;

  SegmentCallback m_segmentCallback;
  OnFetchCompleteCallback m_onFetchComplete;
  OnFetchFailedCallback m_onFetchFailed;

  FinishCallback m_finishCallback;

  bool m_active;
  bool m_timedwait;

  Ndnx::Name m_name;
  Ndnx::Name m_deviceName;
  Ndnx::Name m_forwardingHint;

  boost::posix_time::time_duration m_maximumNoActivityPeriod;

  int64_t m_minSendSeqNo;
  int64_t m_maxInOrderRecvSeqNo;
  std::set<int64_t> m_outOfOrderRecvSeqNo;
  std::set<int64_t> m_inActivePipeline;

  int64_t m_minSeqNo;
  int64_t m_maxSeqNo;

  uint32_t m_pipeline;
  uint32_t m_activePipeline;
  double m_rto;
  double m_maxRto;
  bool m_slowStart;
  uint32_t m_threshold;
  uint32_t m_roundCount;

  boost::posix_time::ptime m_lastPositiveActivity;

  double m_retryPause; // pause to stop trying to fetch (for fetch-manager)
  boost::posix_time::ptime m_nextScheduledRetry;

  ExecutorPtr m_executor; // to serialize FillPipeline events

  boost::mutex m_seqNoMutex;
  boost::mutex m_rtoMutex;
  boost::mutex m_pipelineMutex;
};

typedef boost::error_info<struct tag_errmsg, std::string> errmsg_info_str;

namespace Error {
struct Fetcher : virtual boost::exception, virtual std::exception
{
};
}

typedef boost::shared_ptr<Fetcher> FetcherPtr;

bool
Fetcher::IsActive() const
{
  return m_active;
}


#endif // FETCHER_H
