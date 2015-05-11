// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.DialogInterface;
import android.os.Bundle;

import org.chromium.chrome.R;

/**
 * Form resubmission warning dialog. Presents the cancel/continue choice and fires one of two
 * callbacks accordingly.
 */
public class RepostFormWarningDialog extends DialogFragment {
    // Warning dialog currently being shown, stored for testing.
    private static Dialog sCurrentDialog;

    private Runnable mCancelCallback;
    private Runnable mContinueCallback;

	public RepostFormWarningDialog() {
		super();
	}
	
    public RepostFormWarningDialog(Runnable cancelCallback, Runnable continueCallback) {
        mCancelCallback = cancelCallback;
        mContinueCallback = continueCallback;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity())
                .setMessage(R.string.http_post_warning)
                .setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        if (mContinueCallback != null) {
                            mCancelCallback.run();
                        }
                    }
                })
                .setPositiveButton(R.string.http_post_warning_resend,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                if (mContinueCallback != null) {
                                    mContinueCallback.run();
                                }
                            }
                        });

        assert getCurrentDialog() == null;
        Dialog dialog = builder.create();
        setCurrentDialog(dialog);

        return dialog;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        setCurrentDialog(null);
    }

    /**
     * Sets the currently displayed dialog in sCurrentDialog. This is required by findbugs, which
     * allows static fields only to be set from static methods.
     */
    private static void setCurrentDialog(Dialog dialog) {
        sCurrentDialog = dialog;
    }

    /**
     * @return dialog currently being displayed.
     */
    public static Dialog getCurrentDialog() {
        return sCurrentDialog;
    }
}
