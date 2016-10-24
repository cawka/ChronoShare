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
#include "fetch-manager.hpp"

#include "test-common.hpp"

namespace ndn {
namespace chronoshare {
namespace tests {

INIT_LOGGER("Test.FetchManager")

BOOST_AUTO_TEST_SUITE(TestFetchManager)

struct FetcherTestData
{
  std::set<uint64_t> recvData;
  std::set<uint64_t> recvContent;

  std::set<Name> differentNames;
  std::set<Name> segmentNames;

  bool m_done;
  bool m_failed;

  shared_ptr<Face> m_face;
  ndn::KeyChain m_keyChain;

  bool m_hasMissing;

  FetcherTestData(shared_ptr<Face> face)
    : m_done(false)
    , m_failed(false)
    , m_face(face)
    , m_hasMissing(true)
  {
  }

  ~FetcherTestData()
  {
  }

  void
  on_Data(const Interest& interest, const Data& data)
  {
    std::cout << data << std::endl;
  }

  void
  on_Timeout(const Interest& interest)
  {
    std::cout << "Timeout " << interest << std::endl;
  }

  void
  listen()
  {
    m_face->setInterestFilter("/fetchtest", bind(&FetcherTestData::onInterest, this, _1, _2),
                            RegisterPrefixSuccessCallback(),
                            bind(&FetcherTestData::onRegisterFailed, this, _1, _2));
    m_face->processEvents();
  }

  void
  onInterest(const InterestFilter& filter, const Interest& interest)
  {

    std::cout << "<< I: " << interest << std::endl;

    /* publish seqnos:  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, <gap 5>, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
     * <gap 1>, 26 */
    // this will allow us to test our pipeline of 6
    int seq = interest.getName().get(interest.getName().size()-1).toNumber();
    if (m_hasMissing && ((seq > 9 && seq < 15) || (seq == 25))) {
      return;
    }
    ndn::shared_ptr<Data> data = ndn::make_shared<Data>();

    Name baseName("/fetchtest");
    baseName.appendNumber(seq);
    data->setName(baseName);
    data->setFreshnessPeriod(time::seconds(30));
    data->setContent(reinterpret_cast<const unsigned char*>(&seq), sizeof(int));

    m_keyChain.sign(*data);
    m_face->put(*data);
  }

  void
  onRegisterFailed(const Name& prefix, const std::string& reason)
  {
    std::cerr << "ERROR: Failed to register prefix \"" << prefix << "\" in local hub's daemon ("
              << reason << ")" << std::endl;
  }

  void
  onData(const ndn::Name& deviceName, const ndn::Name& basename, uint64_t seqno,
         ndn::shared_ptr<ndn::Data> data)
  {
    _LOG_TRACE("onData: " << seqno << data->getName());

    recvData.insert(seqno);
    differentNames.insert(basename);
    Name name = basename;
    name.appendNumber(seqno);
    segmentNames.insert(name);

    ndn::Block block = data->getContent();

    if (block.value_size() == sizeof(int)) {
      recvContent.insert(*reinterpret_cast<const int*>(block.value()));
    }
  }

  void
  finish(const ndn::Name& deviceName, const ndn::Name& baseName)
  {
  }

  void
  onComplete(Fetcher& fetcher)
  {
    m_done = true;
  }

