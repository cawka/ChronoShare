/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012 University of California, Los Angeles
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

#include "ccnx-wrapper.h"
#include "ccnx-closure.h"
#include "ccnx-name.h"
#include "ccnx-selectors.h"
#include "ccnx-pco.h"
#include <unistd.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>

using namespace Ccnx;
using namespace std;
using namespace boost;

BOOST_AUTO_TEST_SUITE(TestCcnxWrapper)

CcnxWrapperPtr c1;
CcnxWrapperPtr c2;
int g_timeout_counter = 0;
int g_dataCallback_counter = 0;

void publish1(const Name &name)
{
  string content = name.toString();
  c1->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void publish2(const Name &name)
{
  string content = name.toString();
  c2->publishData(name, (const unsigned char*)content.c_str(), content.size(), 5);
}

void dataCallback(const Name &name, Ccnx::PcoPtr pco)
{
  cout << " in data callback" << endl;
  BytesPtr content = pco->contentPtr ();
  string msg(reinterpret_cast<const char *> (head (*content)), content->size());
  g_dataCallback_counter ++;
  BOOST_CHECK_EQUAL(name, msg);
}

void encapCallback(const Name &name, Ccnx::PcoPtr pco)
{
  cout << " in encap data callback" << endl;
  BOOST_CHECK(!c1->verify(pco));
  cout << "++++++++++++++++++ Outer content couldn't be verified, which is expected." << endl;
  PcoPtr npco = make_shared<ParsedContentObject> (*(pco->contentPtr()));
  g_dataCallback_counter ++;
  BOOST_CHECK(npco);
  BOOST_CHECK(c1->verify(npco));
}

void
timeout(const Name &name, const Closure &closure, Selectors selectors)
{
  g_timeout_counter ++;
}

void
setup()
{
  if (!c1)
  {
    c1 = make_shared<CcnxWrapper> ();
  }
  if (!c2)
  {
    c2 = make_shared<CcnxWrapper> ();
  }
}

void
teardown()
{
  if (c1)
  {
    c1.reset();
  }
  if (c2)
  {
    c2.reset();
  }
}


BOOST_AUTO_TEST_CASE (BlaCcnxWrapperTest)
{
  INIT_LOGGERS ();
  
  setup();
  Name prefix1("/c1");
  Name prefix2("/c2");

  c1->setInterestFilter(prefix1, bind(publish1, _1));
  usleep(100000);
  c2->setInterestFilter(prefix2, bind(publish2, _1));

  Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1, _2, _3));

  c1->sendInterest(Name("/c2/hi"), closure);
  usleep(100000);
  c2->sendInterest(Name("/c1/hi"), closure);
  sleep(1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 2);
  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;

  teardown();
}

BOOST_AUTO_TEST_CASE (CcnxWrapperSelector)
{

  setup();
  Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1, _2, _3));

  Selectors selectors;
  selectors
    .interestLifetime(1)
    .childSelector(Selectors::RIGHT);

  string n1 = "/random/01";
  c1->sendInterest(Name(n1), closure, selectors);
  sleep(2);
  c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 0);

  string n2 = "/random/02";
  selectors.interestLifetime(2);
  c1->sendInterest(Name(n2), closure, selectors);
  sleep(1);
  c2->publishData(Name(n2), (const unsigned char *)n2.c_str(), n2.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_timeout_counter, 1);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);

  // reset
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;

  teardown();

}

void
reexpress(const Name &name, const Closure &closure, Selectors selectors)
{
  g_timeout_counter ++;
  c1->sendInterest(name, closure, selectors);
}

BOOST_AUTO_TEST_CASE (TestTimeout)
{
  setup();
  g_dataCallback_counter = 0;
  g_timeout_counter = 0;
  Closure closure (bind(dataCallback, _1, _2), bind(reexpress, _1, _2, _3));

  Selectors selectors;
  selectors.interestLifetime(1);

  string n1 = "/random/04";
  c1->sendInterest(Name(n1), closure, selectors);
  usleep(3500000);
  c2->publishData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);
  BOOST_CHECK_EQUAL(g_timeout_counter, 3);
  teardown();
}

BOOST_AUTO_TEST_CASE (TestUnsigned)
{
  setup();
  string n1 = "/xxxxxx/unsigned/01";
  Closure closure (bind(dataCallback, _1, _2), bind(timeout, _1, _2, _3));

  g_dataCallback_counter = 0;
  c1->sendInterest(Name(n1), closure);
  usleep(100000);
  c2->publishUnsignedData(Name(n1), (const unsigned char *)n1.c_str(), n1.size(), 1);
  usleep(100000);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 1);

  string n2 = "/xxxxxx/signed/01";
  Bytes content = c1->createContentObject(Name(n1), (const unsigned char *)n2.c_str(), n2.size(), 1);
  c1->publishUnsignedData(Name(n2), head(content), content.size(), 1);
  Closure encapClosure(bind(encapCallback, _1, _2), bind(timeout, _1, _2, _3));
  c2->sendInterest(Name(n2), encapClosure);
  usleep(4000000);
  BOOST_CHECK_EQUAL(g_dataCallback_counter, 2);
  teardown();
}


 /*
 BOOST_AUTO_TEST_CASE (CcnxWrapperUnsigningTest)
 {
   setup();
   Bytes data;
   data.resize(1024);
   for (int i = 0; i < 1024; i++)
   {
     data[i] = 'm';
   }

   Name name("/unsigningtest");

   posix_time::ptime start = posix_time::second_clock::local_time();
   for (uint64_t i = 0; i < 100000; i++)
   {
     Name n = name;
     n.appendComp(i);
     c1->publishUnsignedData(n, data, 10);
   }
   posix_time::ptime end = posix_time::second_clock::local_time();

   posix_time::time_duration duration = end - start;

   cout << "Publishing 100000 1K size content objects costs " <<duration.total_milliseconds() << " milliseconds" << endl;
   cout << "Average time to publish one content object is " << (double) duration.total_milliseconds() / 100000.0 << " milliseconds" << endl;
    teardown();
 }
 */


BOOST_AUTO_TEST_SUITE_END()
