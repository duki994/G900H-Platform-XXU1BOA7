/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef CollectionIndexCache_h
#define CollectionIndexCache_h

#include "core/dom/Element.h"

namespace WebCore {

template <typename Collection, typename NodeType>
class CollectionIndexCache {
public:
    CollectionIndexCache();
    ~CollectionIndexCache();

    bool isEmpty(const Collection& collection)
    {
        if (isCachedNodeCountValid())
            return !cachedNodeCount();
        if (cachedNode())
            return false;
        return !nodeAt(collection, 0);
    }
    bool hasExactlyOneNode(const Collection& collection)
    {
        if (isCachedNodeCountValid())
            return cachedNodeCount() == 1;
        if (cachedNode())
            return !cachedNodeIndex() && !nodeAt(collection, 1);
        return nodeAt(collection, 0) && !nodeAt(collection, 1);
    }

    unsigned nodeCount(const Collection&);
    NodeType* nodeAt(const Collection&, unsigned index);

    void invalidate();

private:
    NodeType* nodeBeforeCachedNode(const Collection&, unsigned index, const ContainerNode& root);
    NodeType* nodeAfterCachedNode(const Collection&, unsigned index, const ContainerNode& root);
    unsigned computeNodeCountUpdatingListCache(const Collection&);

    ALWAYS_INLINE NodeType* cachedNode() const { return m_currentNode; }
    ALWAYS_INLINE unsigned cachedNodeIndex() const { ASSERT(cachedNode()); return m_cachedNodeIndex; }
    ALWAYS_INLINE void setCachedNode(NodeType* node, unsigned index)
    {
        ASSERT(node);
        m_currentNode = node;
        m_cachedNodeIndex = index;
    }

    ALWAYS_INLINE bool isCachedNodeCountValid() const { return m_isLengthCacheValid; }
    ALWAYS_INLINE unsigned cachedNodeCount() const { return m_cachedNodeCount; }
    ALWAYS_INLINE void setCachedNodeCount(unsigned length)
    {
        m_cachedNodeCount = length;
        m_isLengthCacheValid = true;
    }

    NodeType* m_currentNode;
    Vector<NodeType*> m_cachedList;
    unsigned m_cachedNodeCount;
    unsigned m_cachedNodeIndex;
    unsigned m_isLengthCacheValid : 1;
    unsigned m_isListValid : 1;
};

template <typename Collection, typename NodeType>
CollectionIndexCache<Collection, NodeType>::CollectionIndexCache()
    : m_currentNode(0)
    , m_cachedNodeCount(0)
    , m_cachedNodeIndex(0)
    , m_isLengthCacheValid(false)
    , m_isListValid(false)
{
}

template <typename Collection, typename NodeType>
CollectionIndexCache<Collection, NodeType>::~CollectionIndexCache()
{
    unsigned externalMemoryAllocated = m_cachedList.capacity() * sizeof(NodeType*);
    if (externalMemoryAllocated)
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-static_cast<int>(externalMemoryAllocated));
}

template <typename Collection, typename NodeType>
void CollectionIndexCache<Collection, NodeType>::invalidate()
{
    m_currentNode = 0;
    m_isLengthCacheValid = false;
    m_isListValid = false;
    m_cachedList.shrink(0);
}

template <typename Collection, typename NodeType>
inline unsigned CollectionIndexCache<Collection, NodeType>::nodeCount(const Collection& collection)
{
    if (isCachedNodeCountValid())
        return cachedNodeCount();

    setCachedNodeCount(computeNodeCountUpdatingListCache(collection));
    ASSERT(isCachedNodeCountValid());
    ASSERT(m_isListValid);

    return cachedNodeCount();
}

