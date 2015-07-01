/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2013-2015 Regents of the University of California.
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

#ifndef OBJECT_DB_H
#define OBJECT_DB_H

#include <string>
#include <sqlite3.h>
#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <ctime>
#include <vector>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/face.hpp>

namespace ndn {
namespace chronoshare {

class ObjectDb {
public:
  // database will be create in <folder>/<first-pair-of-hash-bytes>/<rest-of-hash>
  ObjectDb(const boost::filesystem::path& folder, const std::string& hash);
  ~ObjectDb();

  void
  saveContentObject(const ndn::Name& deviceName, sqlite3_int64 segment, const ndn::Data& data);

  ndn::BufferPtr
  fetchSegment(const ndn::Name& deviceName, sqlite3_int64 segment);

  // sqlite3_int64
  // getNumberOfSegments (const ndn::Name &deviceName);

  time_t
  secondsSinceLastUse();

  static bool
  DoesExist(const boost::filesystem::path& folder, const ndn::Name& deviceName,
            const std::string& hash);

private:
  void
  willStartSave();

  void
  didStopSave();

private:
  sqlite3* m_db;
  time_t m_lastUsed;
};

typedef shared_ptr<ObjectDb> ObjectDbPtr;

} // chronoshare
} // ndn

#endif // OBJECT_DB_H
