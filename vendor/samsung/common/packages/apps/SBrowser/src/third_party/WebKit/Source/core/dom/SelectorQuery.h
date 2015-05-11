/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SelectorQuery_h
#define SelectorQuery_h

#include "core/css/CSSSelectorList.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class CSSSelector;
class ContainerNode;
class Document;
class Element;
class ExceptionState;
class Node;
class NodeList;
class SimpleNodeList;
class SpaceSplitString;

class SelectorDataList {
public:
    void initialize(const CSSSelectorList&);
    bool matches(Element&) const;
    PassRefPtr<NodeList> queryAll(ContainerNode& rootNode) const;
    PassRefPtr<Element> queryFirst(ContainerNode& rootNode) const;

private:
    struct SelectorData {
        SelectorData(const CSSSelector& selector, bool isFastCheckable) : selector(selector), isFastCheckable(isFastCheckable) { }
        const CSSSelector& selector;
        bool isFastCheckable;
    };

    bool canUseFastQuery(const ContainerNode& rootNode) const;
    bool selectorMatches(const SelectorData&, Element&, const ContainerNode&) const;

    template <typename SelectorQueryTrait>
    void collectElementsByClassName(ContainerNode& rootNode, const AtomicString& className, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait>
    void collectElementsByTagName(ContainerNode& rootNode, const QualifiedName& tagName, typename SelectorQueryTrait::OutputType&) const;

    template <typename SelectorQueryTrait>
    void findTraverseRootsAndExecute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;

    enum MatchTraverseRootState { DoesNotMatchTraverseRoots, MatchesTraverseRoots };
    template <typename SelectorQueryTrait>
    void executeForTraverseRoot(const SelectorData&, ContainerNode* traverseRoot, MatchTraverseRootState, ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait, typename SimpleElementListType>
    void executeForTraverseRoots(const SelectorData&, SimpleElementListType& traverseRoots, MatchTraverseRootState, ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;

    template <typename SelectorQueryTrait>
    void executeSlow(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;
    template <typename SelectorQueryTrait>
    void execute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType&) const;
    const CSSSelector* selectorForIdLookup(const CSSSelector&) const;

    Vector<SelectorData> m_selectors;
};

class SelectorQueryResultCache {
public:
    SelectorQueryResultCache()
        : m_rootNode(0)
        , m_documentVersion(0)
    {
    }

    void add(ContainerNode& rootNode, PassRefPtr<NodeList> result)
    {
        m_rootNode = &rootNode;
        m_documentVersion = rootNode.document().domTreeVersion();
        m_queryAll = result;
        m_queryFirst.clear();
    }

    void add(ContainerNode& rootNode, PassRefPtr<Element> result)
    {
        m_rootNode = &rootNode;
        m_documentVersion = rootNode.document().domTreeVersion();
        m_queryFirst = result;
        m_queryAll.clear();
    }

    PassRefPtr<NodeList> queryAllResult(ContainerNode& rootNode) const
    {
        if (m_rootNode == rootNode && rootNode.document().domTreeVersion() == m_documentVersion)
            return m_queryAll;
        return 0;
    }

    PassRefPtr<Element> queryFirstResult(ContainerNode& rootNode) const
    {
        if (m_rootNode == rootNode && rootNode.document().domTreeVersion() == m_documentVersion)
            return m_queryFirst;
        return 0;
    }

private:
    ContainerNode* m_rootNode;
    uint64_t m_documentVersion;
    RefPtr<NodeList> m_queryAll;
    RefPtr<Element> m_queryFirst;
};

class SelectorQuery {
    WTF_MAKE_NONCOPYABLE(SelectorQuery);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SelectorQuery(const CSSSelectorList&);
    bool matches(Element&) const;
    PassRefPtr<NodeList> queryAll(ContainerNode& rootNode) const;
    PassRefPtr<Element> queryFirst(ContainerNode& rootNode) const;
private:
    SelectorDataList m_selectors;
    CSSSelectorList m_selectorList;
    mutable SelectorQueryResultCache m_cache;
};

class SelectorQueryCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SelectorQuery* add(const AtomicString&, const Document&, ExceptionState&);
    void invalidate();

private:
    HashMap<AtomicString, OwnPtr<SelectorQuery> > m_entries;
};

}

#endif
