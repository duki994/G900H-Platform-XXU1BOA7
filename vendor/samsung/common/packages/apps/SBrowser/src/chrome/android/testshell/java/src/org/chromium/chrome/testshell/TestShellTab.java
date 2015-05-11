// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.chrome.browser.TabBase;
import org.chromium.chrome.browser.UrlUtilities;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.infobar.AutoLoginProcessor;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

/**
 * TestShell's implementation of a tab. This mirrors how Chrome for Android subclasses
 * and extends {@link TabBase}.
 */
public class TestShellTab extends TabBase {
    // Tab state
    private boolean mIsLoading;

    /**
     * @param context           The Context the view is running in.
     * @param url               The URL to start this tab with.
     * @param window            The WindowAndroid should represent this tab.
     * @param contentViewClient The client for the {@link ContentView}s of this Tab.
     */
    public TestShellTab(Context context, String url, WindowAndroid window,
            ContentViewClient contentViewClient) {
        super(false, context, window);
        initialize();
        initContentView();
        setContentViewClient(contentViewClient);
        loadUrlWithSanitization(url);
    }

    /**
     * @return Whether or not the tab is currently loading.
     */
    public boolean isLoading() {
        return mIsLoading;
    }

    /**
     * Navigates this Tab's {@link ContentView} to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     * @param postData Optional data to be sent via POST.
     */
    public void loadUrlWithSanitization(String url, byte[] postData) {
        if (url == null) return;

        // Sanitize the URL.
        url = UrlUtilities.fixupUrl(url);

        // Invalid URLs will just return empty.
        if (TextUtils.isEmpty(url)) return;

        ContentView contentView = getContentView();
        if (TextUtils.equals(url, contentView.getUrl())) {
            contentView.getContentViewCore().reload(true);
        } else {
            if (postData == null) {
                contentView.loadUrl(new LoadUrlParams(url));
            } else {
                contentView.loadUrl(LoadUrlParams.createLoadHttpPostParams(url, postData));
            }
        }
    }

    /**
     * Navigates this Tab's {@link ContentView} to a sanitized version of {@code url}.
     * @param url The potentially unsanitized URL to navigate to.
     */
    public void loadUrlWithSanitization(String url) {
        loadUrlWithSanitization(url, null);
    }

    @Override
    protected TabBaseChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new TestShellTabBaseChromeWebContentsDelegateAndroid();
    }

    @Override
    protected AutoLoginProcessor createAutoLoginProcessor() {
        return new AutoLoginProcessor() {
            @Override
            public void processAutoLoginResult(String accountName,
                    String authToken, boolean success, String result) {
                getInfoBarContainer().processAutoLogin(accountName, authToken,
                        success, result);
            }
        };
    }

    @Override
    protected ContextMenuPopulator createContextMenuPopulator() {
        return new ChromeContextMenuPopulator(new TabBaseChromeContextMenuItemDelegate() {
            @Override
            public void onOpenImageUrl(String url) {
                loadUrlWithSanitization(url);
            }
        });
    }

    private class TestShellTabBaseChromeWebContentsDelegateAndroid
            extends TabBaseChromeWebContentsDelegateAndroid {
        @Override
        public void onLoadStarted() {
            mIsLoading = true;
        }

        @Override
        public void onLoadStopped() {
            mIsLoading = false;
        }
    }
}
