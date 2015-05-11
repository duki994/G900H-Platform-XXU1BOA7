package com.sec.android.app.sbrowser.firefox;

import android.app.Activity;
import android.widget.TextView;

public interface ISBrowserFFSignIn {

	void signInInterface(String email, String password, Activity activity, TextView remoteErrorTextView, int errorArray[]);
}
