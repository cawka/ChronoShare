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
#include "dispatcher.hpp"
#include "test-common.hpp"

#include <boost/filesystem.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <cassert>
#include <fstream>

INIT_LOGGER("Test.Dispatcher")

using namespace std;
namespace fs = boost::filesystem;

namespace ndn {
namespace chronoshare {
namespace tests {

BOOST_AUTO_TEST_SUITE(TestDispatcher)

void
cleanDir(fs::path dir)
{
  if (fs::exists(dir)) {
    fs::remove_all(dir);
  }
}

void
checkRoots(ndn::ConstBufferPtr root1, ndn::ConstBufferPtr root2)
{
  BOOST_CHECK_EQUAL(digestToString(*root1), digestToString(*root2));
}

BOOST_AUTO_TEST_CASE(DispatcherTest)
{
  INIT_LOGGERS();

  fs::path dir1("./TestDispatcher/test-white-house");
  fs::path dir2("./TestDispatcher/test-black-house");

  string user1 = "/obamaa";
  string user2 = "/romney";

  string folder = "who-is-president";

  boost::asio::io_service io_service;

  shared_ptr<Face> face1 = make_shared<Face>(io_service);
  usleep(100);
  shared_ptr<Face> face2 = make_shared<Face>(io_service);
  usleep(100);

  std::thread ioServiceThread(boost::bind(&boost::asio::io_service::run, &io_service));

  cleanDir(dir1);
  cleanDir(dir2);

  Dispatcher d1(user1, folder, dir1, *face1, false);
  usleep(100);
  Dispatcher d2(user2, folder, dir2, *face2, false);

  usleep(14900000);

  _LOG_DEBUG("checking obama vs romney");
  checkRoots(d1.SyncRoot(), d2.SyncRoot());

  fs::path filename("a_letter_to_romney.txt");
  string words = "I'm the new socialist President. You are not!";

  fs::path abf = dir1 / filename;

  ofstream ofs;
  ofs.open(abf.string().c_str());
  for (int i = 0; i < 5000; i++) {
    ofs << words;
  }
  ofs.close();

  d1.Did_LocalFile_AddOrModify(filename);

  sleep(5);

  fs::path ef = dir2 / filename;
  BOOST_REQUIRE_MESSAGE(fs::exists(ef), user1 << " failed to notify " << user2 << " about "
                                              << filename.string());
  BOOST_CHECK_EQUAL(fs::file_size(abf), fs::file_size(ef));

  ConstBufferPtr fileHash1 = digestFromFile(abf);
  ConstBufferPtr fileHash2 = digestFromFile(ef);

  checkRoots(fileHash1, fileHash2);

  io_service.stop();
  ioServiceThread.join();
  //  cleanDir(dir1);
  //  cleanDir(dir2);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
