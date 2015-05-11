/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.sec.chromium.content.browser;

import java.lang.ref.WeakReference;
import java.util.Formatter;
import java.util.Locale;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.BitmapDrawable;
import android.media.AudioManager;
import android.os.Handler;
import android.os.Message;
import android.provider.Settings;
import android.util.AttributeSet;
import android.util.Log;
import android.view.animation.Animation;
import android.view.animation.TranslateAnimation;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.SoundEffectConstants;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.PopupWindow;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.SBrowserMainActivity;
import com.sec.chromium.content.browser.SbrWifiDisplayManager;

/**
 * A view containing controls for a MediaPlayer. Typically contains the
 * buttons like "Play/Pause", "Rewind", "Fast Forward" and a progress
 * slider. It takes care of synchronizing the controls with the state
 * of the MediaPlayer.
 * <p>
 * The way to use this class is to instantiate it programatically.
 * The MediaController will create a default set of controls
 * and put them in a window floating above your application. Specifically,
 * the controls will float above the view specified with setAnchorView().
 * The window will disappear if left idle for three seconds and reappear
 * when the user touches the anchor view.
 * <p>
 * Functions like show() and hide() have no effect when MediaController
 * is created in an xml layout.
 * 
 * MediaController will hide and
 * show the buttons according to these rules:
 * <ul>
 * <li> The "previous" and "next" buttons are hidden until setPrevNextListeners()
 *   has been called
 * <li> The "previous" and "next" buttons are visible but disabled if
 *   setPrevNextListeners() was called with null listeners
 * <li> The "rewind" and "fastforward" buttons are shown unless requested
 *   otherwise by using the MediaController(Context, boolean) constructor
 *   with the boolean set to false
 * </ul>
 */
public class SbrVideoControllerView extends FrameLayout {
    private static final String TAG = "SbrVideoControllerView";
    
    private MediaPlayerControl      mPlayer;
    private Context                 mContext;
    private ViewGroup               mAnchor;
    private ViewGroup                    mRoot;
    private ProgressBar             mProgress;
    private TextView                mEndTime, mCurrentTime;
    private boolean                 mShowing;
    private boolean                 mDragging;
    private static final int        TRANSLATE_TIME_DEFAULT = 500;
    private static final int        TRANSLATE_TIME_FORCED = 100;
    private static final int        sDefaultTimeout = 3000;
    private static final int        FADE_OUT = 1;
    private static final int        SHOW_PROGRESS = 2;
    private boolean                 mUseFastForward;
    private boolean                 mFromXml;
    private boolean                 mListenersSet;
    private View.OnClickListener    mNextListener, mPrevListener;
    StringBuilder                   mFormatBuilder;
    Formatter                       mFormatter;
    private ImageButton             mPauseButton;
    private ImageButton             mFfwdButton;
    private ImageButton             mRewButton;
    private ImageButton             mNextButton;
    private ImageButton             mPrevButton;
    private ImageButton             mRotationBtn;
    private ImageButton             mCaptionBtn;
    private Handler                 mHandler = new MessageHandler(this);

    private boolean                 mAnimating = false;

    private SbrWifiDisplayManager   mWFDManager = null;
    
    private boolean                 mForced = false;

    private SbrVideoTitleControllerView mTitleController = null;

    // caption
    private static final int        NO_CLOSED_CAPTION = 0;
    private static final int        CLOSED_CAPTION_SHOWING = 1;
    private static final int        CLOSED_CAPTION_HIDDEN = 2;
    private int                     mCaptionStatus = NO_CLOSED_CAPTION;

    public SbrVideoControllerView(Context context) {
        super(context);
        mContext = context;
        mUseFastForward = true;
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        if (mRoot != null)
            initControllerView(mRoot);
    }
    
    public void setMediaPlayer(MediaPlayerControl player) {
        mPlayer = player;
        updatePausePlay();
    }

    public void setWFDManager(SbrWifiDisplayManager wfdManager) {
        mWFDManager = wfdManager;
    }

    /**
     * Set the view that acts as the anchor for the control view.
     * This can for example be a VideoView, or your Activity's main view.
     * @param view The view to which to anchor the controller when it is visible.
     */
    public void setAnchorView(ViewGroup view) {
        mAnchor = view;

        FrameLayout.LayoutParams frameParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );

        removeAllViews();
        View v = makeControllerView();
        addView(v, frameParams);

