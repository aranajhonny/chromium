// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_
#define REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner);

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~URLRequestContextGetter();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

}  // namespace remoting

#endif  // REMOTING_BASE_URL_REQUEST_CONTEXT_GETTER_H_
