/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkRecordOpts.h"

#include "SkRecordPattern.h"
#include "SkRecords.h"
#include "SkTDArray.h"

using namespace SkRecords;

void SkRecordOptimize(SkRecord* record) {
    // TODO(mtklein): fuse independent optimizations to reduce number of passes?
    SkRecordNoopSaveRestores(record);
    SkRecordAnnotateCullingPairs(record);
    SkRecordReduceDrawPosTextStrength(record);  // Helpful to run this before BoundDrawPosTextH.
    SkRecordBoundDrawPosTextH(record);
}

// Most of the optimizations in this file are pattern-based.  These are all defined as structs with:
//   - a Pattern typedef
//   - a bool onMatch(SkRceord*, Pattern*, unsigned begin, unsigned end) method,
//     which returns true if it made changes and false if not.

// Run a pattern-based optimization once across the SkRecord, returning true if it made any changes.
// It looks for spans which match Pass::Pattern, and when found calls onMatch() with the pattern,
// record, and [begin,end) span of the commands that matched.
template <typename Pass>
static bool apply(Pass* pass, SkRecord* record) {
    typename Pass::Pattern pattern;
    bool changed = false;
    unsigned begin, end = 0;

    while (pattern.search(record, &begin, &end)) {
        changed |= pass->onMatch(record, &pattern, begin, end);
    }
    return changed;
}

// Turns logical no-op Save-[non-drawing command]*-Restore patterns into actual no-ops.
struct SaveRestoreNooper {
    // Star matches greedily, so we also have to exclude Save and Restore.
    typedef Pattern3<Is<Save>,
                     Star<Not<Or3<Is<Save>,
                                  Is<Restore>,
                                  IsDraw> > >,
                     Is<Restore> >
        Pattern;

    bool onMatch(SkRecord* record, Pattern* pattern, unsigned begin, unsigned end) {
        // If restore doesn't revert both matrix and clip, this isn't safe to noop away.
        if (pattern->first<Save>()->flags != SkCanvas::kMatrixClip_SaveFlag) {
            return false;
        }

        // The entire span between Save and Restore (inclusively) does nothing.
        for (unsigned i = begin; i < end; i++) {
            record->replace<NoOp>(i);
        }
        return true;
    }
};
void SkRecordNoopSaveRestores(SkRecord* record) {
    SaveRestoreNooper pass;
    while (apply(&pass, record));  // Run until it stops changing things.
}

// Replaces DrawPosText with DrawPosTextH when all Y coordinates are equal.
struct StrengthReducer {
    typedef Pattern1<Is<DrawPosText> > Pattern;

    bool onMatch(SkRecord* record, Pattern* pattern, unsigned begin, unsigned end) {
        SkASSERT(end == begin + 1);
        DrawPosText* draw = pattern->first<DrawPosText>();

        const unsigned points = draw->paint.countText(draw->text, draw->byteLength);
        if (points == 0) {
            return false;  // No point (ha!).
        }

        const SkScalar firstY = draw->pos[0].fY;
        for (unsigned i = 1; i < points; i++) {
            if (draw->pos[i].fY != firstY) {
                return false;  // Needs full power of DrawPosText.
            }
        }
        // All ys are the same.  We can replace DrawPosText with DrawPosTextH.

        // draw->pos is points SkPoints, [(x,y),(x,y),(x,y),(x,y), ... ].
        // We're going to squint and look at that as 2*points SkScalars, [x,y,x,y,x,y,x,y, ...].
        // Then we'll rearrange things so all the xs are in order up front, clobbering the ys.
        SK_COMPILE_ASSERT(sizeof(SkPoint) == 2 * sizeof(SkScalar), SquintingIsNotSafe);
        SkScalar* scalars = &draw->pos[0].fX;
        for (unsigned i = 0; i < 2*points; i += 2) {
            scalars[i/2] = scalars[i];
        }

        // Extend lifetime of draw to the end of the loop so we can copy its paint.
        Adopted<DrawPosText> adopted(draw);
        SkNEW_PLACEMENT_ARGS(record->replace<DrawPosTextH>(begin, adopted),
                             DrawPosTextH,
                             (draw->text, draw->byteLength, scalars, firstY, draw->paint));
        return true;
    }
};
void SkRecordReduceDrawPosTextStrength(SkRecord* record) {
    StrengthReducer pass;
    apply(&pass, record);
}