template <typename Collection, typename NodeType>
unsigned CollectionIndexCache<Collection, NodeType>::computeNodeCountUpdatingListCache(const Collection& collection)
{
    ASSERT(!m_isListValid);
    ASSERT(m_cachedList.isEmpty());

    ContainerNode& root = collection.rootNode();
    NodeType* firstNode = collection.traverseToFirstElement(root);
    if (!firstNode) {
        m_isListValid = true;
        return 0;
    }

    unsigned oldCapacity = m_cachedList.capacity();
    NodeType* currentNode = firstNode;
    unsigned currentIndex = 0;
    do {
        m_cachedList.append(currentNode);
        currentNode = collection.traverseForwardToOffset(currentIndex + 1, *currentNode, currentIndex, root);
    } while (currentNode);
    m_isListValid = true;

    if (int capacityDifference = m_cachedList.capacity() - oldCapacity)
        v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(capacityDifference * sizeof(NodeType*));

    return m_cachedList.size();
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeAt(const Collection& collection, unsigned index)
{
    if (isCachedNodeCountValid() && index >= cachedNodeCount())
        return 0;

    if (m_isListValid)
        return m_cachedList[index];

    ContainerNode& root = collection.rootNode();
    if (cachedNode()) {
        if (index > cachedNodeIndex())
            return nodeAfterCachedNode(collection, index, root);
        if (index < cachedNodeIndex())
            return nodeBeforeCachedNode(collection, index, root);
        return cachedNode();
    }

    // No valid cache yet, let's find the first matching element.
    ASSERT(!isCachedNodeCountValid());
    NodeType* firstNode = collection.traverseToFirstElement(root);
    if (!firstNode) {
        // The collection is empty.
        setCachedNodeCount(0);
        return 0;
    }
    setCachedNode(firstNode, 0);
    return index ? nodeAfterCachedNode(collection, index, root) : firstNode;
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeBeforeCachedNode(const Collection& collection, unsigned index, const ContainerNode& root)
{
    ASSERT(cachedNode()); // Cache should be valid.
    unsigned currentIndex = cachedNodeIndex();
    ASSERT(currentIndex > index);

    // Determine if we should traverse from the beginning of the collection instead of the cached node.
    bool firstIsCloser = index < currentIndex - index;
    if (UNLIKELY(firstIsCloser || !collection.canTraverseBackward())) {
        NodeType* firstNode = collection.traverseToFirstElement(root);
        ASSERT(firstNode);
        setCachedNode(firstNode, 0);
        return index ? nodeAfterCachedNode(collection, index, root) : firstNode;
    }

    // Backward traversal from the cached node to the requested index.
    NodeType* currentNode = cachedNode();
    ASSERT(collection.canTraverseBackward());
    while ((currentNode = collection.itemBefore(currentNode))) {
        ASSERT(currentIndex);
        --currentIndex;
        if (currentIndex == index) {
            setCachedNode(currentNode, currentIndex);
            return currentNode;
        }
    }
    ASSERT_NOT_REACHED();
    return 0;
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionIndexCache<Collection, NodeType>::nodeAfterCachedNode(const Collection& collection, unsigned index, const ContainerNode& root)
{
    ASSERT(cachedNode()); // Cache should be valid.
    unsigned currentIndex = cachedNodeIndex();
    ASSERT(currentIndex < index);

    // Determine if we should traverse from the end of the collection instead of the cached node.
    bool lastIsCloser = isCachedNodeCountValid() && cachedNodeCount() - index < index - currentIndex;
    if (UNLIKELY(lastIsCloser && collection.canTraverseBackward())) {
        NodeType* lastItem = collection.itemBefore(0);
        ASSERT(lastItem);
        setCachedNode(lastItem, cachedNodeCount() - 1);
        if (index < cachedNodeCount() - 1)
            return nodeBeforeCachedNode(collection, index, root);
        return lastItem;
    }

    // Forward traversal from the cached node to the requested index.
    NodeType* currentNode = collection.traverseForwardToOffset(index, *cachedNode(), currentIndex, root);
    if (!currentNode) {
        // Did not find the node. On plus side, we now know the length.
        setCachedNodeCount(currentIndex + 1);
        return 0;
    }
    setCachedNode(currentNode, currentIndex);
    return currentNode;
}

} // namespace WebCore

#endif // CollectionIndexCache_h
