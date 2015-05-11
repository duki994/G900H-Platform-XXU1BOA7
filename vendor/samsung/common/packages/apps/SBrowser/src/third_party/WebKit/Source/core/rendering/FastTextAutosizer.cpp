/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/rendering/FastTextAutosizer.h"

#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/rendering/InlineIterator.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderListItem.h"
#include "core/rendering/RenderListMarker.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"

using namespace std;

namespace WebCore {

FastTextAutosizer::FastTextAutosizer(const Document* document)
    : m_document(document)
#ifndef NDEBUG
    , m_renderViewInfoPrepared(false)
#endif
{
}

void FastTextAutosizer::record(const RenderBlock* block)
{
    if (!enabled())
        return;

    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    if (!isFingerprintingCandidate(block))
        return;

    AtomicString fingerprint = computeFingerprint(block);
    if (fingerprint.isNull())
        return;

    m_fingerprintMapper.add(block, fingerprint);
}

void FastTextAutosizer::destroy(const RenderBlock* block)
{
    if (!enabled())
        return;
    ASSERT(!m_blocksThatHaveBegunLayout.contains(block));

    m_fingerprintMapper.remove(block);
}

bool FastTextAutosizer::isLayoutRoot(const RenderBlock* block) const
{
    RenderObject* layoutRoot = m_document->view()->layoutRoot(true);
    if (!layoutRoot)
        layoutRoot = m_document->renderer();
    return block == layoutRoot;
}

void FastTextAutosizer::beginLayout(RenderBlock* block)
{
    ASSERT(enabled());
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(block);
#endif
    ASSERT(m_clusterStack.isEmpty() == isLayoutRoot(block));

    if (isLayoutRoot(block)) {
        prepareRenderViewInfo();
    } else if (block == currentCluster()->m_root) {
        // Ignore beginLayout on the same block twice.
        // This can happen with paginated overflow.
        return;
    }

    if (Cluster* cluster = maybeCreateCluster(block))
        m_clusterStack.append(adoptPtr(cluster));

    if (block->childrenInline())
        inflate(block);
}

void FastTextAutosizer::inflateListItem(RenderListItem* listItem, RenderListMarker* listItemMarker)
{
    if (!enabled())
        return;
    ASSERT(listItem && listItemMarker);
#ifndef NDEBUG
    m_blocksThatHaveBegunLayout.add(listItem);
#endif
    // Force the LI to be inside the DBCAT when computing the multiplier.
    // This guarantees that the DBCAT has entered layout, so we can ask for its width.
    // It also makes sense because the list marker is autosized like a text node.
    float multiplier = clusterMultiplier(currentCluster());

    applyMultiplier(listItem, multiplier);
    applyMultiplier(listItemMarker, multiplier);
}

void FastTextAutosizer::endLayout(RenderBlock* block)
{
    ASSERT(enabled());
    if (isLayoutRoot(block)) {
        m_superclusters.clear();
#ifndef NDEBUG
        m_blocksThatHaveBegunLayout.clear();
#endif
    }

    if (currentCluster()->m_root == block)
        m_clusterStack.removeLast();

    ASSERT(m_clusterStack.isEmpty() == isLayoutRoot(block));
}

void FastTextAutosizer::inflate(RenderBlock* block)
{
    Cluster* cluster = currentCluster();
    float multiplier = 0;
    for (RenderObject* descendant = nextChildSkippingChildrenOfBlocks(block, block); descendant; descendant = nextChildSkippingChildrenOfBlocks(descendant, block)) {
        if (descendant->isText()) {
            // We only calculate this multiplier on-demand to ensure the parent block of this text
            // has entered layout.
            if (!multiplier)
                multiplier = cluster->m_autosize ? clusterMultiplier(cluster) : 1.0f;
            applyMultiplier(descendant, multiplier);
            applyMultiplier(descendant->parent(), multiplier); // Parent handles line spacing.
        }
    }
}

bool FastTextAutosizer::enabled()
{
    if (!m_document->settings() || !m_document->page() || m_document->printing())
        return false;

    return m_document->settings()->textAutosizingEnabled();
}

void FastTextAutosizer::prepareRenderViewInfo()
{
    RenderView* renderView = toRenderView(m_document->renderer());
    bool horizontalWritingMode = isHorizontalWritingMode(renderView->style()->writingMode());

    Frame* mainFrame = m_document->page()->mainFrame();
    IntSize frameSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (frameSize.isEmpty())
        frameSize = mainFrame->view()->unscaledVisibleContentSize(ScrollableArea::IncludeScrollbars);
    m_frameWidth = horizontalWritingMode ? frameSize.width() : frameSize.height();

    IntSize layoutSize = m_document->page()->mainFrame()->view()->layoutSize();
    m_layoutWidth = horizontalWritingMode ? layoutSize.width() : layoutSize.height();

    // Compute the base font scale multiplier based on device and accessibility settings.
    m_baseMultiplier = m_document->settings()->accessibilityFontScaleFactor();
    // If the page has a meta viewport or @viewport, don't apply the device scale adjustment.
    const ViewportDescription& viewportDescription = m_document->page()->mainFrame()->document()->viewportDescription();
    if (!viewportDescription.isSpecifiedByAuthor()) {
        float deviceScaleAdjustment = m_document->settings()->deviceScaleAdjustment();
        m_baseMultiplier *= deviceScaleAdjustment;
    }
#ifndef NDEBUG
    m_renderViewInfoPrepared = true;
#endif
}

bool FastTextAutosizer::isFingerprintingCandidate(const RenderBlock* block)
{
    // FIXME: move the logic out of TextAutosizer.cpp into this class.
    return block->isRenderView()
        || (TextAutosizer::isAutosizingContainer(block)
            && TextAutosizer::isIndependentDescendant(block));
}

bool FastTextAutosizer::clusterWouldHaveEnoughTextToAutosize(const RenderBlock* root)
{
    Cluster hypotheticalCluster(root, true, 0);
    return clusterHasEnoughTextToAutosize(&hypotheticalCluster);
}

bool FastTextAutosizer::clusterHasEnoughTextToAutosize(Cluster* cluster)
{
    const RenderBlock* root = cluster->m_root;

    // TextAreas and user-modifiable areas get a free pass to autosize regardless of text content.
    if (root->isTextArea() || (root->style() && root->style()->userModify() != READ_ONLY))
        return true;

    static const float minLinesOfText = 4;
    if (textLength(cluster) >= root->contentLogicalWidth() * minLinesOfText)
        return true;

    return false;
}

float FastTextAutosizer::textLength(Cluster* cluster)
{
    if (cluster->m_textLength >= 0)
        return cluster->m_textLength;

    float length = 0;
    const RenderBlock* root = cluster->m_root;
    bool measureLocalText = TextAutosizer::containerShouldBeAutosized(root);
    RenderObject* descendant = root->nextInPreOrder(root);
    while (descendant) {
        // FIXME: We should skip over text from descendant clusters (see:
        //        clusters-sufficient-text-except-in-root.html). This currently includes text
        //        from descendant clusters.

        if (measureLocalText && descendant->isText()) {
            // Note: Using text().stripWhiteSpace().length() instead of renderedTextLength() because
            // the lineboxes will not be built until layout. These values can be different.
            length += toRenderText(descendant)->text().stripWhiteSpace().length() * descendant->style()->specifiedFontSize();
        }
        descendant = descendant->nextInPreOrder(root);
    }

    return cluster->m_textLength = length;
}

AtomicString FastTextAutosizer::computeFingerprint(const RenderBlock* block)
{
    // FIXME(crbug.com/322340): Implement a fingerprinting algorithm.
    return nullAtom;
}

FastTextAutosizer::Cluster* FastTextAutosizer::maybeCreateCluster(const RenderBlock* block)
{
    if (!TextAutosizer::isAutosizingContainer(block))
        return 0;

    Cluster* parentCluster = m_clusterStack.isEmpty() ? 0 : currentCluster();
    ASSERT(parentCluster || isLayoutRoot(block));

    // Create clusters to suppress / unsuppress autosizing based on containerShouldBeAutosized.
    bool containerCanAutosize = TextAutosizer::containerShouldBeAutosized(block);
    bool parentClusterCanAutosize = parentCluster && parentCluster->m_autosize;
    bool createClusterThatMightAutosize = isLayoutRoot(block)
        || mightBeWiderOrNarrowerDescendant(block)
        || TextAutosizer::isIndependentDescendant(block);

    // If the container would not alter the m_autosize bit, it doesn't need to be a cluster.
    if (!createClusterThatMightAutosize && containerCanAutosize == parentClusterCanAutosize)
        return 0;

    return new Cluster(block, containerCanAutosize, parentCluster, getSupercluster(block));
}

FastTextAutosizer::Supercluster* FastTextAutosizer::getSupercluster(const RenderBlock* block)
{
    AtomicString fingerprint = m_fingerprintMapper.get(block);
    if (fingerprint.isNull())
        return 0;

    BlockSet* roots = &m_fingerprintMapper.getBlocks(fingerprint);
    if (roots->size() < 2)
        return 0;

    SuperclusterMap::AddResult addResult = m_superclusters.add(fingerprint, PassOwnPtr<Supercluster>());
    if (!addResult.isNewEntry)
        return addResult.storedValue->value.get();

    Supercluster* supercluster = new Supercluster(roots);
    addResult.storedValue->value = adoptPtr(supercluster);
    return supercluster;
}

const RenderBlock* FastTextAutosizer::deepestCommonAncestor(BlockSet& blocks)
{
    // Find the lowest common ancestor of blocks.
    // Note: this could be improved to not be O(b*h) for b blocks and tree height h.
    HashCountedSet<const RenderBlock*> ancestors;
    for (BlockSet::iterator it = blocks.begin(); it != blocks.end(); ++it) {
        for (const RenderBlock* block = (*it); block; block = block->containingBlock()) {
            ancestors.add(block);
            // The first ancestor that has all of the blocks as children wins.
            if (ancestors.count(block) == blocks.size())
                return block;
        }
    }
    ASSERT_NOT_REACHED();
    return 0;
}

float FastTextAutosizer::clusterMultiplier(Cluster* cluster)
{
    ASSERT(m_renderViewInfoPrepared);
    if (!cluster->m_multiplier) {

        if (isLayoutRoot(cluster->m_root)
            || TextAutosizer::isIndependentDescendant(cluster->m_root)
            || isWiderDescendant(cluster)
            || isNarrowerDescendant(cluster)) {

            if (cluster->m_supercluster) {
                cluster->m_multiplier = superclusterMultiplier(cluster->m_supercluster);
            } else if (clusterHasEnoughTextToAutosize(cluster)) {
                cluster->m_multiplier = multiplierFromBlock(deepestBlockContainingAllText(cluster));
            } else {
                cluster->m_multiplier = 1.0f;
            }
        } else {
            cluster->m_multiplier = cluster->m_parent ? clusterMultiplier(cluster->m_parent) : 1.0f;
        }
    }
    ASSERT(cluster->m_multiplier);
    return cluster->m_multiplier;
}

float FastTextAutosizer::superclusterMultiplier(Supercluster* supercluster)
{
    if (!supercluster->m_multiplier) {
        const BlockSet* roots = supercluster->m_roots;
        // Set of the deepest block containing all text (DBCAT) of every cluster.
        BlockSet dbcats;
        for (BlockSet::iterator it = roots->begin(); it != roots->end(); ++it) {
            dbcats.add(deepestBlockContainingAllText(*it));
            supercluster->m_anyClusterHasEnoughText |= clusterWouldHaveEnoughTextToAutosize(*it);
        }
        supercluster->m_multiplier = supercluster->m_anyClusterHasEnoughText
            ? multiplierFromBlock(deepestCommonAncestor(dbcats)) : 1.0f;
    }
    ASSERT(supercluster->m_multiplier);
    return supercluster->m_multiplier;
}

float FastTextAutosizer::multiplierFromBlock(const RenderBlock* block)
{
    // If block->needsLayout() is false, it does not need to be in m_blocksThatHaveBegunLayout.
    // This can happen during layout of a positioned object if the cluster's DBCAT is deeper
    // than the positioned object's containing block, and wasn't marked as needing layout.
    ASSERT(m_blocksThatHaveBegunLayout.contains(block) || !block->needsLayout());

    // Block width, in CSS pixels.
    float textBlockWidth = block->contentLogicalWidth();
    float multiplier = min(textBlockWidth, static_cast<float>(m_layoutWidth)) / m_frameWidth;

    return max(m_baseMultiplier * multiplier, 1.0f);
}

const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(Cluster* cluster)
{
    if (!cluster->m_deepestBlockContainingAllText)
        cluster->m_deepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_root);

