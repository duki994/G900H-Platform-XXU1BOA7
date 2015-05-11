// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.sec.chromium.content.browser;

import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.Message;
import android.os.SystemProperties;
import android.provider.Settings;
import android.util.Log;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.MediaController;
import org.chromium.base.CalledByNative;
import org.chromium.content.browser.ContentVideoViewClient;

import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.mainactivity.controller.MainActivityController;
import com.sec.android.app.sbrowser.SBrowserMainActivity;

import com.sec.chromium.content.browser.SbrWifiDisplayManager;

public class SbrContentVideoViewNew extends SbrContentVideoView implements SbrVideoControllerView.MediaPlayerControl{
    private static final String TAG = "SbrContentVideoViewNew";
    private static final String SCREEN_AUTO_BRIGHTNESS_DETAIL = "auto_brightness_detail";

    private static final long KEY_LONG_PRESS_TIME = 500L;

    private static final int START_GESTURE = 0;
    private static final int SET_BACKGROUND_TRANSPARENT = 1;
    private static final int START_VIDEO_AFTER_WFD_CONNECTED = 2;
    private static final int START_VIDEO_WFD_AUTOPLAY = 3;
    
    private static final float MOTION_START_GESTURE_THRESHOLD = 3.0f;
    private static final float MOTION_MOVE_GESTURE_ADJUSTMENT = 1.5f;

    SbrVideoControllerView  mMediaController;
    SbrWifiDisplayManager   mWFDManager;
    ImageView               mAllshareView;
    
    private SbrVideoLockCtrl mVideoLockCtrl;

    private ContentObserver mRotationObserver = null;

    private GestureDetector mGestureDetector;

    private int mCurrentBufferPercentage;

    private int mScreenOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;

    private int mXTouchPos = 0;
    private int mYTouchPos = 0;
    private int mDownYPos = 0;
    private int mGestureThreshold = 5;

    private enum VideoGestureMode {
        MODE_UNDEFINED, VOLUME_MODE, BRIGHTNESS_MODE
    }

    private VideoGestureMode mVideoGestureMode = VideoGestureMode.MODE_UNDEFINED;
    private boolean mIsVideoGestureStart = false;
    private boolean mIsSyncBrightnessNeeded = false;
    private SbrVideoGestureView mVideoGestureView = null;

    private boolean mEnteredFullscreen = false;
    private boolean mPowerKeyRequested = true;
    private int mBackKeyRepeatCnt = 0;

    private class VideoGestureDetector extends GestureDetector.SimpleOnGestureListener {
        private VideoGestureDetector() {
            super();
        }
        @Override
        public boolean onDoubleTap(MotionEvent event) {
            Log.d(TAG, "[html5media] VideoGestureDetector - onDoubleTap");
            if (isPlaying()) {
                pause();
            } else {
                start();
            }
            if (!SbrVideoLockCtrl.getLockState() && isInPlaybackState()) {
                toggleMediaControlsVisiblity();
            }
            return true;
        }
        @Override
        public boolean onSingleTapConfirmed(MotionEvent event) {
            if (!SbrVideoLockCtrl.getLockState() && isInPlaybackState()) {
                toggleMediaControlsVisiblity();
            }
            return true;
        }
    }

    SbrContentVideoViewNew(Context context, long nativeContentVideoView,
            ContentVideoViewClient client, int width, int height) {
        super(context, nativeContentVideoView, client, width, height);

        mWFDManager = new SbrWifiDisplayManager(context);
        mWFDManager.setVideoView(this);

        mMediaController = new SbrVideoControllerView(context);
        mMediaController.setMediaPlayer(this);
        mMediaController.setWFDManager(mWFDManager);
        mMediaController.setAnchorView(this);

        VideoGestureDetector mVideoGestureDetector = new VideoGestureDetector();
        mGestureDetector = new GestureDetector(context, mVideoGestureDetector);

        mVideoLockCtrl = new SbrVideoLockCtrl(context);
        mGestureThreshold =  (int) (MOTION_START_GESTURE_THRESHOLD * context.getResources().getDisplayMetrics().density);

        mCurrentBufferPercentage = 0;
        registerContentObserver();
    }

