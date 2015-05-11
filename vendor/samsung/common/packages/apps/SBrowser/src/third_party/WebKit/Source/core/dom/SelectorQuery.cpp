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

#include "config.h"
#include "core/dom/SelectorQuery.h"

#include "bindings/v8/ExceptionState.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/SelectorChecker.h"
#include "core/css/SelectorCheckerFastPath.h"
#include "core/css/SiblingTraversalStrategies.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"

namespace WebCore {

struct SingleElementSelectorQueryTrait {
    typedef Element* OutputType;
    static const bool shouldOnlyMatchFirstElement = true;
    ALWAYS_INLINE static void appendElement(OutputType& output, Element& element)
    {
        ASSERT(!output);
        output = &element;
    }
};

struct AllElementsSelectorQueryTrait {
    typedef Vector<RefPtr<Node> > OutputType;
    static const bool shouldOnlyMatchFirstElement = false;
    ALWAYS_INLINE static void appendElement(OutputType& output, Node& element)
    {
        output.append(RefPtr<Node>(element));
    }
};

enum ClassElementListBehavior { AllElements, OnlyRoots };

template <ClassElementListBehavior onlyRoots>
class ClassElementList {
public:
    ClassElementList(ContainerNode& rootNode, const AtomicString& className)
        : m_className(className)
        , m_rootNode(rootNode)
        , m_currentElement(nextInternal(ElementTraversal::firstWithin(rootNode))) { }

    bool isEmpty() const { return !m_currentElement; }

    Element* next()
    {
        Element* current = m_currentElement;
        ASSERT(current);
        if (onlyRoots)
            m_currentElement = nextInternal(ElementTraversal::nextSkippingChildren(*m_currentElement, &m_rootNode));
        else
            m_currentElement = nextInternal(ElementTraversal::next(*m_currentElement, &m_rootNode));
        return current;
    }

private:
    Element* nextInternal(Element* element)
    {
        for (; element; element = ElementTraversal::next(*element, &m_rootNode)) {
            if (element->hasClass() && element->classNames().contains(m_className))
                return element;
        }
        return 0;
    }

    const AtomicString& m_className;
    ContainerNode& m_rootNode;
    Element* m_currentElement;
};

void SelectorDataList::initialize(const CSSSelectorList& selectorList)
{
    ASSERT(m_selectors.isEmpty());

    unsigned selectorCount = 0;
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector))
        selectorCount++;

    m_selectors.reserveInitialCapacity(selectorCount);
    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector))
        m_selectors.uncheckedAppend(SelectorData(*selector, SelectorCheckerFastPath::canUse(*selector)));
}

inline bool SelectorDataList::selectorMatches(const SelectorData& selectorData, Element& element, const ContainerNode& rootNode) const
{
    if (selectorData.isFastCheckable && !element.isSVGElement()) {
        SelectorCheckerFastPath selectorCheckerFastPath(selectorData.selector, element);
        if (!selectorCheckerFastPath.matchesRightmostSelector(SelectorChecker::VisitedMatchDisabled))
            return false;
        return selectorCheckerFastPath.matches();
    }

    SelectorChecker selectorChecker(element.document(), SelectorChecker::QueryingRules);
    SelectorChecker::SelectorCheckingContext selectorCheckingContext(selectorData.selector, &element, SelectorChecker::VisitedMatchDisabled);
    selectorCheckingContext.behaviorAtBoundary = SelectorChecker::StaysWithinTreeScope;
    selectorCheckingContext.scope = !rootNode.isDocumentNode() ? &rootNode : 0;
    return selectorChecker.match(selectorCheckingContext, DOMSiblingTraversalStrategy()) == SelectorChecker::SelectorMatches;
}

bool SelectorDataList::matches(Element& targetElement) const
{
    unsigned selectorCount = m_selectors.size();
    for (unsigned i = 0; i < selectorCount; ++i) {
        if (selectorMatches(m_selectors[i], targetElement, targetElement))
            return true;
    }

    return false;
}

