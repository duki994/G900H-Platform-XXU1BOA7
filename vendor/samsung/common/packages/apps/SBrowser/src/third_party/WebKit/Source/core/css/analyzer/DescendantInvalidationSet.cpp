/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/analyzer/DescendantInvalidationSet.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Element.h"

namespace WebCore {

DescendantInvalidationSet::DescendantInvalidationSet()
    : m_allDescendantsMightBeInvalid(false)
{
}

void DescendantInvalidationSet::combine(const DescendantInvalidationSet& other)
{
    m_allDescendantsMightBeInvalid = m_allDescendantsMightBeInvalid || other.m_allDescendantsMightBeInvalid;
    // No longer bother combining data structures, since the whole subtree is deemed invalid.
    if (m_allDescendantsMightBeInvalid)
        return;

    HashSet<AtomicString>::const_iterator end = other.m_classes.end();
    for (HashSet<AtomicString>::const_iterator it = other.m_classes.begin(); it != end; ++it)
        addClass(*it);

    end = other.m_ids.end();
    for (HashSet<AtomicString>::const_iterator it = other.m_ids.begin(); it != end; ++it)
        addId(*it);

    end = other.m_tagNames.end();
    for (HashSet<AtomicString>::const_iterator it = other.m_tagNames.begin(); it != end; ++it)
        addTagName(*it);
}

void DescendantInvalidationSet::addClass(const AtomicString& className)
{
    m_classes.add(className);
}

void DescendantInvalidationSet::addId(const AtomicString& id)
{
    m_ids.add(id);
}

void DescendantInvalidationSet::addTagName(const AtomicString& tagName)
{
    m_tagNames.add(tagName);
}

void DescendantInvalidationSet::getClasses(Vector<AtomicString>& classes)
{
    for (HashSet<AtomicString>::const_iterator it = m_classes.begin(); it != m_classes.end(); ++it)
        classes.append(*it);
}

} // namespace WebCore
