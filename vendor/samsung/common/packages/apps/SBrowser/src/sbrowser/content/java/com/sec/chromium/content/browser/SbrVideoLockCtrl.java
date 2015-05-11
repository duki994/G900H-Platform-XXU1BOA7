// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.sec.chromium.content.browser;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;
import android.view.IWindowManager;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.HoverPopupWindow;
import android.widget.ImageButton;
import android.widget.RelativeLayout;

import com.sec.android.app.sbrowser.R;

public class SbrVideoLockCtrl {

    private static final String TAG = "SbrVideoLockCtrl";

    private static final int HIDE_LOCK_ICON = 0;

    private static boolean mLockMode;

    private SbrContentVideoViewNew mSbrContentVideoView;
    private Context mContext;
    private RelativeLayout mLockCtrlView;
    private ImageButton mLockBtn;
    private boolean mIsViewAdded;
    
    public SbrVideoLockCtrl(Context context) {
        mContext = context;
        mSbrContentVideoView = null;
        initLockCtrlView();
    }

    public void initLockCtrlView() {
        mLockMode = false;
        mIsViewAdded = false;
        LayoutInflater inflate = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mLockCtrlView = (RelativeLayout) inflate.inflate(R.layout.videoplayer_lock_layout, null);
        mLockBtn = (ImageButton) mLockCtrlView.findViewById(R.id.lock_btn);
        mLockBtn.setHoverPopupType(HoverPopupWindow.TYPE_TOOLTIP);
        mLockBtn.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        break;
                    case MotionEvent.ACTION_UP:
                        if (0 <= event.getX() && v.getWidth() >= event.getX()
                                && 0 <= event.getY() && v.getHeight() >= event.getY()
                                && mLockMode) {
                            toggleFullscreenLock(false);
                        }
                        break;
                    default:
                        break;
                }
                return false;
            }
        });
        mLockBtn.setVisibility(View.GONE);
        requestSystemKeyEvent(KeyEvent.KEYCODE_POWER, true);
    }

    public boolean requestSystemKeyEvent(int keyCode, boolean request) {
        if (mContext == null) {
            return false;
        }
        IWindowManager windowmanager = IWindowManager.Stub.asInterface(ServiceManager.getService(Context.WINDOW_SERVICE));
        if (null != windowmanager) {
            try {
                if (mContext instanceof Activity)
                    return windowmanager.requestSystemKeyEvent(keyCode, ((Activity) mContext).getComponentName(), request);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
        return false;
    }

    public void toggleFullscreenLock(boolean toggle) {
        Log.d(TAG, "[html5media] toggleFullscreenLock. mLockMode : " + mLockMode + "->" + !mLockMode);
        if (!mIsViewAdded || mSbrContentVideoView == null) {
            mSbrContentVideoView = (SbrContentVideoViewNew) SbrContentVideoView.getSbrContentVideoView();
            if (mSbrContentVideoView != null && mLockCtrlView != null) {
                mSbrContentVideoView.addView(mLockCtrlView);
                mIsViewAdded = true;
            }
        }
        Configuration configuration = mContext.getResources().getConfiguration();
        if (!toggle) {
            requestSystemKeyEvent(KeyEvent.KEYCODE_HOME, false);
            requestSystemKeyEvent(KeyEvent.KEYCODE_APP_SWITCH, false);
            if ((configuration.keyboard == Configuration.KEYBOARD_12KEY)
                    && (configuration.navigation == Configuration.NAVIGATION_DPAD)) {
                requestSystemKeyEvent(KeyEvent.KEYCODE_ENDCALL, false) ;
            }
            mLockMode = false;
            hideLockIcon();
            if (mSbrContentVideoView != null && mSbrContentVideoView.mMediaController != null) {
                mSbrContentVideoView.mMediaController.show();
            }
        } else {
            requestSystemKeyEvent(KeyEvent.KEYCODE_HOME, true);
            requestSystemKeyEvent(KeyEvent.KEYCODE_APP_SWITCH, true);
            if ((configuration.keyboard == Configuration.KEYBOARD_12KEY)
                    && (configuration.navigation == Configuration.NAVIGATION_DPAD)) {
                requestSystemKeyEvent(KeyEvent.KEYCODE_ENDCALL, true);
            }
            mLockMode = true;
            showLockIcon();
            if (mSbrContentVideoView != null && mSbrContentVideoView.mMediaController != null && mSbrContentVideoView.mMediaController.isShowing()) {
                mSbrContentVideoView.mMediaController.hide();
            }
        }
        if (mSbrContentVideoView != null && mSbrContentVideoView.mMediaController != null) {
            mSbrContentVideoView.mMediaController.checkShowingAndDismissPopupVolBar();
        }
    }

    public void showLockIcon() {
        if (mLockCtrlView == null || mLockCtrlView == null)
            return;
        mLockCtrlView.bringToFront();
        mLockBtn.bringToFront();
        mLockCtrlView.setVisibility(View.VISIBLE);
        mLockBtn.setVisibility(View.VISIBLE);
        mHandler.removeMessages(HIDE_LOCK_ICON);
        mHandler.sendEmptyMessageDelayed(HIDE_LOCK_ICON, 3000);
    }

    public void hideLockIcon() {
        if (mLockCtrlView == null || mLockCtrlView == null)
            return;
        mLockCtrlView.setVisibility(View.GONE);
        mLockBtn.setVisibility(View.GONE);
    }

    private final Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case HIDE_LOCK_ICON:
                    hideLockIcon();
                    break;
                default:
                    return;
            }
        }
    };

    public static boolean getLockState() {
        return mLockMode;
    }

    public void releaseLockCtrlView() {
        mHandler.removeCallbacksAndMessages(null);
        if (mLockMode) {
            toggleFullscreenLock(false);
        }
        requestSystemKeyEvent(KeyEvent.KEYCODE_POWER, false);
    }

}