package org.sec.android.app.sbrowser.firefox.controller;

import java.io.IOException;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;

import android.util.Log;
import android.view.View;
import android.widget.TextView;

import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.firefox.FxAccountAbstractSetupActivity;
import com.sec.android.app.sbrowser.firefox.ISBrowserShowRMErr;

public class SBrowserFFShowRMErr implements ISBrowserShowRMErr {
	private static final String LOG_TAG = FxAccountAbstractSetupActivity.class
			.getSimpleName();

	@Override
	public void showRemoteError(Exception e, int errorArray[],
			TextView remoteErrorTextView) {
		 if (e instanceof IOException) {
			 Log.i("SBrowserFFShowRMErr", " SBrowserFFShowRMErr No INTERNET");
			remoteErrorTextView
					.setText(errorArray[8]);
		} else if (e instanceof FxAccountClientRemoteException) {
			Log.i("SBrowserFFShowRMErr", " SBrowserFFShowRMErr custum error");
			remoteErrorTextView.setText(((FxAccountClientRemoteException) e)
					.getErrorMessageStringResource(errorArray));
		} 
		Logger.warn(LOG_TAG, "Got exception; showing error message: "
				+ remoteErrorTextView.getText().toString(), e);
		remoteErrorTextView.setVisibility(View.VISIBLE);
	}

}
