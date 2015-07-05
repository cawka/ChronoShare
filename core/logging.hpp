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

#ifndef LOGGING_H
#define LOGGING_H

#include "core/chronoshare-config.hpp"

#include <log4cxx/logger.h>

namespace ndn {
namespace chronoshare {

#define MEMBER_LOGGER static log4cxx::LoggerPtr staticModuleLogger;

#define INIT_MEMBER_LOGGER(className, name)                                                        \
  log4cxx::LoggerPtr className::staticModuleLogger = log4cxx::Logger::getLogger(name);

#define INIT_LOGGER(name)                                                                          \
  static log4cxx::LoggerPtr staticModuleLogger = log4cxx::Logger::getLogger(name);

#define _LOG_DEBUG(x) LOG4CXX_DEBUG(staticModuleLogger, x);

#define _LOG_TRACE(x) LOG4CXX_TRACE(staticModuleLogger, x);

#define _LOG_FUNCTION(x) LOG4CXX_TRACE(staticModuleLogger, __FUNCTION__ << "(" << x << ")");

#define _LOG_FUNCTION_NOARGS LOG4CXX_TRACE(staticModuleLogger, __FUNCTION__ << "()");

#define _LOG_ERROR(x) LOG4CXX_ERROR(staticModuleLogger, x);

#define _LOG_ERROR_COND(cond, x)                                                                   \
  if (cond) {                                                                                      \
    _LOG_ERROR(x)                                                                                  \
  }

#define _LOG_DEBUG_COND(cond, x)                                                                   \
  if (cond) {                                                                                      \
    _LOG_DEBUG(x)                                                                                  \
  }

void
INIT_LOGGERS();

} // chronoshare
} // ndn

#endif // LOGGING_H
