// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.sec.chromium.content.browser;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.util.Log;
import android.view.animation.Animation;
import android.view.animation.AlphaAnimation;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.graphics.Color;

import com.sec.android.app.sbrowser.common.SBrowserUtils;
import com.sec.android.app.sbrowser.mainactivity.controller.MainActivityController;
import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.SBrowserMainActivity;

import com.sec.chromium.content.browser.SbrActivityContentVideoViewClient;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.ui.base.ViewAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.content.browser.ContentVideoViewClient;

/**
 * This class implements accelerated fullscreen video playback using surface view.
 */
@JNINamespace("content")
public class SbrContentVideoView extends FrameLayout
        implements SurfaceHolder.Callback, ViewAndroidDelegate {

    private static final String TAG = "SbrContentVideoView";

    /* Do not change these values without updating their counterparts
     * in include/media/mediaplayer.h!
     */
    private static final int MEDIA_NOP = 0; // interface test message
    private static final int MEDIA_PREPARED = 1;
    private static final int MEDIA_PLAYBACK_COMPLETE = 2;
    private static final int MEDIA_BUFFERING_UPDATE = 3;
    private static final int MEDIA_SEEK_COMPLETE = 4;
    private static final int MEDIA_SET_VIDEO_SIZE = 5;
    private static final int MEDIA_ERROR = 100;
    private static final int MEDIA_INFO = 200;

    /**
     * Keep these error codes in sync with the code we defined in
     * MediaPlayerListener.java.
     */
    public static final int MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 2;
    public static final int MEDIA_ERROR_INVALID_CODE = 3;

    // all possible internal states
    private static final int STATE_ERROR              = -1;
    private static final int STATE_IDLE               = 0;
    private static final int STATE_PLAYING            = 1;
    private static final int STATE_PAUSED             = 2;
    private static final int STATE_PLAYBACK_COMPLETED = 3;

    private SurfaceHolder mSurfaceHolder;
    private int mVideoWidth;
    private int mVideoHeight;
    private int mDuration;

    // Native pointer to C++ ContentVideoView object.
    private long mNativeSbrContentVideoView;

    // webkit should have prepared the media
    private static int mCurrentState = STATE_IDLE;

    // Strings for displaying media player errors
    private String mPlaybackErrorText;
    private String mUnknownErrorText;
    private String mErrorButton;
    private String mErrorTitle;
    private String mVideoLoadingText;

    // This view will contain the video.
    private VideoSurfaceView mVideoSurfaceView;

    // Progress view when the video is loading.
    private View mProgressView;

    // The ViewAndroid is used to keep screen on during video playback.
    private ViewAndroid mViewAndroid;

    protected final ContentVideoViewClient mClient;

    private final MainActivityController mActivityController;

    private static boolean sIsOverlayVideoMode = false;
    
    private boolean IsSurfaceCreated = false;

    private Context mContext;

    public class VideoSurfaceView extends SurfaceView {

        boolean bIsPresentationMode = false;

        public VideoSurfaceView(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int width = getDefaultSize(mVideoWidth, widthMeasureSpec);
            int height = getDefaultSize(mVideoHeight, heightMeasureSpec);
            if (mVideoWidth > 0 && mVideoHeight > 0) {
                if (mVideoWidth * height  > width * mVideoHeight) {
                    height = width * mVideoHeight / mVideoWidth;
                } else if (mVideoWidth * height  < width * mVideoHeight) {
                    width = height * mVideoWidth / mVideoHeight;
                }
            }
            setMeasuredDimension(width, height);
        }

        public boolean isPresentationMode(){
            return bIsPresentationMode;
        }

        public void setIsPresentationMode(boolean isPresentationMode){
            bIsPresentationMode = isPresentationMode;
        }
    }

    private static class ProgressView extends LinearLayout {

        private final ProgressBar mProgressBar;
        private final TextView mTextView;

        public ProgressView(Context context, String videoLoadingText) {
            super(context);
            setOrientation(LinearLayout.VERTICAL);
            setLayoutParams(new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT));
            mProgressBar = new ProgressBar(context, null, android.R.attr.progressBarStyleLarge);
            mTextView = new TextView(context);
            mTextView.setTextColor(Color.WHITE);
            mTextView.setText(videoLoadingText);
            addView(mProgressBar);
            addView(mTextView);
        }
    }

    private final Runnable mExitFullscreenRunnable = new Runnable() {
        @Override
        public void run() {
            exitFullscreen(true);
        }
    };

    protected SbrContentVideoView(Context context, long nativeSbrContentVideoView,
            ContentVideoViewClient client, int width, int height) {
        super(context);
        
        if(context instanceof SBrowserMainActivity)
            mActivityController = ((SBrowserMainActivity) context).getController();
        else
            mActivityController = null;
        
        mNativeSbrContentVideoView = nativeSbrContentVideoView;
        mViewAndroid = new ViewAndroid(new WindowAndroid(context.getApplicationContext()), this);
        mClient = client;
        if((height != 0) && (width != 0)) {
            mVideoWidth = width;
            mVideoHeight = height;
        }
        initResources(context);
        mVideoSurfaceView = new VideoSurfaceView(context);
        mVideoSurfaceView.setBackgroundColor(Color.BLACK);
        
        setBackgroundColor(Color.BLACK);
        
        Animation alphaAnimation = null;
        alphaAnimation = new AlphaAnimation(0.0f, 1.0f);
        alphaAnimation.setDuration(300);
        
        setAnimation(alphaAnimation);
        setVisibility(View.VISIBLE);

        alphaAnimation.setAnimationListener(new Animation.AnimationListener() {
            public void onAnimationStart(Animation arg0) {
                Log.d(TAG,"[html5media] animation start");
            }
            public void onAnimationRepeat(Animation arg0) {
            }
            public void onAnimationEnd(Animation arg0) {
                Log.d(TAG,"[html5media] animation end");
                setOverlayVideoMode(true);
                showContentVideoView();
            }
        });

        mClient.onShowCustomView(this);
        mContext = context;
    }

    private void initResources(Context context) {
        if (mPlaybackErrorText != null) return;
        mPlaybackErrorText = context.getString(
                R.string.media_player_error_text_invalid_progressive_playback);
        mUnknownErrorText = context.getString(
                R.string.media_player_error_text_unknown);
        mErrorButton = context.getString(
                R.string.media_player_error_button);
        mErrorTitle = context.getString(
                R.string.media_player_error_title);
        mVideoLoadingText = context.getString(
                R.string.media_player_loading_video);
    }

    public void showContentVideoView() {
        mVideoSurfaceView.getHolder().addCallback(this);

        this.addView(mVideoSurfaceView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
                Gravity.CENTER));

        mProgressView = mClient.getVideoLoadingProgressView();
        if (mProgressView == null) {
            mProgressView = new ProgressView(getContext(), mVideoLoadingText);
        }
        this.addView(mProgressView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));
    }

    public VideoSurfaceView getSurfaceView() {
        return mVideoSurfaceView;
    }

    @CalledByNative
    public void onMediaPlayerError(int errorType) {
        Log.d(TAG, "[html5media] OnMediaPlayerError: " + errorType);
        if (mCurrentState == STATE_ERROR || mCurrentState == STATE_PLAYBACK_COMPLETED) {
            return;
        }

        // Ignore some invalid error codes.
        if (errorType == MEDIA_ERROR_INVALID_CODE) {
            return;
        }

        mCurrentState = STATE_ERROR;

        /* Pop up an error dialog so the user knows that
         * something bad has happened. Only try and pop up the dialog
         * if we're attached to a window. When we're going away and no
         * longer have a window, don't bother showing the user an error.
         *
         * TODO(qinmin): We need to review whether this Dialog is OK with
         * the rest of the browser UI elements.
         */
        if (getWindowToken() != null) {
            String message;

            if (errorType == MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK) {
                message = mPlaybackErrorText;
            } else {
                message = mUnknownErrorText;
            }

            try {
                new AlertDialog.Builder(getContext())
                    .setTitle(mErrorTitle)
                    .setMessage(message)
                    .setPositiveButton(mErrorButton,
                            new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int whichButton) {
                            /* Inform that the video is over.
                             */
                            onCompletion();
                        }
                    })
                    .setCancelable(false)
                    .show();
            } catch (RuntimeException e) {
                Log.e(TAG, "Cannot show the alert dialog, error message: " + message, e);
            }
        }
    }

    @CalledByNative
    private void onVideoSizeChanged(int width, int height) {
        Log.d(TAG,"[html5media] onVideoSizeChanged. " + width + ", " + height);
        if(width != mVideoWidth && height != mVideoHeight) {
            mVideoWidth = width;
            mVideoHeight = height;
            // This will trigger the SurfaceView.onMeasure() call.
            mVideoSurfaceView.getHolder().setFixedSize(mVideoWidth, mVideoHeight);
        }
    }

    @CalledByNative
    protected void onBufferingUpdate(int percent) {
    }

    @CalledByNative
    protected void onPlaybackComplete() {
        onCompletion();
    }

    @CalledByNative
    protected void onUpdateMediaMetadata(
            int videoWidth,
            int videoHeight,
            int duration,
            boolean canPause,
            boolean canSeekBack,
            boolean canSeekForward) {
        mDuration = duration;
        if(mProgressView != null)
            mProgressView.setVisibility(View.GONE);
        mCurrentState = isPlaying() ? STATE_PLAYING : STATE_PAUSED;
        onVideoSizeChanged(videoWidth, videoHeight);
    }

    @CalledByNative
    protected void onUpdateCCVisibility(int status) {
    }

    protected void setBackgroundTransparent(){
        setBackgroundColor(Color.TRANSPARENT);
        if(mVideoSurfaceView != null)
            mVideoSurfaceView.setBackgroundColor(Color.TRANSPARENT);
    }

    public void setBackgroundBlack(){
        setBackgroundColor(Color.BLACK);
        if(mVideoSurfaceView != null)
            mVideoSurfaceView.setBackgroundColor(Color.BLACK);
    }

    public void hidePopups() {
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mSurfaceHolder = holder;
        openVideo();
        if (mClient != null)
            ((SbrActivityContentVideoViewClient)mClient).setFullscreen(true);
        if (mActivityController != null) {
            if (SBrowserUtils.isKeyboardShowing(getContext())) {
                SBrowserUtils.hideKeyboard(mActivityController.getLocationBar());
            }
            if (mActivityController.getFullScreenManager() != null) {
                mActivityController.getFullScreenManager().hideToolBarIfVisible();
            }
            mActivityController.setLocaltionBarClearFocus();
        }
        IsSurfaceCreated = true;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (mNativeSbrContentVideoView != 0) {
            nativeSetSurface(mNativeSbrContentVideoView, null);
        }
        mSurfaceHolder = null;
        post(mExitFullscreenRunnable);
        if (mClient != null)
            ((SbrActivityContentVideoViewClient)mClient).setFullscreen(false);
        IsSurfaceCreated = false;
    }

    protected boolean getSurfaceState() {
        return IsSurfaceCreated;
    }

    @CalledByNative
    protected void openVideo() {
        if (mSurfaceHolder != null) {
            mCurrentState = STATE_IDLE;
            if (mNativeSbrContentVideoView != 0) {
                Log.d(TAG,"[html5media] openVideo");
                nativeRequestMediaMetadata(mNativeSbrContentVideoView);
                nativeSetSurface(mNativeSbrContentVideoView,
                        mSurfaceHolder.getSurface());
            }
        }
    }

    protected void onCompletion() {
        mCurrentState = STATE_PLAYBACK_COMPLETED;
    }


    protected boolean isInPlaybackState() {
        return (mCurrentState != STATE_ERROR && mCurrentState != STATE_IDLE);
    }

    protected void start() {
        if (isInPlaybackState()) {
            if (mNativeSbrContentVideoView != 0) {
                nativePlay(mNativeSbrContentVideoView);
            }
            mCurrentState = STATE_PLAYING;
        }
    }

    protected void pause() {
        if (isInPlaybackState()) {
            if (isPlaying()) {
                if (mNativeSbrContentVideoView != 0) {
                    nativePause(mNativeSbrContentVideoView);
                }
                mCurrentState = STATE_PAUSED;
            }
        }
    }

    // cache duration as mDuration for faster access
    protected int getDuration() {
        if (isInPlaybackState()) {
            if (mDuration > 0) {
                return mDuration;
            }
            if (mNativeSbrContentVideoView != 0) {
                mDuration = nativeGetDurationInMilliSeconds(mNativeSbrContentVideoView);
            } else {
                mDuration = 0;
            }
            return mDuration;
        }
        mDuration = -1;
        return mDuration;
    }

    protected int getCurrentPosition() {
        if (isInPlaybackState() && mNativeSbrContentVideoView != 0) {
            return nativeGetCurrentPosition(mNativeSbrContentVideoView);
        }
        return 0;
    }

    protected void seekTo(int msec) {
        if (mNativeSbrContentVideoView != 0) {
            nativeSeekTo(mNativeSbrContentVideoView, msec);
        }
    }

    protected boolean isPlaying() {
        return mNativeSbrContentVideoView != 0 && nativeIsPlaying(mNativeSbrContentVideoView);
    }

    @CalledByNative
    private static SbrContentVideoView createSbrContentVideoView(
            Context context, long nativeSbrContentVideoView, ContentVideoViewClient client,
            boolean legacy, int width, int height) {
        ThreadUtils.assertOnUiThread();
        // The context needs be Activity to create the ContentVideoView correctly.
        if (!(context instanceof Activity)) {
            Log.w(TAG, "Wrong type of context, can't create fullscreen video");
            return null;
        }
        Log.d(TAG,"[html5media] createSbrContentVideoView. l : " + legacy);
        if (legacy || !(context instanceof SBrowserMainActivity)) {
            return new SbrContentVideoViewLegacy(context, nativeSbrContentVideoView, client, width, height);
        } else {
            sIsOverlayVideoMode = true;
            return new SbrContentVideoViewNew(context, nativeSbrContentVideoView, client, width, height);
        }
    }

    public void removeSurfaceView(boolean shouldSetNull) {
        Log.d(TAG,"[html5media] removeSurfaceView. " + shouldSetNull);
        removeView(mVideoSurfaceView);
        removeView(mProgressView);
        if(shouldSetNull){
            mVideoSurfaceView = null;
            mProgressView = null;
        }
    }

    public void exitFullscreen(boolean relaseMediaPlayer) {
        destroyContentVideoView(false);
        if (mNativeSbrContentVideoView != 0) {
            nativeExitFullscreen(mNativeSbrContentVideoView, relaseMediaPlayer);
            mNativeSbrContentVideoView = 0;
        }
    }

    @CalledByNative
    private void onExitFullscreen() {
        exitFullscreen(false);
    }

    @CalledByNative
    protected void onMediaInterrupted() {
        // Fix for calling updatePausePlay to update full-screen video controller when onMediaInterrupted
        // This will be overridden by each class which extends SbrContentVideoView.
    }

    @CalledByNative
    protected void onStart() {
        // Fix for the cases where video starts after a full-screen view was created.
        // This will be overridden by each class which extends SbrContentVideoView.
    }

    /**
     * This method shall only be called by native and exitFullscreen,
     * To exit fullscreen, use exitFullscreen in Java.
     */
    @CalledByNative
    protected void destroyContentVideoView(boolean nativeViewDestroyed) {
        if (mVideoSurfaceView != null) {
            removeSurfaceView(true);
            setVisibility(View.GONE);

            // To prevent re-entrance, call this after removeSurfaceView.
            mClient.onDestroyContentVideoView();
        }
        
        if (nativeViewDestroyed) {
            mNativeSbrContentVideoView = 0;
        }
    }

    public static SbrContentVideoView getSbrContentVideoView() {
        return nativeGetSingletonJavaSbrContentVideoView();
    }

    @Override
    public View acquireAnchorView() {
        View anchorView = new View(getContext());
        addView(anchorView);
        return anchorView;
    }

    @Override
    public void setAnchorViewPosition(View view, float x, float y, float width, float height) {
        Log.e(TAG, "setAnchorViewPosition isn't implemented");
    }

    @Override
    public void releaseAnchorView(View anchorView) {
        removeView(anchorView);
    }

    @CalledByNative
    private long getNativeViewAndroid() {
        return mViewAndroid.getNativePointer();
    }

    protected void setOverlayVideoMode(boolean enable) {
        Log.d(TAG,"[html5media] setOverlayVideoMode. " + enable);
        if (mActivityController != null) {
            sIsOverlayVideoMode = enable;
            mActivityController.setOverlayVideoMode(enable);
        }
    }

    public boolean isCurrentInstanceInFullScreenMode(Context context) {
        if (sIsOverlayVideoMode && (mContext == context)) {
            return true;
        } else {
            return false;
        }
    }

    protected void setCCVisibility(boolean visible){
        if (mNativeSbrContentVideoView != 0) {
            nativeSetCCVisibility(mNativeSbrContentVideoView, visible);
        }
    }

    public void onActivityResume(){
    }

    protected static int getCurrentState() {
        return mCurrentState;
    }

    private static native SbrContentVideoView nativeGetSingletonJavaSbrContentVideoView();
    private native void nativeExitFullscreen(long nativeSbrContentVideoView,
            boolean relaseMediaPlayer);
    private native int nativeGetCurrentPosition(long nativeSbrContentVideoView);
    private native int nativeGetDurationInMilliSeconds(long nativeSbrContentVideoView);
    private native void nativeRequestMediaMetadata(long nativeSbrContentVideoView);
    private native int nativeGetVideoWidth(long nativeSbrContentVideoView);
    private native int nativeGetVideoHeight(long nativeSbrContentVideoView);
    private native boolean nativeIsPlaying(long nativeSbrContentVideoView);
    private native void nativePause(long nativeSbrContentVideoView);
    private native void nativePlay(long nativeSbrContentVideoView);
    private native void nativeSeekTo(long nativeSbrContentVideoView, int msec);
    private native void nativeSetSurface(long nativeSbrContentVideoView, Surface surface);
    
    private native void nativeSetCCVisibility(long nativeSbrContentVideoView, boolean visible);
}
