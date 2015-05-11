// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/fake_distiller.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace test {

MockDistillerFactory::MockDistillerFactory() {}
MockDistillerFactory::~MockDistillerFactory() {}

FakeDistiller::FakeDistiller(bool execute_callback)
    : execute_callback_(execute_callback) {
  EXPECT_CALL(*this, Die()).Times(testing::AnyNumber());
}

FakeDistiller::~FakeDistiller() { Die(); }

void FakeDistiller::DistillPage(const GURL& url,
                                const DistillerCallback& callback) {
  url_ = url;
  callback_ = callback;
  if (execute_callback_) {
    scoped_ptr<DistilledArticleProto> proto(new DistilledArticleProto);
    proto->add_pages()->set_url(url_.spec());
    RunDistillerCallback(proto.Pass());
  }
}

void FakeDistiller::RunDistillerCallback(
    scoped_ptr<DistilledArticleProto> proto) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeDistiller::RunDistillerCallbackInternal,
                 base::Unretained(this),
                 base::Passed(&proto)));
}

void FakeDistiller::RunDistillerCallbackInternal(
    scoped_ptr<DistilledArticleProto> proto) {
  EXPECT_FALSE(callback_.is_null());
  callback_.Run(proto.Pass());
  callback_.Reset();
}

}  // namespace test
}  // namespace dom_distiller