// Tries to replace DrawPosTextH with BoundedDrawPosTextH, which knows conservative upper and lower
// bounds to use with SkCanvas::quickRejectY.
struct TextBounder {
    typedef Pattern1<Is<DrawPosTextH> > Pattern;

    bool onMatch(SkRecord* record, Pattern* pattern, unsigned begin, unsigned end) {
        SkASSERT(end == begin + 1);
        DrawPosTextH* draw = pattern->first<DrawPosTextH>();

        // If we're drawing vertical text, none of the checks we're about to do make any sense.
        // We'll need to call SkPaint::computeFastBounds() later, so bail if that's not possible.
        if (draw->paint.isVerticalText() || !draw->paint.canComputeFastBounds()) {
            return false;
        }

        // Rather than checking the top and bottom font metrics, we guess.  Actually looking up the
        // top and bottom metrics is slow, and this overapproximation should be good enough.
        const SkScalar buffer = draw->paint.getTextSize() * 1.5f;
        SkDEBUGCODE(SkPaint::FontMetrics metrics;)
        SkDEBUGCODE(draw->paint.getFontMetrics(&metrics);)
        SkASSERT(-buffer <= metrics.fTop);
        SkASSERT(+buffer >= metrics.fBottom);

        // Let the paint adjust the text bounds.  We don't care about left and right here, so we use
        // 0 and 1 respectively just so the bounds rectangle isn't empty.
        SkRect bounds;
        bounds.set(0, draw->y - buffer, SK_Scalar1, draw->y + buffer);
        SkRect adjusted = draw->paint.computeFastBounds(bounds, &bounds);

        Adopted<DrawPosTextH> adopted(draw);
        SkNEW_PLACEMENT_ARGS(record->replace<BoundedDrawPosTextH>(begin, adopted),
                             BoundedDrawPosTextH,
                             (&adopted, adjusted.fTop, adjusted.fBottom));
        return true;
    }
};
void SkRecordBoundDrawPosTextH(SkRecord* record) {
    TextBounder pass;
    apply(&pass, record);
}

// Replaces PushCull with PairedPushCull, which lets us skip to the paired PopCull when the canvas
// can quickReject the cull rect.
// There's no efficient way (yet?) to express this one as a pattern, so we write a custom pass.
class CullAnnotator {
public:
    // Do nothing to most ops.
    template <typename T> void operator()(T*) {}

    void operator()(PushCull* push) {
        Pair pair = { fIndex, push };
        fPushStack.push(pair);
    }

    void operator()(PopCull* pop) {
        Pair push = fPushStack.top();
        fPushStack.pop();

        SkASSERT(fIndex > push.index);
        unsigned skip = fIndex - push.index;

        Adopted<PushCull> adopted(push.command);
        SkNEW_PLACEMENT_ARGS(fRecord->replace<PairedPushCull>(push.index, adopted),
                             PairedPushCull, (&adopted, skip));
    }

    void apply(SkRecord* record) {
        for (fRecord = record, fIndex = 0; fIndex < record->count(); fIndex++) {
            fRecord->mutate(fIndex, *this);
        }
    }

private:
    struct Pair {
        unsigned index;
        PushCull* command;
    };

    SkTDArray<Pair> fPushStack;
    SkRecord* fRecord;
    unsigned fIndex;
};
void SkRecordAnnotateCullingPairs(SkRecord* record) {
    CullAnnotator pass;
    pass.apply(record);
}
