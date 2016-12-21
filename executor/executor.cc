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
 */

#include "executor.h"
#include "logging.h"

INIT_MEMBER_LOGGER(Executor,"Executor")

using namespace std;
using namespace boost;

Executor::Executor(int poolSize)
  : m_needStop(true)
  , m_poolSize(poolSize)
{
}

Executor::~Executor()
{
  _LOG_DEBUG("Enter destructor");
  shutdown();
  _LOG_DEBUG("Exit destructor");
}

void
Executor::start()
{
  if (m_needStop)
    {
      m_needStop = false;
      for (int i = 0; i < m_poolSize; i++)
        {
          m_group.create_thread(bind(&Executor::run, this));
        }
    }
}

void
Executor::shutdown()
{
  if (!m_needStop)
    {
      m_needStop = true;
      _LOG_DEBUG("Iterrupting all");
      m_group.interrupt_all();
      _LOG_DEBUG("Join all");
      m_group.join_all();
    }
}


void
Executor::execute(const Job &job)
{
  _LOG_DEBUG("Add to job queue");

  Lock lock(m_mutex);
  bool queueWasEmpty = m_queue.empty();
  m_queue.push_back(job);

  // notify working threads if the queue was empty
  if (queueWasEmpty)
  {
    m_cond.notify_one();
  }
}

int
Executor::poolSize()
{
  return m_group.size();
}

int
Executor::jobQueueSize()
{
  Lock lock(m_mutex);
  return m_queue.size();
}

void
Executor::run()
{
  _LOG_DEBUG("Start thread");

  while (!m_needStop)
  {
    Job job = waitForJob();

    _LOG_DEBUG(">>> enter job");
    job(); // even if job is "null", nothing bad will happen
    _LOG_DEBUG("<<< exit job");
  }

  _LOG_DEBUG("Executor thread finished");
}

Executor::Job
Executor::waitForJob()
{
  Lock lock(m_mutex);

  // wait until job queue is not empty
  while (m_queue.empty())
  {
    _LOG_DEBUG("Unlocking mutex for wait");
    m_cond.wait(lock);
    _LOG_DEBUG("Re-locking mutex after wait");
  }

  _LOG_DEBUG("Got signal on condition");

  Job job;
  if (!m_queue.empty()) // this is not always guaranteed, especially after interruption from destructor
    {
      job = m_queue.front();
      m_queue.pop_front();
    }
  return job;
}
