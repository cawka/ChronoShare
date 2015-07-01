//
// request_handler.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_handler.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <boost/lexical_cast.hpp>
#include "mime_types.hpp"
#include "reply.hpp"
#include "request.hpp"
#include <QIODevice>
#include <QFile>
#include <QDataStream>
#include <QString>
#include "logging.hpp"

INIT_LOGGER("HttpServer")

namespace http {
namespace server {

request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root.c_str())
{
}

void
request_handler::handle_request(const request& req, reply& rep)
{
  // Decode url to path.
  std::string request_path;
  if (!url_decode(req.uri, request_path)) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // Request path must be absolute and not contain "..".
  if (request_path.empty() || request_path[0] != '/'
      || request_path.find("..") != std::string::npos) {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // If path ends in slash (i.e. is a directory) then add "index.html".
  if (request_path[request_path.size() - 1] == '/') {
    request_path += "index.html";
  }

  // Determine the file extension.
  std::size_t last_slash_pos = request_path.find_last_of("/");
  std::size_t last_dot_pos = request_path.find_last_of(".");
  std::string extension;
  if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos) {
    extension = request_path.substr(last_dot_pos + 1);
  }

  // Open the file to send back.
  // The following is a hack to make the server understands Qt's
  // resource system, so that the html resources can be managed using Qt's
  // resource system (e.g. no need to worry about the location of html)
  // in Mac OS, it will be inside the bundle, in Linux, perhaps somewhere
  // in /usr/local/share
  QString full_path = doc_root_.absolutePath() + QString(request_path.c_str());
  QFile file(full_path);
  if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
    rep = reply::stock_reply(reply::not_found);
    return;
  }

  _LOG_DEBUG("Serving file: " << request_path);
  // Fill out the reply to be sent to the client.
  rep.status = reply::ok;
  char buf[512];
  QDataStream in(&file);
  while (true) {
    int bytes = in.readRawData(buf, sizeof(buf));
    if (bytes > 0) {
      rep.content.append(buf, bytes);
    }
    else {
      break;
    }
  }
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = boost::lexical_cast<std::string>(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);
}

bool
request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '%') {
      if (i + 3 <= in.size()) {
        int value = 0;
        std::istringstream is(in.substr(i + 1, 2));
        if (is >> std::hex >> value) {
          out += static_cast<char>(value);
          i += 2;
        }
        else {
          return false;
        }
      }
      else {
        return false;
      }
    }
    else if (in[i] == '+') {
      out += ' ';
    }
    else {
      out += in[i];
    }
  }
  return true;
}

} // namespace server
} // namespace http
