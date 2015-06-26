/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2015 University of California, Los Angeles
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
#include "sync-core.h"
#include "logging.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <thread>

using namespace std;
using namespace ndn;
using namespace boost;
using namespace boost::filesystem;

INIT_LOGGER("Test.SyncCore");

BOOST_AUTO_TEST_SUITE(SyncCoreTests)

void callback(const SyncStateMsgPtr &msg)
{
  _LOG_DEBUG("Callback I'm called!!!!");
  BOOST_CHECK(msg->state_size() > 0);
  int size = msg->state_size();
  int index = 0;
  while (index < size)
  {
    SyncState state = msg->state(index);
    BOOST_CHECK(state.has_old_seq());
    BOOST_CHECK(state.old_seq() >= 0);
    if (state.seq() != 0)
    {
      BOOST_CHECK(state.old_seq() != state.seq());
    }
    index++;
  }
}

void checkRoots(ndn::ConstBufferPtr root1, ndn::ConstBufferPtr root2)
{
//  std::cout << "I'm checking rootDigest!!" << std::endl;
//  std::cout << "root1 " << DigestComputer::shortDigest(*root1) << std::endl;
//  std::cout << "root2 " << DigestComputer::shortDigest(*root2) << std::endl;
  BOOST_CHECK_EQUAL(DigestComputer::digestToString(*root1), DigestComputer::digestToString(*root2));
//  std::cout << "checking rootDigest Over!!" << std::endl;
}

BOOST_AUTO_TEST_CASE(SyncCoreTest)
{
  INIT_LOGGERS();

  string dir = "./SyncCoreTest";
  // clean the test dir
  path d(dir);
  if (exists(d))
  {
    remove_all(d);
  }

  string dir1 = "./SyncCoreTest/1";
  string dir2 = "./SyncCoreTest/2";
  Name user1("/shuai");
  Name loc1("/locator1");
  Name user2("/loli");
  Name loc2("/locator2");
  Name syncPrefix("/broadcast/arslan");
  boost::shared_ptr<ndn::Face> c1 = boost::make_shared<ndn::Face>();
  boost::shared_ptr<ndn::Face> c2 = boost::make_shared<ndn::Face>();
  SyncLogPtr log1(new SyncLog(dir1, user1));
  SyncLogPtr log2(new SyncLog(dir2, user2));

  SyncCore *core1 = new SyncCore(c1, log1, user1, loc1, syncPrefix, bind(callback, _1));
  usleep(10000);
  SyncCore *core2 = new SyncCore(c2, log2, user2, loc2, syncPrefix, bind(callback, _1));

  sleep(2);

  checkRoots(core1->root(), core2->root());

  _LOG_TRACE ("\n\n\n\n\n\n----------\n");

  core1->updateLocalState(1);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 1);
  BOOST_CHECK_EQUAL(log2->LookupLocator(user1), loc1);

  core1->updateLocalState(5);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core2->seq(user1), 5);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user1), loc1);

  core2->updateLocalState(10);
  usleep(100000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 10);
  BOOST_CHECK_EQUAL(log1->LookupLocator (user2), loc2);

  // simple simultaneous data generation
   _LOG_TRACE ("\n\n\n\n\n\n----------Simultaneous\n");

  core1->updateLocalState(11);
  usleep(100);
  core2->updateLocalState(15);
  usleep(2000000);
  checkRoots(core1->root(), core2->root());
  BOOST_CHECK_EQUAL(core1->seq(user2), 15);
  BOOST_CHECK_EQUAL(core2->seq(user1), 11);

  BOOST_CHECK_EQUAL(log1->LookupLocator (user1), loc1);
  BOOST_CHECK_EQUAL(log1->LookupLocator (user2), loc2);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user1), loc1);
  BOOST_CHECK_EQUAL(log2->LookupLocator (user2), loc2);

  // clean the test dir
  if (exists(d))
  {
    _LOG_DEBUG("Clear ALL");
  }
}

BOOST_AUTO_TEST_SUITE_END()
