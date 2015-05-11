// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnyCollection_h
#define AnyCollection_h

#include "core/html/HTMLCollection.h"

namespace WebCore {

class AnyCollection FINAL : public HTMLCollection {
public:
    static PassRefPtr<AnyCollection> create(ContainerNode* rootNode, CollectionType type)
    {
        ASSERT_UNUSED(type, type == AnyCollectionType);
        return adoptRef(new AnyCollection(rootNode));
    }

    virtual ~AnyCollection();

    ALWAYS_INLINE bool elementMatches(const Element&) const { return true; }

private:
    explicit AnyCollection(ContainerNode* rootNode);
};

} // namespace WebCore

#endif // AnyCollection_h