    return cluster->m_deepestBlockContainingAllText;
}

// FIXME: Refactor this to look more like FastTextAutosizer::deepestCommonAncestor. This is copied
//        from TextAutosizer::findDeepestBlockContainingAllText.
const RenderBlock* FastTextAutosizer::deepestBlockContainingAllText(const RenderBlock* root)
{
    size_t firstDepth = 0;
    const RenderObject* firstTextLeaf = findTextLeaf(root, firstDepth, First);
    if (!firstTextLeaf)
        return root;

    size_t lastDepth = 0;
    const RenderObject* lastTextLeaf = findTextLeaf(root, lastDepth, Last);
    ASSERT(lastTextLeaf);

    // Equalize the depths if necessary. Only one of the while loops below will get executed.
    const RenderObject* firstNode = firstTextLeaf;
    const RenderObject* lastNode = lastTextLeaf;
    while (firstDepth > lastDepth) {
        firstNode = firstNode->parent();
        --firstDepth;
    }
    while (lastDepth > firstDepth) {
        lastNode = lastNode->parent();
        --lastDepth;
    }

    // Go up from both nodes until the parent is the same. Both pointers will point to the LCA then.
    while (firstNode != lastNode) {
        firstNode = firstNode->parent();
        lastNode = lastNode->parent();
    }

    if (firstNode->isRenderBlock())
        return toRenderBlock(firstNode);

    // containingBlock() should never leave the cluster, since it only skips ancestors when finding
    // the container of position:absolute/fixed blocks, and those cannot exist between a cluster and
    // its text node's lowest common ancestor as isAutosizingCluster would have made them into their
    // own independent cluster.
    const RenderBlock* containingBlock = firstNode->containingBlock();
    ASSERT(containingBlock->isDescendantOf(root));

    return containingBlock;
}

