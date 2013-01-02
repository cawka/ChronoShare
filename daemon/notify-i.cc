/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013 University of California, Los Angeles
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
 */

#include "notify-i.h"

using namespace std;

void
NotifyI::updateFile (const ::std::string &filename,
                     const ::ChronoshareClient::HashBytes &hash,
                     const ::std::string &atime,
                     const ::std::string &mtime,
                     const ::std::string &ctime,
                     ::Ice::Int mode,
                     const ::Ice::Current&)
{
  cout << "updateFile " << filename << endl;
}


void
NotifyI::deleteFile (const ::std::string &filename,
                     const ::Ice::Current&)
{
  cout << "deleteFile " << filename << endl;
}

