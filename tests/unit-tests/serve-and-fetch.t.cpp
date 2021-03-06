/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2017, Regents of the University of California.
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

#include "ccnx-common.hpp"
#include "ccnx-wrapper.hpp"
#include "content-server.hpp"
#include "fetch-manager.hpp"
#include "object-db.hpp"
#include "object-manager.hpp"
#include "scheduler.hpp"
#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_time.hpp>
#include <ctime>
#include <stdio.h>

#include "logging.hpp"

_LOG_INIT(Test.ServerAndFetch);

using namespace Ndnx;
using namespace std;
using namespace boost;
using namespace boost::filesystem;

BOOST_AUTO_TEST_SUITE(TestServeAndFetch)

path root("test-server-and-fetch");
path filePath = root / "random-file";
unsigned char magic = 'm';
int repeat = 1024 * 400;
mutex mut;
condition_variable cond;
bool finished;
int ack;

void
setup()
{
  if (exists(root)) {
    remove_all(root);
  }

  create_directory(root);

  // create file
  FILE* fp = fopen(filePath.string().c_str(), "w");
  for (int i = 0; i < repeat; i++) {
    fwrite(&magic, 1, sizeof(magic), fp);
  }
  fclose(fp);

  ack = 0;
  finished = false;
}

void
teardown()
{
  if (exists(root)) {
    remove_all(root);
  }

  ack = 0;
  finished = false;
}

Name
simpleMap(const Name& deviceName)
{
  return Name("/local");
}

void
segmentCallback(const Name& deviceName, const Name& baseName, uint64_t seq, PcoPtr pco)
{
  ack++;
  Bytes co = pco->content();
  int size = co.size();
  for (int i = 0; i < size; i++) {
    BOOST_CHECK_EQUAL(co[i], magic);
  }
}

void
finishCallback(Name& deviceName, Name& baseName)
{
  BOOST_CHECK_EQUAL(ack, repeat / 1024);
  unique_lock<mutex> lock(mut);
  finished = true;
  cond.notify_one();
}

BOOST_AUTO_TEST_CASE(TestServeAndFetch)
{
  _LOG_DEBUG("Setting up test environment ...");
  setup();

  NdnxWrapperPtr ndnx_serve = make_shared<NdnxWrapper>();
  usleep(1000);
  NdnxWrapperPtr ndnx_fetch = make_shared<NdnxWrapper>();

  Name deviceName("/test/device");
  Name localPrefix("/local");
  Name broadcastPrefix("/broadcast");

  const string APPNAME = "test-chronoshare";

  time_t start = time(NULL);
  _LOG_DEBUG("At time " << start << ", publish local file to database, this is extremely slow ...");
  // publish file to db
  ObjectManager om(ndnx_serve, root, APPNAME);
  tuple<HashPtr, size_t> pub = om.localFileToObjects(filePath, deviceName);
  time_t end = time(NULL);
  _LOG_DEBUG("At time " << end << ", publish finally finished, used " << end - start
                        << " seconds ...");

  ActionLogPtr dummyLog;
  ContentServer server(ndnx_serve, dummyLog, root, deviceName, "pentagon's secrets", APPNAME, 5);
  server.registerPrefix(localPrefix);
  server.registerPrefix(broadcastPrefix);

  FetchManager fm(ccnx_fetch, bind(simpleMap, _1), Name("/local/broadcast"));
  HashPtr hash = pub.get<0>();
  Name baseName = Name("/")(deviceName)(APPNAME)("file")(hash->GetHash(), hash->GetHashBytes());

  fm.Enqueue(deviceName, baseName, bind(segmentCallback, _1, _2, _3, _4),
             bind(finishCallback, _1, _2), 0, pub.get<1>() - 1);

  unique_lock<mutex> lock(mut);
  system_time timeout = get_system_time() + posix_time::milliseconds(5000);
  while (!finished) {
    if (!cond.timed_wait(lock, timeout)) {
      BOOST_FAIL("Fetching has not finished after 5 seconds");
      break;
    }
  }
  ccnx_fetch->shutdown();
  ccnx_serve->shutdown();

  _LOG_DEBUG("Finish");
  usleep(100000);

  teardown();
}

BOOST_AUTO_TEST_SUITE_END()