    @Override
    protected void openVideo() {
        super.openVideo();
        mCurrentBufferPercentage = 0;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (SbrVideoLockCtrl.getLockState() && mVideoLockCtrl != null) {
            mVideoLockCtrl.showLockIcon();
            return true;
        }
        toggleMediaControlsVisiblity();
        return false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP) {
            finishUpVideoGesture();
        }
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_HEADSETHOOK:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
                if (isInPlaybackState() && event.getAction() == KeyEvent.ACTION_UP && mMediaController != null) {
                    if (isPlaying()) {
                        pause();
                        mMediaController.show();
                    } else {
                        start();
                        mMediaController.hide();
                    }
                }
                return true;
            case KeyEvent.KEYCODE_MEDIA_PLAY:
                if (isInPlaybackState() && event.getAction() == KeyEvent.ACTION_UP && mMediaController != null) {
                    if (!isPlaying()) {
                        start();
                        mMediaController.hide();
                    }
                }
                return true;
            case KeyEvent.KEYCODE_MEDIA_STOP:
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
                if (isInPlaybackState() && event.getAction() == KeyEvent.ACTION_UP && mMediaController != null) {
                    if (isPlaying()) {
                        pause();
                        mMediaController.show();
                    }
                }
                return true;
            case KeyEvent.KEYCODE_ESCAPE:
            case KeyEvent.KEYCODE_BACK:
                if(event.getAction() == KeyEvent.ACTION_DOWN)
                    mBackKeyRepeatCnt = event.getRepeatCount();
                if (getSurfaceState() && event.getAction() == KeyEvent.ACTION_UP && mBackKeyRepeatCnt == 0) {
                    if (SbrVideoLockCtrl.getLockState() && mVideoLockCtrl != null) {
                        mVideoLockCtrl.showLockIcon();
                        return true;
                    }
                    Log.d(TAG,"[html5media] onkey. back");
                    exitFullscreen(false);
                }
                return true;
            case KeyEvent.KEYCODE_POWER:
                if (event.getAction() == KeyEvent.ACTION_UP) {
                    long pressTime = event.getEventTime() - event.getDownTime();
                    if (pressTime < KEY_LONG_PRESS_TIME) {
                        Log.d(TAG,"[html5media] KeyEvent.KEYCODE_POWER for full-screen video LockMode");
                        if (mWFDManager != null && mWFDManager.isShowingDevicePopup())
                            mWFDManager.dismissDevicePopup();
                        if (mVideoLockCtrl != null) {
                            if (SbrVideoLockCtrl.getLockState()) {
                                mVideoLockCtrl.toggleFullscreenLock(false);
                            } else {
                                if (mMediaController != null && mMediaController.isAnimating()) {
                                    mMediaController.hideForced();
                                }
                                finishUpVideoGesture();
                                mVideoLockCtrl.toggleFullscreenLock(true);
                            }
                        }
                    }
                }
                return true;
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_SEARCH:
                return true;
            case KeyEvent.KEYCODE_HOME:
            case KeyEvent.KEYCODE_APP_SWITCH:
                if (SbrVideoLockCtrl.getLockState() && mVideoLockCtrl != null) {
                    mVideoLockCtrl.showLockIcon();
                }
                return true;
            case KeyEvent.KEYCODE_VOLUME_UP:
                if (event.getAction() == KeyEvent.ACTION_DOWN && isPlaying()) {
                    if (!mIsVideoGestureStart && mMediaController != null && mMediaController.keyVolumeUp())
                        mMediaController.showPopupVolbar();
                }
                return true;
            case KeyEvent.KEYCODE_VOLUME_DOWN:
                if (event.getAction() == KeyEvent.ACTION_DOWN && isPlaying()) {
                    if (!mIsVideoGestureStart && mMediaController != null && mMediaController.keyVolumeDown())
                        mMediaController.showPopupVolbar();
                }
                return true;
            default:
                break;
        }
        return super.dispatchKeyEvent(event);
    }
    
    @Override
    protected void onUpdateMediaMetadata(
            int videoWidth,
            int videoHeight,
            int duration,
            boolean canPause,
            boolean canSeekBack,
            boolean canSeekForward) {
        super.onUpdateMediaMetadata(videoWidth, videoHeight, duration,
                canPause, canSeekBack, canSeekForward);

        if (mMediaController == null) return;

        Log.d(TAG,"[html5media] onUpdateMediaMetadata");

        mMediaController.setEnabled(true);

        // If paused , should show the controller forever.
        if (!SbrVideoLockCtrl.getLockState()) {
            if (isPlaying()) {
                mMediaController.show();
            } else {
                mMediaController.show(0);
            }
        }
    }

    @Override
    public void setBackgroundBlack() {
        super.setBackgroundBlack();
        if(mHandler != null)
            mHandler.sendEmptyMessageDelayed(SET_BACKGROUND_TRANSPARENT, 800);
    }

    @Override
    public void hidePopups() {
        if (mMediaController != null) {
            mMediaController.checkShowingAndDismissPopupVolBar();
        }
        finishUpVideoGesture();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.d(TAG,"[html5media] surfaceChanged. " + width + ", " + height);
        super.surfaceChanged(holder, format, width, height);

        if(mHandler != null)
            mHandler.sendEmptyMessageDelayed(SET_BACKGROUND_TRANSPARENT, 400);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG,"[html5media] surfaceCreated");

        // only for the first time fullscreen view is created
        if(mEnteredFullscreen == false && mHandler != null){
            mHandler.sendEmptyMessageDelayed(START_VIDEO_WFD_AUTOPLAY, 1000);
            mEnteredFullscreen = true;
        }

        super.surfaceCreated(holder);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG,"[html5media] surfaceDestroyed");

        VideoSurfaceView surfaceView = getSurfaceView();
        if(surfaceView != null && surfaceView.isPresentationMode()) {
            Log.d(TAG,"[html5media] surfaceView is in presentation mode");
            return;
        }

        setOverlayVideoMode(false);
        unRegisterContentObserver();

        super.surfaceDestroyed(holder);
    }

    @Override
    public void showContentVideoView() {
        Log.d(TAG,"[html5media] showContentVideoView");
        VideoSurfaceView surfaceView = getSurfaceView();
        if (surfaceView == null) return;

        setOnTouchListener(new OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (!getSurfaceState()) {
                    return true;
                }
                if (SbrVideoLockCtrl.getLockState() && mVideoLockCtrl != null) {
                    mVideoLockCtrl.showLockIcon();
                    return true;
                }
                final int action = event.getAction();
                switch (action) {
                    case MotionEvent.ACTION_POINTER_2_DOWN:
                        Log.d(TAG, "[html5media] OnTouchListener - MotionEvent.ACTION_POINTER_2_DOWN");
                        if (mHandler != null)
                            mHandler.removeMessages(START_GESTURE);
                        break;
                    case MotionEvent.ACTION_MOVE:
                        //It should pass, if start point was the touch area of notification(2 times of height). 
                        if (mDownYPos < getContext().getResources().getDimension(R.dimen.notification_height) * 2) {
                            break;
                        }
                        if (!mIsVideoGestureStart 
                                && (mVideoGestureMode == VideoGestureMode.MODE_UNDEFINED)
                                && !(event.getPointerCount() >= 2)) {
                            int ty = (int) event.getY();
                            int tx = (int) event.getX();
                            int tmoveYVal = Math.abs(mYTouchPos - ty);
                            int tmoveXVal = Math.abs(mXTouchPos - tx);
                            if (tmoveYVal >= mGestureThreshold && tmoveYVal > tmoveXVal * 2) {
                                int screenWidth = ((SBrowserMainActivity) getContext()).getWindow().getDecorView().getWidth();
                                mVideoGestureMode = ((mXTouchPos > screenWidth / 2) ? VideoGestureMode.VOLUME_MODE : VideoGestureMode.BRIGHTNESS_MODE);

                                startShowGesture();
                                mHandler.sendEmptyMessageDelayed(START_GESTURE, 50);
                            }
                        } else if (mIsVideoGestureStart && mVideoGestureView != null && mVideoGestureView.isShowing()) {
                            int y = (int) event.getY();
                            if (y == mYTouchPos)
                                break;
                            float adjValue = MOTION_MOVE_GESTURE_ADJUSTMENT / getContext().getResources().getDisplayMetrics().density;
                            int moveVal = (int) ((mYTouchPos - y) * adjValue);

                            if (moveVal != 0) {
                                if (mVideoGestureMode == VideoGestureMode.VOLUME_MODE) {
                                    mVideoGestureView.setVolume(moveVal);
                                } else if (mVideoGestureMode == VideoGestureMode.BRIGHTNESS_MODE) {
                                    mVideoGestureView.setBrightness(moveVal);
                                    if (!mIsSyncBrightnessNeeded)
                                        mIsSyncBrightnessNeeded = true;
                                }
                            }
                        }
                        mXTouchPos = (int) event.getX();
                        mYTouchPos = (int) event.getY();
                        break;
                    case MotionEvent.ACTION_DOWN:
                        mXTouchPos = (int) event.getX();
                        mYTouchPos = mDownYPos = (int) event.getY();
                        if (mMediaController != null) {
                            mMediaController.checkShowingAndDismissPopupVolBar();
                        }
                        break;
                    case MotionEvent.ACTION_CANCEL:
                        finishUpVideoGesture();
                        break;
                    case MotionEvent.ACTION_UP:
                        finishUpVideoGesture();
                        break;
                    default:
                        break;
                }
                if (mGestureDetector != null) {
                    mGestureDetector.onTouchEvent(event);
                }
                return true;
            }
        });
        surfaceView.setFocusable(true);
        surfaceView.setFocusableInTouchMode(true);
        surfaceView.requestFocus();

        super.showContentVideoView();
    }

    @Override
    public void exitFullscreen(boolean relaseMediaPlayer) {
        Log.d(TAG,"[html5media] exitFullscreen.");

        if (mHandler != null){
            mHandler.removeMessages(SET_BACKGROUND_TRANSPARENT);
            mHandler.removeMessages(START_VIDEO_AFTER_WFD_CONNECTED);
            mHandler.removeMessages(START_VIDEO_WFD_AUTOPLAY);
        }

        if(mWFDManager != null){
            mWFDManager.dismissDevicePopup();
            mWFDManager.dismissPresentation(true);
            mWFDManager.unregisterWifiDisplayReceiver();
            //mWFDManager.unregisterDisplayListener();
            mWFDManager = null;
        }
        
        super.exitFullscreen(relaseMediaPlayer);
    }

    @Override
    protected void onBufferingUpdate(int percent) {
        super.onBufferingUpdate(percent);
        mCurrentBufferPercentage = percent;
    }

    @Override
    public void onMediaPlayerError(int errorType) {
        if(mWFDManager != null)
            mWFDManager.onPlayerError();
        
        super.onMediaPlayerError(errorType);
    }

    private void toggleMediaControlsVisiblity() {        
        if (mMediaController != null) {
            if (mMediaController.isShowing()) {
                mMediaController.hide();
            } else {
                mMediaController.show();
            }
        }
    }

    @Override
    protected void onMediaInterrupted() {
        if (mMediaController != null) {
            mMediaController.updatePausePlay();
        }
    }

    @Override
    protected void onStart() {
        if (mMediaController != null && mMediaController.isShowing()) {
            mMediaController.hideForced();
            mMediaController.showForced();
        }
    }

    @Override
    protected void onPlaybackComplete() {
        super.onCompletion();
        if (mMediaController != null) {
            mMediaController.setCurrentTimeToMax();
        }
    }

    @CalledByNative
    protected void destroyContentVideoView(boolean nativeViewDestroyed) {
        Log.d(TAG,"[html5media] destroyContentVideoView");
        finishUpVideoGesture();
        if (mVideoGestureView != null) {
            mVideoGestureView.releaseView();
            mVideoGestureView = null;
        }
        if (mMediaController != null) {
            SBrowserMainActivity activity = (SBrowserMainActivity) getContext();
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
            
            mMediaController.setEnabled(false);
            mMediaController.hide();
            mMediaController.releaseView();
            mMediaController = null;
        }

        mVideoLockCtrl.releaseLockCtrlView();
        unRegisterContentObserver();

        super.destroyContentVideoView(nativeViewDestroyed);
    }

    @Override
    protected void onUpdateCCVisibility(int status) {
        if(mMediaController != null)
            mMediaController.updateClosedCaptionBtn(status);
    }

    private void registerContentObserver() {
        if(getContext() == null) return;
        Log.d(TAG, "[html5media] registerContentObserver");

        // Auto Rotation Change Observer
        if (mRotationObserver == null) {
            mRotationObserver = new ContentObserver(new Handler()) {
                public void onChange(boolean selfChange) {
                    Log.d(TAG, "[html5media] registerContentObserver onChange()");
                    SBrowserMainActivity activity = (SBrowserMainActivity)getContext();
                    if(mMediaController != null){
                        mMediaController.updateAutoRotationBtn();
                        if(mMediaController.isAutoRotation()){
                            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
                        }
                        else{
                            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                        }
                    }
                }
            };
            ContentResolver cr = getContext().getContentResolver();
            Uri tmp = Settings.System.getUriFor(Settings.System.ACCELEROMETER_ROTATION);
            if(cr != null)
                cr.registerContentObserver(tmp, false, mRotationObserver);
        }
        else{
            Log.d(TAG,"[html5media] registerContentObserver. mRotationObserver is already registered.");
        }
    }

    private void unRegisterContentObserver() {
        if(getContext() == null || mRotationObserver == null) return;

        Log.d(TAG, "[html5media] unregisterContentObserver");
        ContentResolver cr = getContext().getContentResolver();
        if (cr != null && mRotationObserver != null) {
            cr.unregisterContentObserver(mRotationObserver);
            mRotationObserver = null;
        }
    }

    private Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case START_GESTURE:
                    mIsVideoGestureStart = true;
                    break;
                case SET_BACKGROUND_TRANSPARENT:
                    setBackgroundTransparent();
                    break;
                case START_VIDEO_AFTER_WFD_CONNECTED:
                    start();
                    if (mMediaController != null)
                        mMediaController.updatePausePlay();
                    break;
                case START_VIDEO_WFD_AUTOPLAY:
                    if(mWFDManager != null)
                        mWFDManager.showPresentationAutoplay(isPlaying());
                    break;
                default:
                    break;
            }
        }
    };

    public void attachVideoGestureView() {
        if (mVideoGestureView == null) {
            mVideoGestureView = new SbrVideoGestureView(((SBrowserMainActivity) getContext()).getWindow(), getContext());
            mVideoGestureView.addViewTo(this);
        }
    }

    public void startShowGesture() {
        if (mHandler != null) {
            mHandler.removeMessages(START_GESTURE);
        }

        if (mVideoGestureView == null)
            attachVideoGestureView();

        if (mVideoGestureView != null) {
            if (mMediaController != null && mMediaController.isShowing())
                mMediaController.hide();

            if (mVideoGestureMode == VideoGestureMode.VOLUME_MODE)
                mVideoGestureView.showVolume();
            else
                mVideoGestureView.showBrightness();
        }
    }

    // returns false if GestureView not used
    public boolean finishUpVideoGesture() {
        if (mHandler != null && mHandler.hasMessages(START_GESTURE))
            mHandler.removeMessages(START_GESTURE);

        mIsVideoGestureStart = false;

        if (mVideoGestureView != null && mVideoGestureView.isShowing()) {
            mVideoGestureView.hide();
            if (mVideoGestureMode == VideoGestureMode.BRIGHTNESS_MODE && mIsSyncBrightnessNeeded) {
                mVideoGestureView.syncBrightnessWithSystemLevel();
                mIsSyncBrightnessNeeded = false;
            }

            mVideoGestureMode = VideoGestureMode.MODE_UNDEFINED;
            return true;
        }
        mVideoGestureMode = VideoGestureMode.MODE_UNDEFINED;
        return false;
    }

    public void onPresentationStart(boolean wasPlaying){
        if(mAllshareView == null){
            mAllshareView = new ImageView(getContext());
            mAllshareView.setBackground(getContext().getResources().getDrawable(R.drawable.video_player_allshare_cast));
        }

        this.addView(mAllshareView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));

        if(wasPlaying && mHandler != null)
            mHandler.sendEmptyMessageDelayed(START_VIDEO_AFTER_WFD_CONNECTED, 1000);
    }

    public void onPresentationStop(){
        this.removeView(mAllshareView);
        mAllshareView = null;
    }

    public void onActivityResume(){
        if(mWFDManager != null)
            mWFDManager.onActivityResume();
        
        if (mClient != null)
            ((SbrActivityContentVideoViewClient)mClient).setFullscreen(true);
    }

    public void pauseFullScreenVideo() {
        if (isPlaying()) {
            pause();
        }
    }

    public void setVideoLockCtrl(boolean request) {
        if (mVideoLockCtrl != null && mWFDManager != null && !mWFDManager.isShowingDevicePopup()) {
            Log.d(TAG, "[html5media] setVideoLockCtrl request : " + request);
            if (request && !mPowerKeyRequested) {
                mVideoLockCtrl.requestSystemKeyEvent(KeyEvent.KEYCODE_POWER, true);
                mPowerKeyRequested = true;
            } else if (!request && mPowerKeyRequested) {
                mVideoLockCtrl.requestSystemKeyEvent(KeyEvent.KEYCODE_POWER, false);
                mPowerKeyRequested = false;
            }
        }
    }

    // Implement SbrVideoControllerView.MediaPlayerControl
    @Override
    public boolean canPause() {
        return true;
    }
    @Override
    public boolean canSeekBackward() {
        return true;
    }
    @Override
    public boolean canSeekForward() {
        return true;
    }
    @Override
    public int getBufferPercentage() {
        return mCurrentBufferPercentage;
    }
    @Override
    public int getCurrentPosition() {
        return super.getCurrentPosition();
    }
    @Override
    public int getDuration() {
        return super.getDuration();
    }
    @Override
    public boolean isPlaying() {
        return super.isPlaying();
    }
    @Override
    public void pause() {
        super.pause();
    }
    @Override
    public void seekTo(int i) {
        super.seekTo(i);
    }
    @Override
    public void start() {
        super.start();
    }
    @Override
    public boolean isFullScreen() {
        return false;
    }
    @Override
    public void toggleFullScreen() {
    }
    @Override
    public void toggleScreenOrientation(){
        SBrowserMainActivity activity = (SBrowserMainActivity)getContext();
        if(mScreenOrientation == ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED){
            mScreenOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;
            activity.setRequestedOrientation(mScreenOrientation);
            return;
        }
        if(mScreenOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE){
            mScreenOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
            activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }
    }
    @Override
    public void setCCVisibility(boolean visible){
        super.setCCVisibility(visible);
    }
    // End SbrVideoControllerView.MediaPlayerControl
}