const RenderObject* FastTextAutosizer::findTextLeaf(const RenderObject* parent, size_t& depth, TextLeafSearch firstOrLast)
{
    // List items are treated as text due to the marker.
    // The actual renderer for the marker (RenderListMarker) may not be in the tree yet since it is added during layout.
    if (parent->isListItem())
        return parent;

    if (parent->isEmpty())
        return parent->isText() ? parent : 0;

    ++depth;
    const RenderObject* child = (firstOrLast == First) ? parent->firstChild() : parent->lastChild();
    while (child) {
        // Note: At this point clusters may not have been created for these blocks so we cannot rely
        //       on m_clusters. Instead, we use a best-guess about whether the block will become a cluster.
        if (!TextAutosizer::isAutosizingContainer(child) || !TextAutosizer::isIndependentDescendant(toRenderBlock(child))) {
            const RenderObject* leaf = findTextLeaf(child, depth, firstOrLast);
            if (leaf)
                return leaf;
        }
        child = (firstOrLast == First) ? child->nextSibling() : child->previousSibling();
    }
    --depth;

    return 0;
}

void FastTextAutosizer::applyMultiplier(RenderObject* renderer, float multiplier)
{
    ASSERT(renderer);
    RenderStyle* currentStyle = renderer->style();
    if (currentStyle->textAutosizingMultiplier() == multiplier)
        return;

    // We need to clone the render style to avoid breaking style sharing.
    RefPtr<RenderStyle> style = RenderStyle::clone(currentStyle);
    style->setTextAutosizingMultiplier(multiplier);
    style->setUnique();
    renderer->setStyleInternal(style.release());
}

