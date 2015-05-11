// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/AnyCollection.h"

namespace WebCore {

AnyCollection::AnyCollection(ContainerNode* rootNode)
    : HTMLCollection(rootNode, AnyCollectionType, DoesNotOverrideItemAfter)
{
}

AnyCollection::~AnyCollection()
{
}

} // namespace WebCore