  void
  onFail(Fetcher& fetcher)
  {
    m_failed = true;
  }
};

BOOST_AUTO_TEST_CASE(TestFetcher)
{
  INIT_LOGGERS();

  shared_ptr<Face> face = make_shared<Face>();
  shared_ptr<Face> face1 = make_shared<Face>(face->getIoService());


  Name baseName("/fetchtest");
  Name deviceName("/device");

  FetcherTestData ftData(face);

  std::thread listeningThread(bind(&FetcherTestData::listen, &ftData));

  //listeningThread.detach();
  /* publish seqnos:  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, <gap 5>, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
   * <gap 1>, 26 */
  // this will allow us to test our pipeline of 6
  /*ndn::KeyChain keyChain;
  for (int i = 0; i < 10; i++) {
    ndn::shared_ptr<Data> data = ndn::make_shared<Data>();
    Name name(baseName);
    name.appendNumber(i);

    data->setName(name);
    data->setFreshnessPeriod(time::seconds(300));
    data->setContent(reinterpret_cast<const unsigned char*>(&i), sizeof(int));
    keyChain.sign(*data);
    face->put(*data);
  }

  for (int i = 15; i < 25; i++) {
    ndn::shared_ptr<Data> data = ndn::make_shared<Data>();
    Name name(baseName);
    name.appendNumber(i);
    data->setName(name);

    data->setFreshnessPeriod(time::seconds(300));
    data->setContent(reinterpret_cast<const unsigned char*>(&i), sizeof(int));
    keyChain.sign(*data);
    face->put(*data);
  }

  int oneMore = 26;

  {
    ndn::shared_ptr<Data> data = ndn::make_shared<Data>();
    Name name(baseName);
    name.appendNumber(oneMore);
    data->setName(name);

    data->setFreshnessPeriod(time::seconds(300));
    data->setContent(reinterpret_cast<const unsigned char*>(&oneMore), sizeof(int));
    keyChain.sign(*data);
    face->put(*data);
  }*/

  ndn::Interest interest(ndn::Name("/fetchtest").appendNumber(0));
  face1->expressInterest(interest, bind(&FetcherTestData::on_Data, &ftData, _1, _2),
                        bind(&FetcherTestData::on_Timeout, &ftData, _1));

  Fetcher fetcher(*face1, bind(&FetcherTestData::onData, &ftData, _1, _2, _3, _4),
                  bind(&FetcherTestData::finish, &ftData, _1, _2),
                  bind(&FetcherTestData::onComplete, &ftData, _1),
                  bind(&FetcherTestData::onFail, &ftData, _1), deviceName, Name("/fetchtest"), 0, 26,
                  boost::posix_time::seconds(5), Name()); // this time is not precise

  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  usleep(7000000);
  BOOST_CHECK_EQUAL(ftData.m_failed, true);
  BOOST_CHECK_EQUAL(ftData.differentNames.size(), 1);
  BOOST_CHECK_EQUAL(ftData.segmentNames.size(), 21);
  BOOST_CHECK_EQUAL(ftData.recvData.size(), 21);
  BOOST_CHECK_EQUAL(ftData.recvContent.size(), 21);

  {
    std::ostringstream recvData;
    for (std::set<uint64_t>::iterator i = ftData.recvData.begin(); i != ftData.recvData.end(); i++)
      recvData << *i << ", ";

    std::ostringstream recvContent;
    for (std::set<uint64_t>::iterator i = ftData.recvContent.begin(); i != ftData.recvContent.end(); i++)
      recvContent << *i << ", ";

    BOOST_CHECK_EQUAL(recvData.str(),
                      "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 26, ");
    BOOST_CHECK_EQUAL(recvData.str(), recvContent.str());
  }

  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  usleep(7000000);
  BOOST_CHECK_EQUAL(ftData.m_failed, true);

  // publishing missing pieces
  ftData.m_hasMissing = false;
  /*for (int i = 0; i < 27; i++) {
    ndn::shared_ptr<Data> data = ndn::make_shared<Data>();
    Name name(baseName);
    name.appendNumber(i);
    data->setName(name);
    data->setFreshnessPeriod(time::seconds(1));
    data->setContent(reinterpret_cast<const unsigned char*>(&i), sizeof(int));
    keyChain.sign(*data);
    face->put(*data);
  }*/
  BOOST_CHECK_EQUAL(fetcher.IsActive(), false);
  fetcher.RestartPipeline();
  BOOST_CHECK_EQUAL(fetcher.IsActive(), true);

  usleep(1000000);
  BOOST_CHECK_EQUAL(ftData.m_done, true);

  {
    std::ostringstream recvData;
    for (std::set<uint64_t>::iterator i = ftData.recvData.begin(); i != ftData.recvData.end(); i++)
      recvData << *i << ", ";

    std::ostringstream recvContent;
    for (std::set<uint64_t>::iterator i = ftData.recvContent.begin(); i != ftData.recvContent.end(); i++)
      recvContent << *i << ", ";

    BOOST_CHECK_EQUAL(recvData.str(), "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, "
                                      "17, 18, 19, 20, 21, 22, 23, 24, 25, 26, ");
    BOOST_CHECK_EQUAL(recvData.str(), recvContent.str());
  }

  face->shutdown();
  face1->shutdown();

  listeningThread.join();
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace chronoshare
} // namespace ndn