        mTitleController = new SbrVideoTitleControllerView();
        if (mTitleController != null) {
            mTitleController.setAnchorView();
        }
    }

    /**
     * Create the view that holds the widgets that control playback.
     * Derived classes can override this to create their own.
     * @return The controller view.
     * @hide This doesn't work as advertised
     */
    protected ViewGroup makeControllerView() {
        LayoutInflater inflate = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mRoot = (ViewGroup) inflate.inflate(R.layout.media_controller, null);

        initControllerView(mRoot);

        return mRoot;
    }

    private void initControllerView(ViewGroup v) {
        mPauseButton = (ImageButton) v.findViewById(R.id.videoplayer_btn_pause);
        if (mPauseButton != null) {
            mPauseButton.requestFocus();
            mPauseButton.setOnClickListener(mPauseListener);
        }

        mFfwdButton = (ImageButton) v.findViewById(R.id.videoplayer_btn_ff);
        if (mFfwdButton != null) {
            mFfwdButton.setOnClickListener(mFfwdListener);
            if (!mFromXml) {
                mFfwdButton.setVisibility(mUseFastForward ? View.VISIBLE : View.GONE);
            }
        }

        mRewButton = (ImageButton) v.findViewById(R.id.videoplayer_btn_rew);
        if (mRewButton != null) {
            mRewButton.setOnClickListener(mRewListener);
            if (!mFromXml) {
                mRewButton.setVisibility(mUseFastForward ? View.VISIBLE : View.GONE);
            }
        }

        // By default these are hidden. They will be enabled when setPrevNextListeners() is called 
        mNextButton = (ImageButton) v.findViewById(R.id.next);
        if (mNextButton != null && !mFromXml && !mListenersSet) {
            mNextButton.setVisibility(View.GONE);
        }
        mPrevButton = (ImageButton) v.findViewById(R.id.prev);
        if (mPrevButton != null && !mFromXml && !mListenersSet) {
            mPrevButton.setVisibility(View.GONE);
        }

        mProgress = (ProgressBar) v.findViewById(R.id.videocontroller_progress);
        if (mProgress != null) {
            if (mProgress instanceof SeekBar) {
                SeekBar seeker = (SeekBar) mProgress;
                seeker.setOnSeekBarChangeListener(mSeekListener);
            }
            mProgress.setMax(1000);
        }

        mEndTime = (TextView) v.findViewById(R.id.time_total);
        mCurrentTime = (TextView) v.findViewById(R.id.time_current);
        mFormatBuilder = new StringBuilder();
        mFormatter = new Formatter(mFormatBuilder, Locale.getDefault());

        installPrevNextListeners();

        mRotationBtn = (ImageButton)v.findViewById(R.id.rotate_ctrl_icon);
        if (mRotationBtn != null) {
            mRotationBtn.setOnTouchListener(mRotationBtnTouchListener);
            mRotationBtn.setFocusable(true);
        }

        mCaptionBtn = (ImageButton)v.findViewById(R.id.caption);
        if (mCaptionBtn != null) {
            mCaptionBtn.setOnTouchListener(mCaptionBtnTouchListener);
            mCaptionBtn.setFocusable(true);
            mCaptionBtn.setVisibility(View.GONE);
        }
    }

    /**
     * Show the controller on screen. It will go away
     * automatically after 3 seconds of inactivity.
     */
    public void show() {
        show(sDefaultTimeout);
    }

    public void showForced() {
        mForced = true;
        show(sDefaultTimeout);
    }

    /**
     * Disable pause or seek buttons if the stream cannot be paused or seeked.
     * This requires the control interface to be a MediaPlayerControlExt
     */
    private void disableUnsupportedButtons() {
        if (mPlayer == null) {
            return;
        }
        
        try {
            if (mPauseButton != null && !mPlayer.canPause()) {
                mPauseButton.setEnabled(false);
            }
            if (mRewButton != null && !mPlayer.canSeekBackward()) {
                mRewButton.setEnabled(false);
            }
            if (mFfwdButton != null && !mPlayer.canSeekForward()) {
                mFfwdButton.setEnabled(false);
            }
        } catch (IncompatibleClassChangeError ex) {
            // We were given an old version of the interface, that doesn't have
            // the canPause/canSeekXYZ methods. This is OK, it just means we
            // assume the media can be paused and seeked, and so we don't disable
            // the buttons.
        }
    }
    
    /**
     * Show the controller on screen. It will go away
     * automatically after 'timeout' milliseconds of inactivity.
     * @param timeout The timeout in milliseconds. Use 0 to show
     * the controller until hide() is called.
     */
    public void show(int timeout) {
        if(!mForced && mAnimating) return;

        if (!mShowing && mAnchor != null) {
            setProgress();
            updateAutoRotationBtn();

            if (mPauseButton != null) {
                mPauseButton.requestFocus();
            }
            disableUnsupportedButtons();

            FrameLayout.LayoutParams tlp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM
            );

            RelativeLayout.LayoutParams rlp = new RelativeLayout.LayoutParams(
                android.view.ViewGroup.LayoutParams.MATCH_PARENT,
                android.view.ViewGroup.LayoutParams.WRAP_CONTENT
            );
            rlp.addRule(RelativeLayout.ALIGN_PARENT_TOP);
            
            this.setVisibility(View.INVISIBLE);
            mTitleController.setVisibility(View.INVISIBLE);
            mAnchor.addView(this, tlp);
            mAnchor.addView(mTitleController, rlp);
            mTitleController.checkShowingAndDismissPopupVolBar();
            mTitleController.updateVolume();

            if (timeout != 0) {
                Animation translateOn, titleTranslateOn = null;
                translateOn = new TranslateAnimation(0, 0, mRoot.findViewById(R.id.controller_layout).getHeight(), 0);
                titleTranslateOn = new TranslateAnimation(0, 0, -(mTitleController.findViewById(R.id.first_layout).getHeight()), 0);

                if (mForced) {
                    translateOn.setDuration(TRANSLATE_TIME_FORCED);
                    titleTranslateOn.setDuration(TRANSLATE_TIME_FORCED);
                } else {
                    translateOn.setDuration(TRANSLATE_TIME_DEFAULT);
                    titleTranslateOn.setDuration(TRANSLATE_TIME_DEFAULT);
                }

                translateOn.startNow();
                titleTranslateOn.startNow();
                mTitleController.setAnimation(titleTranslateOn);
                this.setAnimation(translateOn);

                translateOn.setAnimationListener(new Animation.AnimationListener() {
                    public void onAnimationStart(Animation arg0) {
                        mAnimating = true;
                    }
                    public void onAnimationRepeat(Animation arg0) {
                    }
                    public void onAnimationEnd(Animation arg0) {
                        mAnimating = false;
                    }
                });
            }
            this.setVisibility(View.VISIBLE);
            mTitleController.setVisibility(View.VISIBLE);
            if (mForced) {
                mForced = false;
            }
            mShowing = true;
        }
        updatePausePlay();
        
        // cause the progress bar to be updated even if mShowing
        // was already true.  This happens, for example, if we're
        // paused with the progress bar showing the user hits play.
        mHandler.sendEmptyMessage(SHOW_PROGRESS);

        Message msg = mHandler.obtainMessage(FADE_OUT);
        if (timeout != 0) {
            mHandler.removeMessages(FADE_OUT);
            mHandler.sendMessageDelayed(msg, timeout);
        }
    }

    public boolean isShowing() {
        return mShowing;
    }

    public boolean isAnimating() {
        return mAnimating;
    }

    /**
     * Remove the controller from the screen.
     */
    public void hideForced() {
        mForced = true;
        hide();
    }

    public void hide() {
        if (mAnchor == null || (!mForced && mAnimating)) {
            return;
        }
        mHandler.removeMessages(FADE_OUT);
        try {
            if (mShowing) {
                if (!mForced) {
                    Animation translateOff, titleTranslateOff = null;
                    translateOff = new TranslateAnimation(0, 0, 0, mRoot.findViewById(R.id.controller_layout).getHeight());
                    titleTranslateOff = new TranslateAnimation(0, 0, 0, -(mTitleController.findViewById(R.id.first_layout).getHeight()));

                    translateOff.setDuration(TRANSLATE_TIME_DEFAULT);
                    titleTranslateOff.setDuration(TRANSLATE_TIME_DEFAULT);

                    translateOff.startNow();
                    titleTranslateOff.startNow();
                    this.setAnimation(translateOff);
                    mTitleController.setAnimation(titleTranslateOff);

                    translateOff.setAnimationListener(new Animation.AnimationListener() {
                        public void onAnimationStart(Animation arg0) {
                            mAnimating = true;
                        }
                        public void onAnimationRepeat(Animation arg0) {
                        }
                        public void onAnimationEnd(Animation arg0) {
                            mAnimating = false;
                        }
                    });
                }
                this.setVisibility(View.GONE);
                mTitleController.setVisibility(View.GONE);
                mAnchor.removeView(this);
                mAnchor.removeView(mTitleController);

                mHandler.removeMessages(SHOW_PROGRESS);
            }
        } catch (IllegalArgumentException ex) {
            Log.w("MediaController", "already removed");
        }
        if (mForced) {
            mForced = false;
        }
        mShowing = false;
    }

    private String stringForTime(int timeMs) {
        int totalSeconds = timeMs / 1000;

        int seconds = totalSeconds % 60;
        int minutes = (totalSeconds / 60) % 60;
        int hours   = totalSeconds / 3600;

        mFormatBuilder.setLength(0);
        if (hours > 0) {
            return mFormatter.format("%d:%02d:%02d", hours, minutes, seconds).toString();
        } else {
            return mFormatter.format("%02d:%02d", minutes, seconds).toString();
        }
    }

    private int setProgress() {
        if (mPlayer == null || mDragging || (SbrContentVideoView.getCurrentState() == 3)) {
            return 0;
        }

        int position = mPlayer.getCurrentPosition();
        int duration = mPlayer.getDuration();
        if (mProgress != null) {
            if (duration > 0) {
                // use long to avoid overflow
                long pos = 1000L * position / duration;
                mProgress.setProgress( (int) pos);
            }
            int percent = mPlayer.getBufferPercentage();
            mProgress.setSecondaryProgress(percent * 10);
        }

        if (mEndTime != null)
            mEndTime.setText(stringForTime(duration));
        if (mCurrentTime != null)
            mCurrentTime.setText(stringForTime(position));

        return position;
    }

    protected void setCurrentTimeToMax() {
        if (mPlayer == null || mDragging) {
            return;
        }
        int duration = mPlayer.getDuration();
        if (mProgress != null) {
            if (duration > 0) {
                mProgress.setProgress(1000);
                if (mCurrentTime != null)
                    mCurrentTime.setText(stringForTime(duration));
            }
        }
        updatePausePlay();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        show(sDefaultTimeout);
        return true;
    }

    @Override
    public boolean onTrackballEvent(MotionEvent ev) {
        show(sDefaultTimeout);
        return false;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mPlayer == null) {
            return true;
        }
        
        int keyCode = event.getKeyCode();
        final boolean uniqueDown = event.getRepeatCount() == 0
                && event.getAction() == KeyEvent.ACTION_DOWN;

        Log.d(TAG,"[html5media] dispatchKeyEvent. " + keyCode);

        if (keyCode ==  KeyEvent.KEYCODE_HEADSETHOOK
                || keyCode == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                || keyCode == KeyEvent.KEYCODE_SPACE) {
            if (uniqueDown) {
                doPauseResume();
                show(sDefaultTimeout);
                if (mPauseButton != null) {
                    mPauseButton.requestFocus();
                }
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MEDIA_PLAY) {
            if (uniqueDown && !mPlayer.isPlaying()) {
                mPlayer.start();
                updatePausePlay();
                show(sDefaultTimeout);
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MEDIA_STOP
                || keyCode == KeyEvent.KEYCODE_MEDIA_PAUSE) {
            if (uniqueDown && mPlayer.isPlaying()) {
                mPlayer.pause();
                updatePausePlay();
                show(sDefaultTimeout);
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP
                || keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) {
            // don't show the controls for volume adjustment
            return super.dispatchKeyEvent(event);
        } else if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_MENU) {
            if (uniqueDown) {
                hide();
            }
            return true;
        }

        show(sDefaultTimeout);
        return super.dispatchKeyEvent(event);
    }

    private View.OnClickListener mPauseListener = new View.OnClickListener() {
        public void onClick(View v) {
            doPauseResume();
            show(sDefaultTimeout);
        }
    };

    public void updatePausePlay() {
        if (mRoot == null || mPauseButton == null || mPlayer == null) {
            return;
        }

        if (mPlayer.isPlaying()) {
            mPauseButton.setBackgroundResource(R.drawable.btn_control_pause);
        } else {
            mPauseButton.setBackgroundResource(R.drawable.btn_control_play);
        }
    }

    private void doPauseResume() {
        if (mPlayer == null) {
            return;
        }
        
        if (mPlayer.isPlaying()) {
            mPlayer.pause();
        } else {
            mPlayer.start();
        }
        updatePausePlay();
    }

    private void doToggleFullscreen() {
        if (mPlayer == null) {
            return;
        }
        
        mPlayer.toggleFullScreen();
    }

    // There are two scenarios that can trigger the seekbar listener to trigger:
    //
    // The first is the user using the touchpad to adjust the posititon of the
    // seekbar's thumb. In this case onStartTrackingTouch is called followed by
    // a number of onProgressChanged notifications, concluded by onStopTrackingTouch.
    // We're setting the field "mDragging" to true for the duration of the dragging
    // session to avoid jumps in the position in case of ongoing playback.
    //
    // The second scenario involves the user operating the scroll ball, in this
    // case there WON'T BE onStartTrackingTouch/onStopTrackingTouch notifications,
    // we will simply apply the updated position without suspending regular updates.
    private OnSeekBarChangeListener mSeekListener = new OnSeekBarChangeListener() {
        public void onStartTrackingTouch(SeekBar bar) {
            show(3600000);

            mDragging = true;

            // By removing these pending progress messages we make sure
            // that a) we won't update the progress while the user adjusts
            // the seekbar and b) once the user is done dragging the thumb
            // we will post one of these messages to the queue again and
            // this ensures that there will be exactly one message queued up.
            mHandler.removeMessages(SHOW_PROGRESS);
        }

        public void onProgressChanged(SeekBar bar, int progress, boolean fromuser) {
            if (mPlayer == null) {
                return;
            }
            
            if (!fromuser) {
                // We're not interested in programmatically generated changes to
                // the progress bar's position.
                return;
            }

            long duration = mPlayer.getDuration();
            long newposition = (duration * progress) / 1000L;
            mPlayer.seekTo( (int) newposition);
            if (mCurrentTime != null)
                mCurrentTime.setText(stringForTime( (int) newposition));
        }

        public void onStopTrackingTouch(SeekBar bar) {
            mDragging = false;
            setProgress();
            updatePausePlay();
            show(sDefaultTimeout);

            // Ensure that progress is properly updated in the future,
            // the call to show() does not guarantee this because it is a
            // no-op if we are already showing.
            mHandler.sendEmptyMessage(SHOW_PROGRESS);
        }
    };

    @Override
    public void setEnabled(boolean enabled) {
        if (mPauseButton != null) {
            mPauseButton.setEnabled(enabled);
        }
        if (mFfwdButton != null) {
            mFfwdButton.setEnabled(enabled);
        }
        if (mRewButton != null) {
            mRewButton.setEnabled(enabled);
        }
        if (mNextButton != null) {
            mNextButton.setEnabled(enabled && mNextListener != null);
        }
        if (mPrevButton != null) {
            mPrevButton.setEnabled(enabled && mPrevListener != null);
        }
        if (mProgress != null) {
            mProgress.setEnabled(enabled);
        }
        disableUnsupportedButtons();
        super.setEnabled(enabled);
    }

    private View.OnClickListener mRewListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mPlayer == null) {
                return;
            }
            
            int pos = mPlayer.getCurrentPosition();
            pos -= 5000; // milliseconds
            mPlayer.seekTo(pos);
            setProgress();

            show(sDefaultTimeout);
        }
    };

    private View.OnClickListener mFfwdListener = new View.OnClickListener() {
        public void onClick(View v) {
            if (mPlayer == null) {
                return;
            }
            
            int pos = mPlayer.getCurrentPosition();
            pos += 15000; // milliseconds
            mPlayer.seekTo(pos);
            setProgress();

            show(sDefaultTimeout);
        }
    };

    private void installPrevNextListeners() {
        if (mNextButton != null) {
            mNextButton.setOnClickListener(mNextListener);
            mNextButton.setEnabled(mNextListener != null);
        }

        if (mPrevButton != null) {
            mPrevButton.setOnClickListener(mPrevListener);
            mPrevButton.setEnabled(mPrevListener != null);
        }
    }

    public void setPrevNextListeners(View.OnClickListener next, View.OnClickListener prev) {
        mNextListener = next;
        mPrevListener = prev;
        mListenersSet = true;

        if (mRoot != null) {
            installPrevNextListeners();
            
            if (mNextButton != null && !mFromXml) {
                mNextButton.setVisibility(View.VISIBLE);
            }
            if (mPrevButton != null && !mFromXml) {
                mPrevButton.setVisibility(View.VISIBLE);
            }
        }
    }
    
    public interface MediaPlayerControl {
        void    start();
        void    pause();
        int     getDuration();
        int     getCurrentPosition();
        void    seekTo(int pos);
        boolean isPlaying();
        int     getBufferPercentage();
        boolean canPause();
        boolean canSeekBackward();
        boolean canSeekForward();
        boolean isFullScreen();
        void    toggleFullScreen();
        void    toggleScreenOrientation();
        void    setCCVisibility(boolean visible);
    }

    private View.OnTouchListener mCaptionBtnTouchListener = new View.OnTouchListener() {
        public boolean onTouch(View view, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_UP:
                    Log.d(TAG, "[html5media] Caption button clicked.");
                    view.playSoundEffect(SoundEffectConstants.CLICK);

                    if (mCaptionStatus == CLOSED_CAPTION_SHOWING) {
                        mPlayer.setCCVisibility(false);
                        mCaptionStatus = CLOSED_CAPTION_HIDDEN;
                    } else if (mCaptionStatus == CLOSED_CAPTION_HIDDEN) {
                        mPlayer.setCCVisibility(true);
                        mCaptionStatus = CLOSED_CAPTION_SHOWING;
                    }
                    updateClosedCaptionBtn(mCaptionStatus);
                    break;

                default:
                    break;
            }
            return false;
        }
    };

    private View.OnTouchListener mRotationBtnTouchListener = new View.OnTouchListener() {
        public boolean onTouch(View view, MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_UP:
                    Log.d(TAG, "[html5media] rotation btn clicked.");
                    mPlayer.toggleScreenOrientation();
                    break;
                default:
                    break;
            }
            return false;
        }
    };
    
    private static class MessageHandler extends Handler {
        private final WeakReference<SbrVideoControllerView> mView; 

        MessageHandler(SbrVideoControllerView view) {
            mView = new WeakReference<SbrVideoControllerView>(view);
        }
        @Override
        public void handleMessage(Message msg) {
            SbrVideoControllerView view = mView.get();
            if (view == null || view.mPlayer == null) {
                return;
            }
            
            int pos;
            switch (msg.what) {
                case FADE_OUT:
                    view.hide();
                    break;
                case SHOW_PROGRESS:
                    pos = view.setProgress();
                    if (!view.mDragging && view.mShowing && view.mPlayer.isPlaying()) {
                        msg = obtainMessage(SHOW_PROGRESS);
                        sendMessageDelayed(msg, 1000 - (pos % 1000));
                    }
                    break;
            }
        }
    }

    public boolean isAutoRotation() {
        return  (mContext != null) && (Settings.System.getInt(mContext.getContentResolver(), Settings.System.ACCELEROMETER_ROTATION, 0) == 1);
    }

    public boolean isMultiWindowRunning(){
        SBrowserMainActivity activity = (SBrowserMainActivity)mContext;
        if(activity != null && activity.getController() != null)
            return activity.getController().isMultiWindowRunning();

        return false;
    }

    public void updateAutoRotationBtn() {
        if (mRotationBtn != null) {
            if (isAutoRotation() || isMultiWindowRunning())
                mRotationBtn.setVisibility(View.GONE);
            else
                mRotationBtn.setVisibility(View.VISIBLE);
        }
    }

    public boolean keyVolumeUp() {
        if (mTitleController != null && mContext != null) {
            if (mTitleController.isAllSoundOff(mContext)) {
                mTitleController.volumeSame(); // just to create system warning toast popup
                return false;
            } else {
                mTitleController.volumeUp();
            }
        }
        return true;
    }

    public boolean keyVolumeDown() {
        if (mTitleController != null && mContext != null) {
            if (mTitleController.isAllSoundOff(mContext)) {
                mTitleController.volumeSame(); // just to create system warning toast popup
                return false;
            } else {
                mTitleController.volumeDown();
            }
        }
        return true;
    }

    public void showPopupVolbar() {
        if (mTitleController != null) {
            mTitleController.showPopupVolbar(SbrVideoTitleControllerView.POPUP_VOLUMEBAR_UNFOCUSABLE);
            mTitleController.setVolumeSeekbarLevel();
            mTitleController.hideVolumeBarPopup();
        }
    }

    public void checkShowingAndDismissPopupVolBar() {
        if (mTitleController != null)
            mTitleController.checkShowingAndDismissPopupVolBar();
    }

    public void releaseView() {
        if (mTitleController != null)
            mTitleController.releaseView();
    }

    public void updateClosedCaptionBtn(int status){
        if(mCaptionBtn == null)
            return;

        mCaptionStatus = status;
        switch(status){
            case NO_CLOSED_CAPTION:
                mCaptionBtn.setVisibility(View.GONE);
                break;
            case CLOSED_CAPTION_SHOWING:
                mCaptionBtn.setVisibility(View.VISIBLE);
                mCaptionBtn.setImageResource(R.drawable.internet_video_ic_cc_on);
                break;
            case CLOSED_CAPTION_HIDDEN:
                mCaptionBtn.setVisibility(View.VISIBLE);
                mCaptionBtn.setImageResource(R.drawable.internet_video_ic_cc_normal);
                break;
            default:
                break;
        }
    }

    private class SbrVideoTitleControllerView extends RelativeLayout {
        private static final String TAG = "SbrVideoTitleControllerView";

        private static final boolean POPUP_VOLUMEBAR_FOCUSABLE = true;
        private static final boolean POPUP_VOLUMEBAR_UNFOCUSABLE = false;
        private static final long KEY_LONG_PRESS_TIME = 500L;
        private static final int FADE_OUT_VOLUME_BAR_DELAY = 2000;
        private static final int FADE_OUT_VOLUME_BAR = 0;

        private View mRoot;
        private View mVolPopupLayout = null;
        private AudioManager mAudioManager = null;
        private ImageButton mWfdBtn = null;
        private ImageButton mVolumeBtn = null;
        private RelativeLayout mTitleLayout = null;
        private TextView mVolumeTextPopup = null;
        private SeekBar mVolumeSeekBarPopup = null;
        private PopupWindow mPopupVolBar = null;

        private Handler mHandler = new Handler() {
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case FADE_OUT_VOLUME_BAR:
                        checkShowingAndDismissPopupVolBar();
                        break;
                    default:
                        break;
                }
            }
        };

        private SbrVideoTitleControllerView() {
            super(mContext);
            mRoot = this;
            mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        }

        private void setAnchorView() {
            Log.d(TAG, "[html5media] setAnchorView");
            removeAllViews();

            RelativeLayout.LayoutParams rp = new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            View v = makeControllerView();
            addView(v, rp);

            Configuration config = mContext.getResources().getConfiguration();
            if ((config.keyboard == Configuration.KEYBOARD_12KEY) &&
                    (config.navigation == Configuration.NAVIGATION_DPAD)) {
                v.requestFocus();
            }
        }

        private View makeControllerView() {
            LayoutInflater inflate = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            mRoot = inflate.inflate(R.layout.videoplayer_title_layout, null);
            initControllerView(mRoot);
            return mRoot;
        }

        private void initControllerView(View v) {
            mTitleLayout = (RelativeLayout) v.findViewById(R.id.first_layout);

            RelativeLayout viewGroup = (RelativeLayout) findViewById(R.id.popup);
            LayoutInflater layoutInflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            mVolPopupLayout = layoutInflater.inflate(R.layout.vol_popup, viewGroup);

            if (mPopupVolBar == null) {
                mPopupVolBar = new PopupWindow(mContext);
            } else {
                checkShowingAndDismissPopupVolBar();
            }

            mWfdBtn = (ImageButton)v.findViewById(R.id.display_change_button);
            mVolumeBtn = (ImageButton) v.findViewById(R.id.volume_btn);
            mVolumeTextPopup = (TextView) mVolPopupLayout.findViewById(R.id.vol_text_popup);
            mVolumeSeekBarPopup = (SeekBar) mVolPopupLayout.findViewById(R.id.vol_seekbar_popup);

            if (mWfdBtn != null) {
                mWfdBtn.setOnTouchListener(mWfdBtnTouchListener);
                mWfdBtn.setFocusable(true);
            }

            if (mVolumeBtn != null) {
                mVolumeBtn.setOnTouchListener(mVolumeBtnTouchListener);
                if (getCurrentVolume() == 0) {
                    mVolumeBtn.setImageResource(R.drawable.video_control_vol_mute_icon_n);
                } else {
                    mVolumeBtn.setImageResource(R.drawable.video_control_vol_icon_n);
                }
                mVolumeBtn.setFocusable(true);
                mVolumeBtn.setOnKeyListener(mVolumeBtnKeyListener);
            }

            if (mVolumeSeekBarPopup != null) {
                mVolumeSeekBarPopup.setMode(ProgressBar.MODE_VERTICAL); // Using vertical seek bar
                mVolumeSeekBarPopup.setOnSeekBarChangeListener(mVolumeSeekBarChangeListenerpopup);
                mVolumeSeekBarPopup.setOnTouchListener(mVolumeSeekBarTouchListener);
                mVolumeSeekBarPopup.setMax(mAudioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC));
            }
        }

        @Override
        public void onFinishInflate() {
            super.onFinishInflate();
            if (mRoot != null)
                initControllerView(mRoot);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            show(sDefaultTimeout);
            return true;
        }
        
        private View.OnTouchListener mWfdBtnTouchListener = new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_UP:
                        Log.d(TAG, "[html5media] WFD button clicked.");

                        view.playSoundEffect(SoundEffectConstants.CLICK);
                        boolean wasPlaying = mPlayer.isPlaying();
                        if(wasPlaying) {
                            mPlayer.pause();
                            updatePausePlay();
                        }

                        mWFDManager.scanWifiDisplays();
                        mWFDManager.showDevicePopup(wasPlaying);
                        
                        break;
                    default:
                        break;
                }
                return false;
            }
        };

        private View.OnTouchListener mVolumeBtnTouchListener = new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (mRoot != null) {
                    switch (event.getAction()) {
                        case MotionEvent.ACTION_DOWN:
                            show(sDefaultTimeout);
                            break;
                        case MotionEvent.ACTION_UP:
                            if (0 <= event.getX() && view.getWidth() >= event.getX() && 0 <= event.getY() && view.getHeight() >= event.getY()) {
                                long pressTime = event.getEventTime() - event.getDownTime();
                                if (pressTime < KEY_LONG_PRESS_TIME) {
                                    view.playSoundEffect(SoundEffectConstants.CLICK);
                                    show(sDefaultTimeout);
                                    if (!checkShowingAndDismissPopupVolBar()) {
                                        if (isAllSoundOff(mContext)) {
                                            volumeSame(); // just to create system warning toast popup
                                            break;
                                        } else {
                                            showPopupVolbar(POPUP_VOLUMEBAR_UNFOCUSABLE);
                                            setVolumeSeekbarLevel();
                                            hideVolumeBarPopup();
                                        }
                                    }
                                }
                            }
                            break;
                        case MotionEvent.ACTION_OUTSIDE:
                            show(sDefaultTimeout);
                            break;
                        default:
                            break;
                    }
                }
                return false;
            }
        };

        private View.OnKeyListener mVolumeBtnKeyListener = new View.OnKeyListener() {
            public boolean onKey(View view, int keyCode, KeyEvent event) {
                boolean retVal = false;
                if (mRoot != null) {
                    switch (keyCode) {
                        case KeyEvent.KEYCODE_ENTER:
                        case KeyEvent.KEYCODE_DPAD_CENTER:
                            switch (event.getAction()) {
                                case KeyEvent.ACTION_DOWN:
                                    show(sDefaultTimeout);
                                    break;
                                case KeyEvent.ACTION_UP:
                                    view.playSoundEffect(SoundEffectConstants.CLICK);
                                    show(sDefaultTimeout);
                                    if (!checkShowingAndDismissPopupVolBar()) {
                                        showPopupVolbar(POPUP_VOLUMEBAR_FOCUSABLE);
                                        setVolumeSeekbarLevel();
                                        hideVolumeBarPopup();
                                        mVolumeSeekBarPopup.setFocusable(true);
                                        mVolumeSeekBarPopup.requestFocus();
                                    }
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            switch (event.getAction()) {
                                case KeyEvent.ACTION_DOWN:
                                    break;
                                case KeyEvent.ACTION_UP:
                                    break;
                                default:
                                    break;
                            }
                            break;
                    }
                }
                return retVal;
            }
        };

        private SeekBar.OnSeekBarChangeListener mVolumeSeekBarChangeListenerpopup = new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromTouch) {
                if (fromTouch) {
                    setVolume(progress);
                    int vol = getCurrentVolume();
                    if (progress != vol) mVolumeSeekBarPopup.setProgress(vol);
                }
                setVolumeBtnPopup();
                mHandler.removeMessages(FADE_OUT_VOLUME_BAR);
            }
            @Override
            public void onStartTrackingTouch(SeekBar arg0) {
            }
            @Override
            public void onStopTrackingTouch(SeekBar arg0) {
                hideVolumeBarPopup();
            }
        };

        private View.OnTouchListener mVolumeSeekBarTouchListener = new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (SbrVideoLockCtrl.getLockState())
                    return true;
                return false;
            }
        };

        private boolean checkShowingAndDismissPopupVolBar() {
            if (mPopupVolBar != null && mPopupVolBar.isShowing()) {
                try {
                    mPopupVolBar.dismiss();
                    if (mVolumeSeekBarPopup != null && mVolumeSeekBarPopup.isPressed())
                        mVolumeSeekBarPopup.setPressed(false);
                } catch (IllegalStateException e) {
                    e.printStackTrace();
                } catch (IllegalArgumentException e) {
                    e.printStackTrace();
                }
                return true;
            }
            return false;
        }

        private void setVolumeBtnPopup() {
            StringBuilder mFormatBuilder = new StringBuilder();
            Formatter mFormatter = new Formatter(mFormatBuilder, Locale.getDefault());
            int vol = getCurrentVolume();
            
            if (vol == 0) {
                mVolumeBtn.setImageResource(R.drawable.video_control_vol_mute_icon_n);
            } else {
                mVolumeBtn.setImageResource(R.drawable.video_control_vol_icon_n);
            }

            mFormatBuilder.setLength(0);
            mVolumeTextPopup.setText(mFormatter.format("%d", vol).toString());
            mFormatter.close();
        }

        private void showPopupVolbar(boolean focused_state) {
            if (mPopupVolBar != null) {
                mPopupVolBar.setContentView(mVolPopupLayout);
                mPopupVolBar.setAnimationStyle(android.R.style.Animation_Dialog);

                RelativeLayout.LayoutParams paramVolumePopup = (LayoutParams) mVolPopupLayout.findViewById(R.id.ctrl_vol_vertical_popup).getLayoutParams();
                RelativeLayout.LayoutParams paramVolumeSeekbar = (LayoutParams) mVolPopupLayout.findViewById(R.id.ctrl_vol_seekbar_layout_popup).getLayoutParams();
                RelativeLayout.LayoutParams paramVolumetext = (LayoutParams) mVolPopupLayout.findViewById(R.id.ctrl_vol_text_layout2).getLayoutParams();

                paramVolumePopup.height = getResources().getDimensionPixelSize(R.dimen.vp_ctrl_vol_vertical_popup_height);
                paramVolumeSeekbar.height = getResources().getDimensionPixelSize(R.dimen.vp_ctrl_vol_seekbar_layout_popup_height);
                paramVolumeSeekbar.topMargin = getResources().getDimensionPixelSize(R.dimen.vp_ctrl_vol_seekbar_layout_popup_marginTop);
                paramVolumeSeekbar.bottomMargin = getResources().getDimensionPixelSize(R.dimen.vp_ctrl_vol_seekbar_layout_popup_marginBottom);
                paramVolumetext.bottomMargin=getResources().getDimensionPixelSize(R.dimen.vp_ctrl_vol_text_layout2_layout_marginBottom);
                mVolPopupLayout.findViewById(R.id.ctrl_vol_vertical_popup).setLayoutParams(paramVolumePopup);
                mVolPopupLayout.findViewById(R.id.ctrl_vol_seekbar_layout_popup).setLayoutParams(paramVolumeSeekbar);
                mVolPopupLayout.findViewById(R.id.ctrl_vol_text_layout2).setLayoutParams(paramVolumetext);

                mPopupVolBar.setWidth(paramVolumePopup.width);
                mPopupVolBar.setHeight(paramVolumePopup.height);
                mPopupVolBar.setBackgroundDrawable(new BitmapDrawable());
                mPopupVolBar.setFocusable(focused_state);
                if (mVolumeSeekBarPopup != null) {
                    mVolumeSeekBarPopup.setFocusable(true);
                }

                mPopupVolBar.setOnDismissListener(new PopupWindow.OnDismissListener() {
                    @Override
                    public void onDismiss() {
                        if (mVolumeSeekBarPopup != null) {
                            mVolumeSeekBarPopup.setFocusable(false);
                        }
                    }
                });

                setVolumeBtnPopup();

                int gravity = Gravity.RIGHT | Gravity.TOP;
                int margin_Y = mTitleLayout.getHeight() + (int) getResources().getDimension(R.dimen.vp_ctrl_vol_vertical_popup_maringTop);
                int margin_Right = 0;
                mPopupVolBar.showAtLocation(mVolPopupLayout, gravity, margin_Right, margin_Y);
            }
        }

        private void setVolumeSeekbarLevel() {
            if (isAllSoundOff(mContext)) {
                return;
            }
            if (mVolumeSeekBarPopup != null) {
                mVolumeSeekBarPopup.setProgress(getCurrentVolume());
            }
        }

        private void hideVolumeBarPopup() {
            long delay = FADE_OUT_VOLUME_BAR_DELAY;
            mHandler.removeMessages(FADE_OUT_VOLUME_BAR);
            mHandler.sendEmptyMessageDelayed(FADE_OUT_VOLUME_BAR, delay);
        }

        private void updateVolume() {
            setVolumeBtnPopup();
            setVolumeSeekbarLevel();
            hideVolumeBarPopup();
        }

        private int getCurrentVolume() {
            return mAudioManager != null ? mAudioManager.getStreamVolume(AudioManager.STREAM_MUSIC) : -1;
        }

        private boolean isAllSoundOff(Context context) {
            return (Settings.System.getInt(context.getContentResolver(), "all_sound_off", 0) == 1);
        }

        private void setVolume(int level) {
            if (mAudioManager != null)
                mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, level, 0);
        }

        private void volumeUp() {
            if (mAudioManager != null)
                mAudioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_RAISE, 0);
        }

        private void volumeDown() {
            if (mAudioManager != null)
                mAudioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_LOWER, 0);
        }

        private void volumeSame() {
            if (mAudioManager != null)
                mAudioManager.adjustStreamVolume(AudioManager.STREAM_MUSIC, AudioManager.ADJUST_SAME, 0);
        }

        private void releaseView() {
            Log.d(TAG, "[html5media] releaseView");
            checkShowingAndDismissPopupVolBar();
            mHandler.removeCallbacksAndMessages(null);
        }
    }

}
