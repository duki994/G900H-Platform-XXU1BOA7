// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.ViewConfiguration;

import org.chromium.base.CommandLine;
import org.chromium.base.TraceEvent;
// SAMSUNG CHANGE : Change to GestureDetector of Framework view >>
import org.chromium.content.browser.third_party.GestureDetector;
import org.chromium.content.browser.third_party.GestureDetector.OnDoubleTapListener;
import org.chromium.content.browser.third_party.GestureDetector.OnGestureListener;
//import android.view.GestureDetector;
//import android.view.GestureDetector.OnGestureListener;
//import android.view.GestureDetector.OnDoubleTapListener;
// SAMSUNG CHANGE : Change to GestureDetector of Framework view <<
import org.chromium.content.common.ContentSwitches;
import android.util.FloatMath;
// SAMSUNG CHANGE : Prediction scroll >>
import android.view.VelocityTracker;
import android.os.Bundle;
// SAMSUNG CHANGE : Prediction scroll <<

/**
 * This class handles all MotionEvent handling done in ContentViewCore including the gesture
 * recognition. It sends all related native calls through the interface MotionEventDelegate.
 */
 public class ContentViewGestureHandler {

    private static final String TAG = "ContentViewGestureHandler";
    /**
     * Used for FLING_START x velocity
     */
    static final String VELOCITY_X = "Velocity X";
    /**
     * Used for FLING_START y velocity
     */
    static final String VELOCITY_Y = "Velocity Y";
    /**
     * Used for SCROLL_BY x distance (scroll offset of update)
     */
    static final String DISTANCE_X = "Distance X";
    /**
     * Used for SCROLL_BY y distance (scroll offset of update)
     */
    static final String DISTANCE_Y = "Distance Y";
    /**
     * Used for SCROLL_START delta X hint (movement triggering scroll)
     */
    static final String DELTA_HINT_X = "Delta Hint X";
    /**
     * Used for SCROLL_START delta Y hint (movement triggering scroll)
     */
    static final String DELTA_HINT_Y = "Delta Hint Y";
    /**
     * Used in SINGLE_TAP_CONFIRMED to check whether ShowPress has been called before.
     */
    static final String SHOW_PRESS = "ShowPress";
    /**
     * Used for PINCH_BY delta
     */
    static final String DELTA = "Delta";

    private final Bundle mExtraParamBundleSingleTap;
    private final Bundle mExtraParamBundleFling;
    private final Bundle mExtraParamBundleScroll;
    private final Bundle mExtraParamBundleScrollStart;
    private final Bundle mExtraParamBundleDoubleTapDragZoom;
    private final Bundle mExtraParamBundlePinchBy;
    protected GestureDetector mGestureDetector;
    private OnGestureListener mListener;
    private OnDoubleTapListener mDoubleTapListener;
    protected ScaleGestureDetector mMultiTouchDetector;
    protected ScaleGestureListener mMultiTouchListener;
    protected MotionEvent mCurrentDownEvent;
    private final MotionEventDelegate mMotionEventDelegate;
    private ScrollEventDelegate mScrollEventDelegate;

    // Remember whether onShowPress() is called. If it is not, in onSingleTapConfirmed()
    // we will first show the press state, then trigger the click.
    private boolean mShowPressIsCalled;

    // Whether a sent TAP_DOWN event has yet to be accompanied by a corresponding
    // SINGLE_TAP_UP, SINGLE_TAP_CONFIRMED, TAP_CANCEL or DOUBLE_TAP.
    private boolean mNeedsTapEndingEvent;

    // This flag is used for ignoring the remaining touch events, i.e., All the events until the
    // next ACTION_DOWN. This is automatically set to false on the next ACTION_DOWN.
    protected boolean mIgnoreRemainingTouchEvents;

    // TODO(klobag): this is to avoid a bug in GestureDetector. With multi-touch,
    // mAlwaysInTapRegion is not reset. So when the last finger is up, onSingleTapUp()
    // will be mistakenly fired.
    private boolean mIgnoreSingleTap;

    // True from right before we send the first scroll event until the last finger is raised.
    protected boolean mTouchScrolling;

    // Used to remove the touch slop from the initial scroll event in a scroll gesture.
    private boolean mSeenFirstScrollEvent;

    private boolean mPinchInProgress = false;

    private static final int DOUBLE_TAP_TIMEOUT = ViewConfiguration.getDoubleTapTimeout();

    // Indicate current double tap mode state.
    private int mDoubleTapMode = DOUBLE_TAP_MODE_NONE;

    // x, y coordinates for an Anchor on double tap drag zoom.
    private float mDoubleTapDragZoomAnchorX;
    private float mDoubleTapDragZoomAnchorY;

    // On double tap this will store the y coordinates of the touch.
    private float mDoubleTapY;

    // Double tap drag zoom sensitive (speed).
    private static final float DOUBLE_TAP_DRAG_ZOOM_SPEED = 0.005f;

    // Used to track the last rawX/Y coordinates for moves.  This gives absolute scroll distance.
    // Useful for full screen tracking.
    protected float mLastRawX = 0;
    protected float mLastRawY = 0;

    // Cache of square of the scaled touch slop so we don't have to calculate it on every touch.
    private int mScaledTouchSlopSquare;

    // Object that keeps track of and updates scroll snapping behavior.
    private final SnapScrollController mSnapScrollController;

    // Used to track the accumulated scroll error over time. This is used to remove the
    // rounding error we introduced by passing integers to webkit.
    private float mAccumulatedScrollErrorX = 0;
    private float mAccumulatedScrollErrorY = 0;

    // The page's viewport and scale sometimes allow us to disable double tap gesture detection,
    // according to the logic in ContentViewCore.onRenderCoordinatesUpdated().
    private boolean mShouldDisableDoubleTap;

    // Keeps track of the last long press event, if we end up opening a context menu, we would need
    // to potentially use the event to send TAP_CANCEL to remove ::active styling
    protected MotionEvent mLastLongPressEvent;

    // Whether the click delay should always be disabled by sending clicks for double tap gestures.
    private final boolean mDisableClickDelay;

    private final float mPxToDp;

    // To block fling event comming after zooming
    private boolean mIsPointerDownCalled = false;
    private double mPinchEndTime = -1;

    static final int DOUBLE_TAP_MODE_NONE = 0;
    static final int DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS = 1;
    static final int DOUBLE_TAP_MODE_DRAG_ZOOM = 2;
    static final int DOUBLE_TAP_MODE_DISABLED = 3;

    //SAMSUNG CHANGE: block scroll during zoom
    private boolean mBlockScrollEvent = false;

    //SAMSUNG CHANGES -- Multifinger scroll >>
    private float mFocusX;
    private float mFocusY;
    private float mAccumulatedSpan;
    private long mStartTimePannigCheck;
    private static final long PANNING_BLOCKING_TIME = 50;
    private static final int SPAN_CAPACITY = 3;
    private FocusMovementQueue mFocusMovementQueue;
    private FocusMovementQueue mSpanQueue;
    //	private int zoomIgnored=0;
    private long mPrevPinchTime=0;
    private int mSpanCount=0;
    //SAMSUNG CHANGES -- Multifinger scroll <<

    // SAMSUNG CHANGE : Prediction scroll >>
    //private static final boolean PREDICTION_SCROLL_SYS = "1".equals(SystemProperties.get("debug.browser.predictionscroll"));
    //private static final boolean PREDICTION_SCROLL = "2".equals(SystemProperties.get("debug.browser.predictionscroll"));
    private static final boolean PREDICTION_SCROLL_SYS = "1".equals(System.getProperty("debug.browser.predictionscroll"));
    //private static final boolean PREDICTION_SCROLL = "2".equals(System.getProperty("debug.browser.predictionscroll"));
    private boolean mPredictionScroll = false;
    private VelocityTracker mVelocityTracker = null;
    private int mDisplayWidthPixels;
    private int mDisplayHeightPixels;

    // SAMSUNG CHANGE : Prediction scroll <<
    private int lastPredictionXDelta=0, lastPredictionYDelta=0;
    private int NANOS_TO_MS = 1000000;
    private float predictionTime = 7f;//10f;
    private float accelerationRatio = 1.0f;
    // SAMSUNG CHANGE : Prediction scroll <<

    static final int SCROLL_COMPENSATION_SPEED = 1;
    static final int MIN_SCROLL_COMPENSATION_VALUE = 1;
    static final int MAX_SCROLL_COMPENSATION_VALUE = 15;
    private static final boolean ENABLE_COMPENSATION_SCROLL = !"1".equals(System.getProperty("debug.browser.compensationscroll"));
    private static final boolean ENABLE_ENHANCED_COMPENSATION = true;

    private static class FocusMovementQueue {
        private static final int QUEUE_CAPACITY = 5;
        private float[] mQueue;
        private float mSum;
        private int mSize;
        private int mIndex;

        FocusMovementQueue() {
            mQueue = new float[QUEUE_CAPACITY];
            mSize = 0;
            mSum = 0;
            mIndex = 0;
        }

        private void clear() {
            mSize = 0;
            mSum = 0;
            mIndex = 0;
            for (int i = 0; i < QUEUE_CAPACITY; ++i) {
                mQueue[i] = 0;
            }
        }

        private void add(float focusDelta) {
            mSum += focusDelta;

	    if (mSize < QUEUE_CAPACITY) {  // fill up the queue.
		mSize++;
	    } 
	    else {  // circulate the queue.
		mSum -= mQueue[mIndex];
	    }
	    mQueue[mIndex] = focusDelta;
	    mIndex = (mIndex + 1) % QUEUE_CAPACITY;
	}
        
        private float getSum() {
            return mSum;
        }
    }


    /**
     * This is an interface to handle MotionEvent related communication with the native side also
     * access some ContentView specific parameters.
     */
    public interface MotionEventDelegate {
        /**
         * Signal the start of gesture detection for the provided {@link MotionEvent}.
         * @param event The {@link MotionEvent} being fed to the gesture detectors.
         */
        public void onTouchEventHandlingBegin(MotionEvent event);

        /**
         * Signal that all gestures for the current {@link MotionEvent} have been dispatched.
         */
        public void onTouchEventHandlingEnd();

        /**
         * Forward a generated event to the client.  This will normally be wrapped by
         * calls to {@link #onTouchEventHandlingBegin(MotionEvent)} and
         * {@link #onTouchEventHandlingEnd()}, unless the gesture is generated from
         * a touch timeout, e.g., LONG_PRESS.
         * @param type The type of the gesture event.
         * @param timeMs The time the gesture event occurred at.
         * @param x The x location for the gesture event.
         * @param y The y location for the gesture event.
         * @param extraParams A bundle that holds specific extra parameters for certain gestures.
         *                    This is read-only and should not be modified in this function.
         * Refer to gesture type definition for more information.
         * @return Whether the gesture was forwarded successfully.
         */
        boolean onGestureEventCreated(int type, long timeMs, int x, int y, Bundle extraParams);
    }

    public interface ScrollEventDelegate {
        public void onScrollStarted();
    }

    private class DefaultScrollEventDelegate implements ScrollEventDelegate {
        public void onScrollStarted() {}
    }

    protected void setScrollEventDelegate(ScrollEventDelegate delegate) {
        mScrollEventDelegate = delegate;
    }

    protected ContentViewGestureHandler(Context context, MotionEventDelegate delegate) {
        mExtraParamBundleSingleTap = new Bundle();
        mExtraParamBundleFling = new Bundle();
        mExtraParamBundleScroll = new Bundle();
        mExtraParamBundleScrollStart = new Bundle();
        mExtraParamBundleDoubleTapDragZoom = new Bundle();
        mExtraParamBundlePinchBy = new Bundle();

        mMotionEventDelegate = delegate;
        mScrollEventDelegate = new DefaultScrollEventDelegate();
        mSnapScrollController = new SnapScrollController(context);
        mPxToDp = 1.0f / context.getResources().getDisplayMetrics().density;

        mDisableClickDelay = CommandLine.isInitialized() &&
                CommandLine.getInstance().hasSwitch(ContentSwitches.DISABLE_CLICK_DELAY);
        initGestureDetectors(context);

	mDisplayWidthPixels = context.getResources().getDisplayMetrics().widthPixels;
	mDisplayHeightPixels = context.getResources().getDisplayMetrics().heightPixels;
		

    }

    private int mCompensationY = 0;
    private float mLastDistanceY = 0;
    private int mCompensationYInThisRound = 0;

    private void initGestureDetectors(final Context context) {
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        mScaledTouchSlopSquare = scaledTouchSlop * scaledTouchSlop;
        try {
            TraceEvent.begin();
            GestureDetector.SimpleOnGestureListener listener =
                new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onDown(MotionEvent e) {
                        mShowPressIsCalled = false;
                        mIgnoreSingleTap = false;
                        mTouchScrolling = false;
                        mSeenFirstScrollEvent = false;
                        mLastRawX = e.getRawX();
                        mLastRawY = e.getRawY();
                        mAccumulatedScrollErrorX = 0;
                        mAccumulatedScrollErrorY = 0;
                        mLastLongPressEvent = null;
                        mNeedsTapEndingEvent = false;
                        mSnapScrollController.resetSnapScrollMode();
                        if (sendMotionEventAsGesture(GestureEventType.TAP_DOWN, e, null)) {
                            mNeedsTapEndingEvent = true;
                        }
                        mCompensationY = 0;	
                        mLastDistanceY = 0; 
                        mCompensationYInThisRound = 0;

                        if (mPredictionScroll)
                        {
                            lastPredictionXDelta = 0;
                            lastPredictionYDelta = 0;
                        	
                        }
                        // Return true to indicate that we want to handle touch
                        return true;
                    }

                    @Override
                    public boolean onScroll(MotionEvent e1, MotionEvent e2,
                            float rawDistanceX, float rawDistanceY) {
                        try {
                            TraceEvent.begin();
                            //SAMSUNG CHANGE: block scroll during zoom
                            if (mBlockScrollEvent) {
                                Log.i(TAG, "[SBRCHECK_LU] onScroll() return by mBlockScrollEvent");
                                return true;
                            }

                            //SAMSUNG CHANGE : blockScrollAFfterZooming by skvin.lee >>
                            double curTime = System.currentTimeMillis();
                            if((curTime - mPinchEndTime) < 100.0f
                              && (curTime - mPinchEndTime) > 0.0f){
                                Log.i(TAG, "[SBRCHECK_LU] onScroll() return by blockScrollAFfterZooming");
                                return true;
                            }
                            //SAMSUNG CHANGE : blockScrollAFfterZooming by skvin.lee <<

                            assert e1.getEventTime() <= e2.getEventTime();
                            // SAMSUNG CHANGE : Prediction scroll >>
                            float predictionXDelta = 0;
                            float predictionYDelta = 0;
                            if (mPredictionScroll)
                            {
                            	// mVelocityTracker is created/destoryed in onTouchEvent
                                mVelocityTracker.addMovement(e2);
                            	
                                float curXVelocity;
                                float curYVelocity;		

                                mVelocityTracker.computeCurrentVelocity(1);
                                curXVelocity = -(mVelocityTracker.getXVelocity());
                                curYVelocity = -(mVelocityTracker.getYVelocity());

                                predictionXDelta = curXVelocity * predictionTime;
                                predictionYDelta = curYVelocity * predictionTime;
                            }

                            
                            // SAMSUNG CHANGE : Prediction scroll >>

                            float distanceX = rawDistanceX;
                            float distanceY = rawDistanceY;
                            if (!mSeenFirstScrollEvent) {
                                // Remove the touch slop region from the first scroll event to avoid a
                                // jump.
                                mSeenFirstScrollEvent = true;
                                double distance = Math.sqrt(
                                        distanceX * distanceX + distanceY * distanceY);
                                double epsilon = 1e-3;
                                if (distance > epsilon) {
                                    double ratio = Math.max(0, distance - scaledTouchSlop) / distance;
                                    mCompensationY = (int)(distanceY - distanceY * ratio);
                                    mCompensationYInThisRound = mCompensationY;
                                    distanceX *= ratio;
                                    distanceY *= ratio;
                                }
                            }
                            mSnapScrollController.updateSnapScrollMode(distanceX, distanceY);
                            if (mSnapScrollController.isSnappingScrolls()) {
                                if (mSnapScrollController.isSnapHorizontal()) {
                                    distanceY = 0;
                                } else {
                                    distanceX = 0;
                                }
                            }
							
                            if(ENABLE_COMPENSATION_SCROLL && distanceY != 0 && mCompensationYInThisRound != 0){
                                int multipleValueY = Math.min(Math.max(MIN_SCROLL_COMPENSATION_VALUE, Math.abs((int)distanceY/10)), MAX_SCROLL_COMPENSATION_VALUE);
                                if(Math.abs(distanceY) < 3){
                                    multipleValueY = 0;
                                }
                                int compensationValue = SCROLL_COMPENSATION_SPEED * multipleValueY;
                                if(mLastDistanceY * distanceY < 0){
                                    if(mCompensationY == 0){
                                        if(ENABLE_ENHANCED_COMPENSATION){
                                            if(Math.abs(mCompensationYInThisRound) > scaledTouchSlop){
                                                mCompensationYInThisRound = -mCompensationYInThisRound;
                                            }else{
                                                mCompensationYInThisRound = (int)(-mCompensationYInThisRound * 2);
                                            }
                                        }else{
                                            mCompensationYInThisRound = -mCompensationYInThisRound;
                                        }
                                        mCompensationY = mCompensationYInThisRound;
                                    }else{
                                        mCompensationY = 0;
                                        mCompensationYInThisRound = 0;
                                    }
                                }else if(mCompensationY != 0){
                                    if(compensationValue > Math.abs(mCompensationY)){
                                        distanceY += mCompensationY;
                                        mCompensationY = 0;
                                    }else{
                                        distanceY += (distanceY > 0 ? compensationValue : -compensationValue);
                                        mCompensationY += (distanceY < 0 ? compensationValue : -compensationValue);
                                    }
                                }
                                mLastDistanceY = distanceY;
                            }

                            mLastRawX = e2.getRawX();
                            mLastRawY = e2.getRawY();
                            if (!mTouchScrolling) {
                                sendTapCancelIfNecessary(e1);
                                // Note that scroll start hints are in distance traveled, where
                                // scroll deltas are in the opposite direction.
                                mScrollEventDelegate.onScrollStarted();
                                mExtraParamBundleScrollStart.putInt(DELTA_HINT_X, (int) -rawDistanceX);
                                mExtraParamBundleScrollStart.putInt(DELTA_HINT_Y, (int) -rawDistanceY);
                                assert mExtraParamBundleScrollStart.size() == 2;
                                if (sendGesture(GestureEventType.SCROLL_START, e2.getEventTime(),
                                            (int) e1.getX(), (int) e1.getY(),
                                            mExtraParamBundleScrollStart)) {
                                    mTouchScrolling = true;
                                }
                            }

                            // distanceX and distanceY is the scrolling offset since last onScroll.
                            // Because we are passing integers to webkit, this could introduce
                            // rounding errors. The rounding errors will accumulate overtime.
                            // To solve this, we should be adding back the rounding errors each time
                            // when we calculate the new offset.
                            int x = (int) e2.getX();
                            int y = (int) e2.getY();
                            /*if ((PREDICTION_SCROLL_SYS || PREDICTION_SCROLL) 
			         && mVelocityTracker != null) {
				 x = (int) (e2.getX() + predictionXDelta);
				 y = (int) (e2.getY() + predictionYDelta);
			    }*/

                            int dx = (int) (distanceX + mAccumulatedScrollErrorX);
                            int dy = (int) (distanceY + mAccumulatedScrollErrorY);
                            mAccumulatedScrollErrorX = distanceX + mAccumulatedScrollErrorX - dx;
                            mAccumulatedScrollErrorY = distanceY + mAccumulatedScrollErrorY - dy;


                            if(mPredictionScroll)
                            {
                                if ( ( predictionXDelta > 0 && lastPredictionXDelta < 0 ) || ( predictionXDelta < 0 && lastPredictionXDelta > 0 ) )
                                    lastPredictionXDelta = -lastPredictionXDelta;
                                if ( ( predictionYDelta > 0 && lastPredictionYDelta < 0 ) || ( predictionYDelta < 0 && lastPredictionYDelta > 0 ) )
                                    lastPredictionYDelta = -lastPredictionYDelta;


                                if ( Math.abs( dx + (int)predictionXDelta ) <  Math.abs( lastPredictionXDelta ) )
                                    lastPredictionXDelta = 0;
                                if ( Math.abs( dy + (int)predictionYDelta ) <  Math.abs( lastPredictionYDelta ) )
                                    lastPredictionYDelta = 0;

                                if (distanceX == 0 // snap to Vertical
                                    || Math.abs(predictionXDelta) > mDisplayWidthPixels/3) { // sometimes large prediction value
                                    predictionXDelta = 0;
                                    lastPredictionXDelta = 0;
                                }

                                if (distanceY == 0 // snap to Horizontal
                                    || Math.abs(predictionYDelta) > mDisplayHeightPixels/3) { // sometimes large prediction value
                                    predictionYDelta = 0;
                                    lastPredictionYDelta = 0;
                                }

                                int orgdx = dx; 
                                int orgdy = dy;

                                dx = dx + (int)predictionXDelta - lastPredictionXDelta;
                                dy = dy + (int)predictionYDelta - lastPredictionYDelta;
    
                                // to prevent inverse calibration problem
                                if( Math.abs(orgdx) > Math.abs(dx) )
                                {
                                    dx = orgdx;
                                    //predictionXDelta = 0;
                                }
                                if( Math.abs(orgdy) > Math.abs(dy) )
                                {
                                    dy = orgdy;
                                    //predictionYDelta = 0;      
                                } 
                                // to prevent inverse calibration problem
    
                                lastPredictionXDelta = (int)predictionXDelta;
                                lastPredictionYDelta = (int)predictionYDelta;
                            }
								

                            mExtraParamBundleScroll.putInt(DISTANCE_X, dx);
                            mExtraParamBundleScroll.putInt(DISTANCE_Y, dy);
                            assert mExtraParamBundleScroll.size() == 2;

                            if ((dx | dy) != 0) {
                                sendGesture(GestureEventType.SCROLL_BY,
                                        e2.getEventTime(), x, y, mExtraParamBundleScroll);
                            }

                            return true;
                        }finally {
                            TraceEvent.end();
                        }
                    }

                    @Override
                    public boolean onFling(MotionEvent e1, MotionEvent e2,
                            float velocityX, float velocityY) {
                        assert e1.getEventTime() <= e2.getEventTime();

                        if (mIsPointerDownCalled) {
                            mIsPointerDownCalled = false;
                            return false;
                        }

                        if (mSnapScrollController.isSnappingScrolls()) {
                            if (mSnapScrollController.isSnapHorizontal()) {
                                velocityY = 0;
                            } else {
                                velocityX = 0;
                            }
                        }

                        fling(e2.getEventTime(), (int) e1.getX(0), (int) e1.getY(0),
                                        (int) velocityX, (int) velocityY);
                        return true;
                    }

                    @Override
                    public void onShowPress(MotionEvent e) {
                        mShowPressIsCalled = true;
                        sendMotionEventAsGesture(GestureEventType.SHOW_PRESS, e, null);
                    }

                    @Override
                    public boolean onSingleTapUp(MotionEvent e) {
                        if (isDistanceBetweenDownAndUpTooLong(e.getRawX(), e.getRawY())) {
                            sendTapCancelIfNecessary(e);
                            mIgnoreSingleTap = true;
                            return true;
                        }
                        // This is a hack to address the issue where user hovers
                        // over a link for longer than DOUBLE_TAP_TIMEOUT, then
                        // onSingleTapConfirmed() is not triggered. But we still
                        // want to trigger the tap event at UP. So we override
                        // onSingleTapUp() in this case. This assumes singleTapUp
                        // gets always called before singleTapConfirmed.
                        if (!mIgnoreSingleTap) {
                            if (e.getEventTime() - e.getDownTime() > DOUBLE_TAP_TIMEOUT) {
                                if (sendTapEndingEventAsGesture(
                                            GestureEventType.SINGLE_TAP_UP, e, null)) {
                                    mIgnoreSingleTap = true;
                                }
                                return true;
                            } else if (isDoubleTapDisabled() || mDisableClickDelay) {
                                // If double tap has been disabled, there is no need to wait
                                // for the double tap timeout.
                                return onSingleTapConfirmed(e);
                            } else {
                                // Notify Blink about this tapUp event anyway,
                                // when none of the above conditions applied.
                                sendMotionEventAsGesture(
                                        GestureEventType.SINGLE_TAP_UNCONFIRMED, e, null);
                            }
                        }

                        return triggerLongTapIfNeeded(e);
                    }

                    @Override
                    public boolean onSingleTapConfirmed(MotionEvent e) {
                        // Long taps in the edges of the screen have their events delayed by
                        // ContentViewHolder for tab swipe operations. As a consequence of the delay
                        // this method might be called after receiving the up event.
                        // These corner cases should be ignored.
                        if (mIgnoreSingleTap) return true;

                        mExtraParamBundleSingleTap.putBoolean(SHOW_PRESS, mShowPressIsCalled);
                        assert mExtraParamBundleSingleTap.size() == 1;
                        if (sendTapEndingEventAsGesture(GestureEventType.SINGLE_TAP_CONFIRMED, e,
                                mExtraParamBundleSingleTap)) {
                            mIgnoreSingleTap = true;
                        }
                        return true;
                    }

                    @Override
                    public boolean onDoubleTapEvent(MotionEvent e) {
                        switch (e.getActionMasked()) {
                            case MotionEvent.ACTION_DOWN:
                                // Note that this will be called before the corresponding |onDown()|
                                // of the same ACTION_DOWN event.  Thus, the preceding TAP_DOWN
                                // should be cancelled prior to sending a new one (in |onDown()|).
                                sendTapCancelIfNecessary(e);
// @Samsung: commented out to disable zoom in/out with one finger
// after a double tap. Should be uncommented after approving with
// UEx team, that it is a feature, not a bug.
// Bug: http://107.108.218.239/bugzilla/show_bug.cgi?id=1383
/***************************************************************************************************
                                mDoubleTapDragZoomAnchorX = e.getX();
                                mDoubleTapDragZoomAnchorY = e.getY();

                                mDoubleTapMode = DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS;

                                // If a long-press fires during a double-tap, the GestureDetector
                                // will stop feeding MotionEvents to |onDoubleTapEvent()|,
                                // preventing double-tap drag zoom. Long press detection will be
                                // re-enabled on the next ACTION_DOWN.
                                mGestureDetector.setIsLongpressEnabled(false);
                                break;
                            case MotionEvent.ACTION_MOVE:
                                if (mDoubleTapMode
                                        == DOUBLE_TAP_MODE_DRAG_DETECTION_IN_PROGRESS) {
                                    float distanceX = mDoubleTapDragZoomAnchorX - e.getX();
                                    float distanceY = mDoubleTapDragZoomAnchorY - e.getY();

                                    // Begin double tap drag zoom mode if the move distance is
                                    // further than the threshold.
                                    if (isDistanceGreaterThanTouchSlop(distanceX, distanceY)) {
                                        sendTapCancelIfNecessary(e);
                                        mExtraParamBundleScrollStart.putInt(DELTA_HINT_X,
                                                (int) -distanceX);
                                        mExtraParamBundleScrollStart.putInt(DELTA_HINT_Y,
                                                (int) -distanceY);
                                        assert mExtraParamBundleScrollStart.size() == 2;
                                        sendGesture(GestureEventType.SCROLL_START, e.getEventTime(),
                                                (int) e.getX(), (int) e.getY(),
                                                mExtraParamBundleScrollStart);
                                        pinchBegin(e.getEventTime(),
                                                Math.round(mDoubleTapDragZoomAnchorX),
                                                Math.round(mDoubleTapDragZoomAnchorY));
                                        mDoubleTapMode = DOUBLE_TAP_MODE_DRAG_ZOOM;
                                    }
                                } else if (mDoubleTapMode == DOUBLE_TAP_MODE_DRAG_ZOOM) {
                                    assert mExtraParamBundleDoubleTapDragZoom.isEmpty();
                                    sendGesture(GestureEventType.SCROLL_BY, e.getEventTime(),
                                            (int) e.getX(), (int) e.getY(),
                                            mExtraParamBundleDoubleTapDragZoom);

                                    float dy = mDoubleTapY - e.getY();
                                    pinchBy(e.getEventTime(),
                                            Math.round(mDoubleTapDragZoomAnchorX),
                                            Math.round(mDoubleTapDragZoomAnchorY),
                                            (float) Math.pow(dy > 0 ?
                                                    1.0f - DOUBLE_TAP_DRAG_ZOOM_SPEED :
                                                    1.0f + DOUBLE_TAP_DRAG_ZOOM_SPEED,
                                                    Math.abs(dy * mPxToDp)));
                                }
                                break;
                            case MotionEvent.ACTION_UP:
***************************************************************************************************/
                                if (mDoubleTapMode != DOUBLE_TAP_MODE_DRAG_ZOOM) {
                                    // Normal double tap gesture.
                                    sendTapEndingEventAsGesture(
                                            GestureEventType.DOUBLE_TAP, e, null);
                                }
                                endDoubleTapDragIfNecessary(e);
                                break;
                            case MotionEvent.ACTION_CANCEL:
                                sendTapCancelIfNecessary(e);
                                endDoubleTapDragIfNecessary(e);
                                break;
                            default:
                                break;
                        }
                        mDoubleTapY = e.getY();
                        return true;
                    }
                    // SAMSUNG CHANGE : Change to GestureDetector of Framework view >>
                    /*@Override
                    public void onLongPress(MotionEvent e) {
                        assert !isDoubleTapActive();
                        if (isScaleGestureDetectionInProgress()) return;
                        setIgnoreSingleTap(true);
                        mLastLongPressEvent = e;
                        sendMotionEventAsGesture(GestureEventType.LONG_PRESS, e, null);
                        return;
                    }*/
                    public boolean onLongPress(MotionEvent e) {
                        assert !isDoubleTapActive();
                        if (isScaleGestureDetectionInProgress()) return false;
                        setIgnoreSingleTap(true);
                        mLastLongPressEvent = e;
                        sendMotionEventAsGesture(GestureEventType.LONG_PRESS, e, null);
                        // Returning true puts the GestureDetector in "longpress" mode, disabling
                        // further scrolling.  This is undesirable, as it is quite common for a
                        // longpress gesture to fire on content that won't trigger a context menu.
                        return false;
                    }
                    // SAMSUNG CHANGE : Change to GestureDetector of Framework view <<

                    /**
                     * This method inspects the distance between where the user started touching
                     * the surface, and where she released. If the points are too far apart, we
                     * should assume that the web page has consumed the scroll-events in-between,
                     * and as such, this should not be considered a single-tap.
                     *
                     * We use the Android frameworks notion of how far a touch can wander before
                     * we think the user is scrolling.
                     *
                     * @param x the new x coordinate
                     * @param y the new y coordinate
                     * @return true if the distance is too long to be considered a single tap
                     */
                    private boolean isDistanceBetweenDownAndUpTooLong(float x, float y) {
                        return isDistanceGreaterThanTouchSlop(mLastRawX - x, mLastRawY - y);
                    }
                };
            mListener = listener;
            mDoubleTapListener = listener;
            mGestureDetector = new GestureDetector(context, listener);

            mMultiTouchListener = new ScaleGestureListener();
            mMultiTouchDetector = new ScaleGestureDetector(context, mMultiTouchListener);

            //SAMSUNG CHANGES -- Multifinger scroll >>
            mFocusMovementQueue = new FocusMovementQueue();
            //SAMSUNG CHANGES -- Multifinger scroll <<

            // ScaleGestureDetector's "QuickScale" feature was introduced in KitKat.
            // As ContentViewGestureHandler already implements this feature,
            // explicitly disable it to prevent double-handling of the gesture.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                disableQuickScale(mMultiTouchDetector);
            }
        } finally {
            TraceEvent.end();
        }
    }

    protected class ScaleGestureListener implements ScaleGestureDetector.OnScaleGestureListener {
        // Completely silence scaling events. Used in WebView when zoom support
        // is turned off.
        private boolean mPermanentlyIgnoreDetectorEvents = false;

        // Whether any pinch zoom event has been sent to native.
        protected boolean mPinchEventSent;

        // ScaleGestureDetector previous to 4.2.2 failed to record the touch event time
        // (b/7626515), so we record it manually for synthesizing pinch gestures.
        private long mCurrentEventTime;

        void setCurrentEventTime(long currentEventTime) {
            mCurrentEventTime = currentEventTime;
        }

        protected long getEventTime(ScaleGestureDetector detector) {
            // Workaround for b/7626515, fixed in 4.2.2.
            assert mCurrentEventTime != 0;
            assert detector.getEventTime() == 0 || detector.getEventTime() == mCurrentEventTime;
            return mCurrentEventTime;
        }

        protected boolean getPermanentlyIgnoreDetectorEvents() {
            return mPermanentlyIgnoreDetectorEvents;
        }

        public void setPermanentlyIgnoreDetectorEvents(boolean value) {
            // Note that returning false from onScaleBegin / onScale makes the
            // gesture detector not to emit further scaling notifications
            // related to this gesture. Thus, if detector events are enabled in
            // the middle of the gesture, we don't need to do anything.
            mPermanentlyIgnoreDetectorEvents = value;
        }

        //SAMSUNG CHANGES -- Multifinger scroll >>

        // If the user moves the fingers but keeps the same distance between them,
        // we should do panning only.
    	public boolean isPanningOnly(ScaleGestureDetector detector) {
	        float prevFocusX = mFocusX;
	        float prevFocusY = mFocusY;
	        mFocusX = detector.getFocusX();
	        mFocusY = detector.getFocusY();
      	    long currentTime = SystemClock.uptimeMillis();

			//For blocking panning check in multi-touch initially
			if(mStartTimePannigCheck == 0) {
				mStartTimePannigCheck = SystemClock.uptimeMillis();
			}
			if((currentTime < mStartTimePannigCheck + PANNING_BLOCKING_TIME))	{
				Log.i("PTRMFS", "ZM.isPanningOnly() currentTime < PANNING_BLOCKING_TIME return false");
				return false;
      		}

		    float focusDelta = (prevFocusX == 0 && prevFocusY == 0) ? 0 :
		        FloatMath.sqrt((mFocusX - prevFocusX) * (mFocusX - prevFocusX)+ (mFocusY - prevFocusY) * (mFocusY - prevFocusY));
		    mFocusMovementQueue.add(focusDelta);

			float deltaCurrSpan = detector.getCurrentSpan() - detector.getPreviousSpan();
		    float deltaSpan = Math.abs(detector.getCurrentSpan() - detector.getPreviousSpan()) + mAccumulatedSpan;
		    final boolean result = mFocusMovementQueue.getSum() > deltaSpan;

		    if (result) {
			
				mSpanCount++;
				if (mSpanCount % SPAN_CAPACITY == 0)
		        {	
					mAccumulatedSpan = 0;
					mSpanCount = 0;
				}
				else
				{
					mAccumulatedSpan += deltaSpan;
					// mSpanQueue.add(deltaSpan);
				}
		    } else {
				mAccumulatedSpan = 0;
				mSpanCount = 0;
		   // mSpanQueue.clear();
        	}
		    // Log.i("PTRMFS","ZM.isPanningOnly() move="+mFocusMovementQueue.getSum()+" Accum="+mAccumulatedSpan+"  Curr Span="+deltaCurrSpan+"  Delta Span="+deltaSpan+"  spanCount="+mSpanCount);
		    // Log.i("PTRMFS","ZM.isPanningOnly() result="+result);
        	return result;
    	}
//SAMSUNG CHANGES -- Multifinger scroll <<

        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            if (mPermanentlyIgnoreDetectorEvents) return false;
            mPinchEventSent = false;
            mBlockScrollEvent = true;
            setIgnoreSingleTap(true);

    		//SAMSUNG CHANGES -- Multifinger scroll >>
    		mStartTimePannigCheck = 0;					
    		mFocusMovementQueue.clear();
			// mSpanQueue.clear();
			mSpanCount = 0;
    		//SAMSUNG CHANGES -- Multifinger scroll <<

            return true;
        }

        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            if (!mPinchEventSent) return;
            pinchEnd(getEventTime(detector));
            mPinchEventSent = false;

    		//SAMSUNG CHANGES -- Multifinger scroll >>
    		mStartTimePannigCheck=0;
    		//SAMSUNG CHANGES -- Multifinger scroll <<
        }

        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (mPermanentlyIgnoreDetectorEvents) return false;

    		//SAMSUNG CHANGES -- Multifinger scroll >>		
    		if (isPanningOnly(detector)) {
        			mFocusMovementQueue.clear();
        			blockScrollEvents(false);
    				return true;
    		}
    		else {
    			mFocusMovementQueue.clear();
    		}			
    		blockScrollEvents(true);			
    		//SAMSUNG CHANGES -- Multifinger scroll <<

            // It is possible that pinchBegin() was never called when we reach here.
            // This happens when webkit handles the 2nd touch down event. That causes
            // ContentView to ignore the onScaleBegin() call. And if webkit does not
            // handle the touch move events afterwards, we will face a situation
            // that pinchBy() is called without any pinchBegin().
            // To solve this problem, we call pinchBegin() here if it is never called.
            if (!mPinchEventSent) {
                pinchBegin(getEventTime(detector),
                        (int) detector.getFocusX(), (int) detector.getFocusY());
                mPinchEventSent = true;
            }
            pinchBy(getEventTime(detector), (int) detector.getFocusX(), (int) detector.getFocusY(),
                    detector.getScaleFactor());
            return true;
        }
    };

    @TargetApi(Build.VERSION_CODES.KITKAT)
    private static void disableQuickScale(ScaleGestureDetector scaleGestureDetector) {
       if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;
       scaleGestureDetector.setQuickScaleEnabled(false);
     }

    /**
     * Fling the ContentView from the current position.
     * @param x Fling touch starting position
     * @param y Fling touch starting position
     * @param velocityX Initial velocity of the fling (X) measured in pixels per second.
     * @param velocityY Initial velocity of the fling (Y) measured in pixels per second.
     */
    void fling(long timeMs, int x, int y, int velocityX, int velocityY) {
        if (velocityX == 0 && velocityY == 0) {
            endTouchScrollIfNecessary(timeMs, true);
            return;
        }

        if (!mTouchScrolling) {
            // The native side needs a SCROLL_BEGIN before FLING_START
            // to send the fling to the correct target. Send if it has not sent.
            // The distance traveled in one second is a reasonable scroll start hint.
            mExtraParamBundleScrollStart.putInt(DELTA_HINT_X, velocityX);
            mExtraParamBundleScrollStart.putInt(DELTA_HINT_Y, velocityY);
            assert mExtraParamBundleScrollStart.size() == 2;
            sendGesture(GestureEventType.SCROLL_START, timeMs, x, y, mExtraParamBundleScrollStart);
        }
        endTouchScrollIfNecessary(timeMs, false);

        mExtraParamBundleFling.putInt(VELOCITY_X, velocityX);
        mExtraParamBundleFling.putInt(VELOCITY_Y, velocityY);
        assert mExtraParamBundleFling.size() == 2;
        sendGesture(GestureEventType.FLING_START, timeMs, x, y, mExtraParamBundleFling);
    }

    /**?
     * End DOUBLE_TAP_MODE_DRAG_ZOOM by sending SCROLL_END and PINCH_END events.
     * @param event A hint event that its x, y, and eventTime will be used for the ending events
     *              to send. This argument is an optional and can be null.
     */
    private void endDoubleTapDragIfNecessary(MotionEvent event) {
        assert event != null;
        if (!isDoubleTapActive()) return;
        if (mDoubleTapMode == DOUBLE_TAP_MODE_DRAG_ZOOM) {
            pinchEnd(event.getEventTime());
            sendGesture(GestureEventType.SCROLL_END, event.getEventTime(),
                    (int) event.getX(), (int) event.getY(), null);
        }
        mDoubleTapMode = DOUBLE_TAP_MODE_NONE;
        updateDoubleTapListener();
    }

    /**
     * Reset touch scroll flag and optionally send a SCROLL_END event if necessary.
     * @param timeMs The time in ms for the event initiating this gesture.
     * @param sendScrollEndEvent Whether to send SCROLL_END event.
     */
    private void endTouchScrollIfNecessary(long timeMs, boolean sendScrollEndEvent) {
        if (!mTouchScrolling) return;
        mTouchScrolling = false;
        if (sendScrollEndEvent) {
            sendGesture(GestureEventType.SCROLL_END, timeMs, 0, 0, null);
        }
    }

    /**
     * @return Whether native is tracking a scroll.
     */
    public boolean isNativeScrolling() {
        // TODO(wangxianzhu): Also return true when fling is active once the UI knows exactly when
        // the fling ends.
        return mTouchScrolling;
    }

    /**
     * @return Whether native is tracking a pinch (i.e. between sending PINCH_BEGIN and PINCH_END).
     */
    boolean isNativePinching() {
        return mPinchInProgress;
    }

    /**
     * Starts a pinch gesture.
     * @param timeMs The time in ms for the event initiating this gesture.
     * @param x The x coordinate for the event initiating this gesture.
     * @param y The x coordinate for the event initiating this gesture.
     */
    protected void pinchBegin(long timeMs, int x, int y) {
        sendGesture(GestureEventType.PINCH_BEGIN, timeMs, x, y, null);
        mBlockScrollEvent = true;
    }

    /**
     * Pinch by a given percentage.
     * @param timeMs The time in ms for the event initiating this gesture.
     * @param anchorX The x coordinate for the anchor point to be used in pinch.
     * @param anchorY The y coordinate for the anchor point to be used in pinch.
     * @param delta The percentage to pinch by.
     */
    protected void pinchBy(long timeMs, int anchorX, int anchorY, float delta) {
        mExtraParamBundlePinchBy.putFloat(DELTA, delta);
        assert mExtraParamBundlePinchBy.size() == 1;
        sendGesture(GestureEventType.PINCH_BY, timeMs, anchorX, anchorY, mExtraParamBundlePinchBy);
        mPinchInProgress = true;
    }

    /**
     * End a pinch gesture.
     * @param timeMs The time in ms for the event initiating this gesture.
     */
    private void pinchEnd(long timeMs) {
        mPinchEndTime = System.currentTimeMillis();
        sendGesture(GestureEventType.PINCH_END, timeMs, 0, 0, null);
        mPinchInProgress = false;
	mBlockScrollEvent = false;
    }

    void blockScrollEvents(boolean blockScroll) {
        // Log.v("PTRJAVA","CVGH.blockScrollEvents mBlockScrollEvent="+blockScroll);
		mBlockScrollEvent=blockScroll;
    }

    /**
     * Ignore singleTap gestures.
     */
    void setIgnoreSingleTap(boolean value) {
        mIgnoreSingleTap = value;
    }

    /**
     * Cancel the current touch event sequence by sending ACTION_CANCEL and ignore all the
     * subsequent events until the next ACTION_DOWN.
     *
     * One example usecase is stop processing the touch events when showing context popup menu.
     */
    public void setIgnoreRemainingTouchEvents() {
        if (mIgnoreRemainingTouchEvents) return;

        MotionEvent me = obtainActionCancelMotionEvent();
        if (mCurrentDownEvent != null) {
            // Only insert a synthetic event if there's an active touch sequence.
            onTouchEvent(me);
        } else {
            // Otherwise, we still want to reset the gesture detector pipeline
            // (e.g., reset double-tap detection state).
            mGestureDetector.onTouchEvent(me);
            processTouchEventForMultiTouch(me);
        }
        me.recycle();

        assert mCurrentDownEvent == null;
        mIgnoreRemainingTouchEvents = true;
    }

    
    public void setPredictionScroll( boolean value ) {	
        mPredictionScroll = value;
    }

    /**
     * Handle the incoming MotionEvent.
     * @return Whether the event was handled.
     */
    protected boolean onTouchEvent(MotionEvent event) {
        final int eventAction = event.getActionMasked();
		
        if (eventAction == MotionEvent.ACTION_DOWN) {
            mIsPointerDownCalled = false;
            mBlockScrollEvent = false;			
        } else if (eventAction == MotionEvent.ACTION_POINTER_DOWN) {
            mIsPointerDownCalled = true;
        }


        if (mPredictionScroll)
        {

            if (eventAction == MotionEvent.ACTION_DOWN) {
                if (mVelocityTracker == null) {
                    mVelocityTracker = VelocityTracker.obtain();
                }
            }
            else if((eventAction == MotionEvent.ACTION_UP || eventAction == MotionEvent.ACTION_CANCEL)) {
                if (mVelocityTracker != null) {
                    mVelocityTracker.recycle();
                    mVelocityTracker = null;
                }
            }
        }
	


        

        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipline, so we ignore them entirely.
        if (eventAction != MotionEvent.ACTION_DOWN
                && eventAction != MotionEvent.ACTION_UP
                && eventAction != MotionEvent.ACTION_CANCEL
                && eventAction != MotionEvent.ACTION_MOVE
                && eventAction != MotionEvent.ACTION_POINTER_DOWN
                && eventAction != MotionEvent.ACTION_POINTER_UP) {
            return false;
        }

        try {
            TraceEvent.begin("onTouchEvent");

            if (mIgnoreRemainingTouchEvents) {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    mIgnoreRemainingTouchEvents = false;
                } else {
                    return false;
                }
            }

            return processTouchEvent(event);
        } finally {
            TraceEvent.end("onTouchEvent");
        }
    }

    /**
     * Handle content view losing focus -- ensure that any remaining active state is removed.
     */
    void onWindowFocusLost() {
        // TODO(jdduke): Determine if this should behave more like setIgnoreRemainingTouchEvents().
        if (mLastLongPressEvent != null) {
            sendTapCancelIfNecessary(mLastLongPressEvent);
        }
    }

    protected MotionEvent obtainActionCancelMotionEvent() {
        MotionEvent me = MotionEvent.obtain(
                mCurrentDownEvent != null ?
                    mCurrentDownEvent.getDownTime() : SystemClock.uptimeMillis(),
                SystemClock.uptimeMillis(),
                MotionEvent.ACTION_CANCEL, 0.0f,  0.0f,  0);
        me.setSource(mCurrentDownEvent != null ?
            mCurrentDownEvent.getSource() : InputDevice.SOURCE_CLASS_POINTER);
        return me;
    }

    /**
     * Resets gesture handlers state; called on didStartLoading().
     * Note that this does NOT clear the pending motion events queue;
     * it gets cleared in hasTouchEventHandlers() called from WebKit
     * FrameLoader::transitionToCommitted iff the page ever had touch handlers.
     */
    void resetGestureHandlers() {
        MotionEvent me = obtainActionCancelMotionEvent();
        mGestureDetector.onTouchEvent(me);
        processTouchEventForMultiTouch(me);
        me.recycle();
    }

    protected boolean processTouchEvent(MotionEvent event) {
        if (!canHandle(event)) return false;

        try {
            mMotionEventDelegate.onTouchEventHandlingBegin(event);

            final boolean wasTouchScrolling = mTouchScrolling;

            mSnapScrollController.setSnapScrollingMode(event, isScaleGestureDetectionInProgress());

            if (event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
                endDoubleTapDragIfNecessary(event);
            } else if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mGestureDetector.setIsLongpressEnabled(true);
                mCurrentDownEvent = MotionEvent.obtain(event);
            }

            boolean handled = mGestureDetector.onTouchEvent(event);
            handled |= processTouchEventForMultiTouch(event);

            if (event.getAction() == MotionEvent.ACTION_UP
                    || event.getAction() == MotionEvent.ACTION_CANCEL) {
                if (event.getAction() == MotionEvent.ACTION_CANCEL) {
                    sendTapCancelIfNecessary(event);
                }

                // "Last finger raised" could be an end to movement, but it should
                // only terminate scrolling if the event did not cause a fling.
                if (wasTouchScrolling && !handled) {
                    endTouchScrollIfNecessary(event.getEventTime(), true);
                }

                if (mCurrentDownEvent != null) recycleEvent(mCurrentDownEvent);
                mCurrentDownEvent = null;
            }
            return handled;
        } finally {
            mMotionEventDelegate.onTouchEventHandlingEnd();
        }
    }

    private boolean isScaleGestureDetectionInProgress() {
        return !mMultiTouchListener.getPermanentlyIgnoreDetectorEvents()
                && mMultiTouchDetector.isInProgress();
    }

    protected boolean processTouchEventForMultiTouch(MotionEvent event) {
        // TODO(jdduke): Need to deal with multi-touch transition
        mMultiTouchListener.setCurrentEventTime(event.getEventTime());
        try {
            boolean inGesture = isScaleGestureDetectionInProgress();
            boolean retVal = mMultiTouchDetector.onTouchEvent(event);
            if (!inGesture && (event.getActionMasked() == MotionEvent.ACTION_UP
                    || event.getActionMasked() == MotionEvent.ACTION_CANCEL)) {
                return false;
            }
            return retVal;
        } catch (Exception e) {
            Log.e(TAG, "ScaleGestureDetector got into a bad state!", e);
            assert false;
        }
        return false;
    }

    private void recycleEvent(MotionEvent event) {
        event.recycle();
    }

    private boolean sendMotionEventAsGesture(
            int type, MotionEvent event, Bundle extraParams) {
        return sendGesture(type, event.getEventTime(),
            (int) event.getX(), (int) event.getY(), extraParams);
    }

    private boolean sendGesture(
            int type, long timeMs, int x, int y, Bundle extraParams) {
        assert timeMs != 0;
        // The only valid gestures that can occur after the touch sequence has
        // ended are SHOW_PRESS and SINGLE_TAP_CONFIRMED, potentially triggered
        // after the double-tap delay window times out.
        if (mCurrentDownEvent == null
                && type != GestureEventType.SINGLE_TAP_CONFIRMED
                && type != GestureEventType.SHOW_PRESS) {
            return false;
        }
        return mMotionEventDelegate.onGestureEventCreated(type, timeMs, x, y, extraParams);
    }

    private boolean sendTapEndingEventAsGesture(int type, MotionEvent e, Bundle extraParams) {
        if (!sendMotionEventAsGesture(type, e, extraParams)) return false;
        mNeedsTapEndingEvent = false;
        return true;
    }

    private void sendTapCancelIfNecessary(MotionEvent e) {
        if (!mNeedsTapEndingEvent) return;
        if (!sendTapEndingEventAsGesture(GestureEventType.TAP_CANCEL, e, null)) return;
        mLastLongPressEvent = null;
    }

    /**
     * @return Whether the ContentViewGestureHandler can handle a MotionEvent right now. True only
     * if it's the start of a new stream (ACTION_DOWN), or a continuation of the current stream.
     */
    private boolean canHandle(MotionEvent ev) {
        return ev.getAction() == MotionEvent.ACTION_DOWN ||
                (mCurrentDownEvent != null && mCurrentDownEvent.getDownTime() == ev.getDownTime());
    }

    /**
     * @return Whether the event can trigger a LONG_TAP gesture. True when it can and the event
     * will be consumed.
     */
    private boolean triggerLongTapIfNeeded(MotionEvent ev) {
        if (mLastLongPressEvent != null
                && ev.getAction() == MotionEvent.ACTION_UP
                && !isScaleGestureDetectionInProgress()) {
            sendTapCancelIfNecessary(ev);
            sendMotionEventAsGesture(GestureEventType.LONG_TAP, ev, null);
            return true;
        }
        return false;
    }

    /**
     * This is for testing only.
     * Sends a show pressed state gesture through mListener. This should always be called after
     * a down event;
     */
    void sendShowPressedStateGestureForTesting() {
        if (mCurrentDownEvent == null) return;
        mListener.onShowPress(mCurrentDownEvent);
    }

    /**
     * This is for testing only.
     * @return Whether a sent TapDown event has been accompanied by a tap-ending event.
     */
    boolean needsTapEndingEventForTesting() {
        return mNeedsTapEndingEvent;
    }

    /**
     * Update whether multi-touch gestures are supported.
     */
    public void updateMultiTouchSupport(boolean supportsMultiTouchZoom) {
        mMultiTouchListener.setPermanentlyIgnoreDetectorEvents(!supportsMultiTouchZoom);
    }

    /**
     * Update whether double-tap gestures are supported. This allows
     * double-tap gesture suppression independent of whether or not the page's
     * viewport and scale would normally prevent double-tap.
     * Note: This should never be called while a double-tap gesture is in progress.
     * @param supportDoubleTap Whether double-tap gestures are supported.
     */
    public void updateDoubleTapSupport(boolean supportDoubleTap) {
        assert !isDoubleTapActive();
        int doubleTapMode = supportDoubleTap ?
                DOUBLE_TAP_MODE_NONE : DOUBLE_TAP_MODE_DISABLED;
        if (mDoubleTapMode == doubleTapMode) return;
        mDoubleTapMode = doubleTapMode;
        updateDoubleTapListener();
    }

    /**
     * Update whether double-tap gesture detection should be suppressed due to
     * the viewport or scale of the current page. Suppressing double-tap gesture
     * detection allows for rapid and responsive single-tap gestures.
     * @param shouldDisableDoubleTap Whether double-tap should be suppressed.
     */
    public void updateShouldDisableDoubleTap(boolean shouldDisableDoubleTap) {
        if (mShouldDisableDoubleTap == shouldDisableDoubleTap) return;
        mShouldDisableDoubleTap = shouldDisableDoubleTap;
        updateDoubleTapListener();
    }

    /**
     * @return Whether double-tap gesture detection is enabled.
     */
    public boolean isDoubleTapDisabled() {
        return mDoubleTapMode == DOUBLE_TAP_MODE_DISABLED || mShouldDisableDoubleTap;
    }

    /**
     * @return Whether the click delay preceding a double tap is disabled.
     */
    public boolean isClickDelayDisabled() {
        return mDisableClickDelay;
    }

    /**
     * @return Whether a double tap-gesture is in-progress.
     */
    public boolean isDoubleTapActive() {
        return mDoubleTapMode != DOUBLE_TAP_MODE_DISABLED &&
               mDoubleTapMode != DOUBLE_TAP_MODE_NONE;
    }

    private void updateDoubleTapListener() {
        if (isDoubleTapDisabled()) {
            // Defer nulling the DoubleTapListener until the double tap gesture is complete.
            if (isDoubleTapActive()) return;
            mGestureDetector.setOnDoubleTapListener(null);
        } else {
            mGestureDetector.setOnDoubleTapListener(mDoubleTapListener);
        }
    }

    private boolean isDistanceGreaterThanTouchSlop(float distanceX, float distanceY) {
        return distanceX * distanceX + distanceY * distanceY > mScaledTouchSlopSquare;
    }
}