PassRefPtr<NodeList> SelectorDataList::queryAll(ContainerNode& rootNode) const
{
    Vector<RefPtr<Node> > result;
    execute<AllElementsSelectorQueryTrait>(rootNode, result);
    return StaticNodeList::adopt(result);
}

PassRefPtr<Element> SelectorDataList::queryFirst(ContainerNode& rootNode) const
{
    Element* matchedElement = 0;
    execute<SingleElementSelectorQueryTrait>(rootNode, matchedElement);
    return matchedElement;
}

template <typename SelectorQueryTrait>
void SelectorDataList::collectElementsByClassName(ContainerNode& rootNode, const AtomicString& className,  typename SelectorQueryTrait::OutputType& output) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(*element, &rootNode)) {
        if (element->hasClass() && element->classNames().contains(className)) {
            SelectorQueryTrait::appendElement(output, *element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait>
void SelectorDataList::collectElementsByTagName(ContainerNode& rootNode, const QualifiedName& tagName,  typename SelectorQueryTrait::OutputType& output) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(*element, &rootNode)) {
        if (SelectorChecker::tagMatches(*element, tagName)) {
            SelectorQueryTrait::appendElement(output, *element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

inline bool SelectorDataList::canUseFastQuery(const ContainerNode& rootNode) const
{
    return m_selectors.size() == 1 && rootNode.inDocument() && !rootNode.document().inQuirksMode();
}

inline bool ancestorHasClassName(ContainerNode& rootNode, const AtomicString& className)
{
    if (!rootNode.isElementNode())
        return false;

    for (Element* element = &toElement(rootNode); element; element = element->parentElement()) {
        if (element->hasClass() && element->classNames().contains(className))
            return true;
    }
    return false;
}


// If returns true, traversalRoots has the elements that may match the selector query.
//
// If returns false, traversalRoots has the rootNode parameter or descendants of rootNode representing
// the subtree for which we can limit the querySelector traversal.
//
// The travseralRoots may be empty, regardless of the returned bool value, if this method finds that the selectors won't
// match any element.
template <typename SelectorQueryTrait>
void SelectorDataList::findTraverseRootsAndExecute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    // We need to return the matches in document order. To use id lookup while there is possiblity of multiple matches
    // we would need to sort the results. For now, just traverse the document in that case.
    ASSERT(m_selectors.size() == 1);

    bool isRightmostSelector = true;
    bool startFromParent = false;
    Element* singleMatchingElement = 0;

    for (const CSSSelector* selector = &m_selectors[0].selector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id && (rootNode.document().getNumberOfElementsWithId(selector->value(), singleMatchingElement) <= 1)) {
            ContainerNode* adjustedNode = &rootNode;
            if (singleMatchingElement && (isTreeScopeRoot(rootNode) || singleMatchingElement->isDescendantOf(&rootNode)))
                adjustedNode = singleMatchingElement;
            else if (!singleMatchingElement || isRightmostSelector)
                adjustedNode = 0;
            if (isRightmostSelector) {
                executeForTraverseRoot<SelectorQueryTrait>(m_selectors[0], adjustedNode, MatchesTraverseRoots, rootNode, output);
                return;
            }

            if (startFromParent && adjustedNode)
                adjustedNode = adjustedNode->parentNode();

            executeForTraverseRoot<SelectorQueryTrait>(m_selectors[0], adjustedNode, DoesNotMatchTraverseRoots, rootNode, output);
            return;
        }

        // If we have both CSSSelector::Id and CSSSelector::Class at the same time, we should use Id
        // to find traverse root.
        if (!SelectorQueryTrait::shouldOnlyMatchFirstElement && !startFromParent && selector->m_match == CSSSelector::Class) {
            if (isRightmostSelector) {
                ClassElementList<AllElements> traverseRoots(rootNode, selector->value());
                executeForTraverseRoots<SelectorQueryTrait>(m_selectors[0], traverseRoots, MatchesTraverseRoots, rootNode, output);
                return;
            }
            // Since there exists some ancestor element which has the class name, we need to see all children of rootNode.
            if (ancestorHasClassName(rootNode, selector->value())) {
                executeForTraverseRoot<SelectorQueryTrait>(m_selectors[0], &rootNode, DoesNotMatchTraverseRoots, rootNode, output);
                return;
            }

            ClassElementList<OnlyRoots> traverseRoots(rootNode, selector->value());
            executeForTraverseRoots<SelectorQueryTrait>(m_selectors[0], traverseRoots, DoesNotMatchTraverseRoots, rootNode, output);
            return;
        }

        if (selector->relation() == CSSSelector::SubSelector)
            continue;
        isRightmostSelector = false;
        if (selector->relation() == CSSSelector::DirectAdjacent || selector->relation() == CSSSelector::IndirectAdjacent)
            startFromParent = true;
        else
            startFromParent = false;
    }

    executeForTraverseRoot<SelectorQueryTrait>(m_selectors[0], &rootNode, DoesNotMatchTraverseRoots, rootNode, output);
}

template <typename SelectorQueryTrait>
void SelectorDataList::executeForTraverseRoot(const SelectorData& selector, ContainerNode* traverseRoot, MatchTraverseRootState matchTraverseRoot, ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    if (!traverseRoot)
        return;

    if (matchTraverseRoot) {
        if (selectorMatches(selector, toElement(*traverseRoot), rootNode))
            SelectorQueryTrait::appendElement(output, toElement(*traverseRoot));
        return;
    }

    for (Element* element = ElementTraversal::firstWithin(*traverseRoot); element; element = ElementTraversal::next(*element, traverseRoot)) {
        if (selectorMatches(selector, *element, rootNode)) {
            SelectorQueryTrait::appendElement(output, *element);
            if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                return;
        }
    }
}

template <typename SelectorQueryTrait, typename SimpleElementListType>
void SelectorDataList::executeForTraverseRoots(const SelectorData& selector, SimpleElementListType& traverseRoots, MatchTraverseRootState matchTraverseRoots, ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    if (traverseRoots.isEmpty())
        return;

    if (matchTraverseRoots) {
        while (!traverseRoots.isEmpty()) {
            Element& element = *traverseRoots.next();
            if (selectorMatches(selector, element, rootNode)) {
                SelectorQueryTrait::appendElement(output, element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
            }
        }
        return;
    }

    while (!traverseRoots.isEmpty()) {
        Element& traverseRoot = *traverseRoots.next();
        for (Element* element = ElementTraversal::firstWithin(traverseRoot); element; element = ElementTraversal::next(*element, &traverseRoot)) {
            if (selectorMatches(selector, *element, rootNode)) {
                SelectorQueryTrait::appendElement(output, *element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
            }
        }
    }
}

template <typename SelectorQueryTrait>
void SelectorDataList::executeSlow(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    for (Element* element = ElementTraversal::firstWithin(rootNode); element; element = ElementTraversal::next(*element, &rootNode)) {
        for (unsigned i = 0; i < m_selectors.size(); ++i) {
            if (selectorMatches(m_selectors[i], *element, rootNode)) {
                SelectorQueryTrait::appendElement(output, *element);
                if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                    return;
                break;
            }
        }
    }
}

const CSSSelector* SelectorDataList::selectorForIdLookup(const CSSSelector& firstSelector) const
{
    for (const CSSSelector* selector = &firstSelector; selector; selector = selector->tagHistory()) {
        if (selector->m_match == CSSSelector::Id)
            return selector;
        if (selector->relation() != CSSSelector::SubSelector)
            break;
    }
    return 0;
}

template <typename SelectorQueryTrait>
void SelectorDataList::execute(ContainerNode& rootNode, typename SelectorQueryTrait::OutputType& output) const
{
    if (!canUseFastQuery(rootNode)) {
        executeSlow<SelectorQueryTrait>(rootNode, output);
        return;
    }

    ASSERT(m_selectors.size() == 1);

    const SelectorData& selector = m_selectors[0];
    const CSSSelector& firstSelector = selector.selector;

    // Fast path for querySelector*('#id'), querySelector*('tag#id').
    if (const CSSSelector* idSelector = selectorForIdLookup(firstSelector)) {
        const AtomicString& idToMatch = idSelector->value();
        Element* singleMatchingElement = 0;
        if (rootNode.treeScope().getNumberOfElementsWithId(idToMatch, singleMatchingElement) > 1) {
            const Vector<Element*>& elements = rootNode.treeScope().getAllElementsById(idToMatch);
            size_t count = elements.size();
            for (size_t i = 0; i < count; ++i) {
                Element& element = *elements[i];
                if (!(isTreeScopeRoot(rootNode) || element.isDescendantOf(&rootNode)))
                    continue;
                if (selectorMatches(selector, element, rootNode)) {
                    SelectorQueryTrait::appendElement(output, element);
                    if (SelectorQueryTrait::shouldOnlyMatchFirstElement)
                        return;
                }
            }
            return;
        }

        if (!singleMatchingElement || !(isTreeScopeRoot(rootNode) || singleMatchingElement->isDescendantOf(&rootNode)))
            return;
        if (selectorMatches(selector, *singleMatchingElement, rootNode))
            SelectorQueryTrait::appendElement(output, *singleMatchingElement);
        return;
    }

    if (!firstSelector.tagHistory()) {
        // Fast path for querySelector*('.foo'), and querySelector*('div').
        switch (firstSelector.m_match) {
        case CSSSelector::Class:
            collectElementsByClassName<SelectorQueryTrait>(rootNode, firstSelector.value(), output);
            return;
        case CSSSelector::Tag:
            collectElementsByTagName<SelectorQueryTrait>(rootNode, firstSelector.tagQName(), output);
            return;
        default:
            break; // If we need another fast path, add here.
        }
    }

    findTraverseRootsAndExecute<SelectorQueryTrait>(rootNode, output);
}

SelectorQuery::SelectorQuery(const CSSSelectorList& selectorList)
    : m_selectorList(selectorList)
{
    m_selectors.initialize(m_selectorList);
}

bool SelectorQuery::matches(Element& element) const
{
    return m_selectors.matches(element);
}

PassRefPtr<NodeList> SelectorQuery::queryAll(ContainerNode& rootNode) const
{
    RefPtr<NodeList> result = m_cache.queryAllResult(rootNode);
    if (!result) {
        result = m_selectors.queryAll(rootNode);
        m_cache.add(rootNode, result);
    }
    return result.release();
}

PassRefPtr<Element> SelectorQuery::queryFirst(ContainerNode& rootNode) const
{
    RefPtr<Element> result = m_cache.queryFirstResult(rootNode);
    if (!result) {
        result = m_selectors.queryFirst(rootNode);
        m_cache.add(rootNode, result);
    }
    return result.release();
}

SelectorQuery* SelectorQueryCache::add(const AtomicString& selectors, const Document& document, ExceptionState& exceptionState)
{
    HashMap<AtomicString, OwnPtr<SelectorQuery> >::iterator it = m_entries.find(selectors);
    if (it != m_entries.end())
        return it->value.get();

    BisonCSSParser parser(CSSParserContext(document, 0));
    CSSSelectorList selectorList;
    parser.parseSelector(selectors, selectorList);

    if (!selectorList.first()) {
        exceptionState.throwDOMException(SyntaxError, "'" + selectors + "' is not a valid selector.");
        return 0;
    }

    // throw a NamespaceError if the selector includes any namespace prefixes.
    if (selectorList.selectorsNeedNamespaceResolution()) {
        exceptionState.throwDOMException(NamespaceError, "'" + selectors + "' contains namespaces, which are not supported.");
        return 0;
    }

    const unsigned maximumSelectorQueryCacheSize = 256;
    if (m_entries.size() == maximumSelectorQueryCacheSize)
        m_entries.remove(m_entries.begin());

    OwnPtr<SelectorQuery> selectorQuery = adoptPtr(new SelectorQuery(selectorList));
    SelectorQuery* rawSelectorQuery = selectorQuery.get();
    m_entries.add(selectors, selectorQuery.release());
    return rawSelectorQuery;
}

void SelectorQueryCache::invalidate()
{
    m_entries.clear();
}

}
