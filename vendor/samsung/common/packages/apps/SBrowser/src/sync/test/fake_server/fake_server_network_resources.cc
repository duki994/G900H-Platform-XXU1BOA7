// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/fake_server_network_resources.h"

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/cancelation_signal.h"
#include "sync/internal_api/public/http_post_provider_factory.h"
#include "sync/internal_api/public/network_time_update_callback.h"
#include "sync/test/fake_server/fake_server.h"
#include "sync/test/fake_server/fake_server_http_post_provider.h"

namespace syncer {

FakeServerNetworkResources::FakeServerNetworkResources(FakeServer* fake_server)
    : fake_server_(fake_server) { }

FakeServerNetworkResources::~FakeServerNetworkResources() {}

scoped_ptr<HttpPostProviderFactory>
    FakeServerNetworkResources::GetHttpPostProviderFactory(
        net::URLRequestContextGetter* baseline_context_getter,
        const NetworkTimeUpdateCallback& network_time_update_callback,
        CancelationSignal* cancelation_signal) {
  return make_scoped_ptr<HttpPostProviderFactory>(
      new FakeServerHttpPostProviderFactory(fake_server_));
}

}  // namespace syncer