bool FastTextAutosizer::mightBeWiderOrNarrowerDescendant(const RenderBlock* block)
{
    // FIXME: This heuristic may need to be expanded to other ways a block can be wider or narrower
    //        than its parent containing block.
    return block->style() && block->style()->width().isSpecified();
}

bool FastTextAutosizer::isWiderDescendant(Cluster* cluster)
{
    if (!cluster->m_parent || !mightBeWiderOrNarrowerDescendant(cluster->m_root))
        return true;
    const RenderBlock* parentDeepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_parent);
    ASSERT(m_blocksThatHaveBegunLayout.contains(cluster->m_root));
    ASSERT(m_blocksThatHaveBegunLayout.contains(parentDeepestBlockContainingAllText));

    // Clusters with a root that is wider than the deepestBlockContainingAllText of their parent
    // autosize independently of their parent. Otherwise, they fall back to their parent's multiplier.
    float contentWidth = cluster->m_root->contentLogicalWidth();
    float clusterTextWidth = parentDeepestBlockContainingAllText->contentLogicalWidth();
    return contentWidth > clusterTextWidth;
}

bool FastTextAutosizer::isNarrowerDescendant(Cluster* cluster)
{
    static float narrowWidthDifference = 200;

    if (!cluster->m_parent || !mightBeWiderOrNarrowerDescendant(cluster->m_root))
        return true;

    const RenderBlock* parentDeepestBlockContainingAllText = deepestBlockContainingAllText(cluster->m_parent);
    ASSERT(m_blocksThatHaveBegunLayout.contains(cluster->m_root));
    ASSERT(m_blocksThatHaveBegunLayout.contains(parentDeepestBlockContainingAllText));

    // Clusters with a root that is significantly narrower than the deepestBlockContainingAllText of
    // their parent autosize independently of their parent. Otherwise, they fall back to their
    // parent's multiplier.
    float contentWidth = cluster->m_root->contentLogicalWidth();
    float clusterTextWidth = parentDeepestBlockContainingAllText->contentLogicalWidth();
    float widthDifference = clusterTextWidth - contentWidth;

    return widthDifference > narrowWidthDifference;
}

