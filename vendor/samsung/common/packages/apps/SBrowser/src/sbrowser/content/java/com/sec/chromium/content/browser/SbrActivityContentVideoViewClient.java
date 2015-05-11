// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.sec.chromium.content.browser;

import android.app.Activity;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;

import com.sec.android.app.sbrowser.SBrowserMainActivity;

import org.chromium.content.browser.ContentVideoViewClient;

/**
 * Uses an existing Activity to handle displaying video in full screen.
 */
public class SbrActivityContentVideoViewClient implements ContentVideoViewClient {
    private final Activity mActivity;
    private View mView;

    public SbrActivityContentVideoViewClient(Activity activity)  {
        this.mActivity = activity;
    }

    @Override
    public void onShowCustomView(View view) {
        mActivity.getWindow().addContentView(view,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        Gravity.CENTER));
        mView = view;
    }

    @Override
    public void onDestroyContentVideoView() {
        FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
        decor.removeView(mView);
        mView = null;
    }

    @Override
    public View getVideoLoadingProgressView() {
        return null;
    }

    public void setFullscreen(boolean force) {
        if (force) {
            mActivity.getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_FULLSCREEN,
                    WindowManager.LayoutParams.FLAG_FULLSCREEN);
        } else {
            if (!(mActivity instanceof SBrowserMainActivity) ||
                    !((SBrowserMainActivity)mActivity).getController().getSettings().useFullscreen()) {
                try {
                    mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                } catch (Exception e) {
                    e.printStackTrace();
                    Log.i("SbrActivityContentVideoViewClient", "exception on setFullscreen");
                }
            }
        }
    }
}
