// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include "base/path_service.h"
#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/devtools_http_handler.h"
#include "net/socket/tcp_listen_socket.h"

RemoteDebuggingServer::RemoteDebuggingServer(
    chrome::HostDesktopType host_desktop_type,
    const std::string& ip,
    int port) {
  base::FilePath output_dir;
  if (!port) {
    // The client requested an ephemeral port. Must write the selected
    // port to a well-known location in the profile directory to
    // bootstrap the connection process.
    bool result = PathService::Get(chrome::DIR_USER_DATA, &output_dir);
    DCHECK(result);
  }

  devtools_http_handler_ = content::DevToolsHttpHandler::Start(
      new net::TCPListenSocketFactory(ip, port),
      "",
      new BrowserListTabContentsProvider(host_desktop_type),
      output_dir);
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  devtools_http_handler_->Stop();
}