FastTextAutosizer::Cluster* FastTextAutosizer::currentCluster() const
{
    ASSERT(!m_clusterStack.isEmpty());
    return m_clusterStack.last().get();
}

void FastTextAutosizer::FingerprintMapper::add(const RenderBlock* block, AtomicString fingerprint)
{
    m_fingerprints.set(block, fingerprint);

    ReverseFingerprintMap::AddResult addResult = m_blocksForFingerprint.add(fingerprint, PassOwnPtr<BlockSet>());
    if (addResult.isNewEntry)
        addResult.storedValue->value = adoptPtr(new BlockSet);
    addResult.storedValue->value->add(block);
}

void FastTextAutosizer::FingerprintMapper::remove(const RenderBlock* block)
{
    AtomicString fingerprint = m_fingerprints.take(block);
    if (fingerprint.isNull())
        return;

    ReverseFingerprintMap::iterator blocksIter = m_blocksForFingerprint.find(fingerprint);
    BlockSet& blocks = *blocksIter->value;
    blocks.remove(block);
    if (blocks.isEmpty())
        m_blocksForFingerprint.remove(blocksIter);
}

AtomicString FastTextAutosizer::FingerprintMapper::get(const RenderBlock* block)
{
    return m_fingerprints.get(block);
}

FastTextAutosizer::BlockSet& FastTextAutosizer::FingerprintMapper::getBlocks(AtomicString fingerprint)
{
    return *m_blocksForFingerprint.get(fingerprint);
}

RenderObject* FastTextAutosizer::nextChildSkippingChildrenOfBlocks(const RenderObject* current, const RenderObject* stayWithin)
{
    if (current == stayWithin || !current->isRenderBlock())
        return current->nextInPreOrder(stayWithin);
    return current->nextInPreOrderAfterChildren(stayWithin);
}

} // namespace WebCore
