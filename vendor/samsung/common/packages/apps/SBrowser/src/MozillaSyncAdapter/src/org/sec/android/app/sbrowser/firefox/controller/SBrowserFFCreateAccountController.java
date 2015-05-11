package org.sec.android.app.sbrowser.firefox.controller;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountAgeLockoutHelper;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.background.fxa.QuickPasswordStretcher;
import org.mozilla.gecko.fxa.activities.AddAccountDelegate;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountCreateAccountTask;

import android.app.Activity;
import android.content.Intent;
import android.os.SystemClock;
import android.view.View;
import android.widget.TextView;

import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.firefox.FxAccountAbstractSetupActivity;
import com.sec.android.app.sbrowser.firefox.FxAccountCreateAccountNotAllowedActivity;
import com.sec.android.app.sbrowser.firefox.FxAccountSignInActivity;
import com.sec.android.app.sbrowser.firefox.ISBrowserFFCreateAccount;

public class SBrowserFFCreateAccountController implements
		ISBrowserFFCreateAccount {

	public static final String DEFAULT_AUTH_SERVER_ENDPOINT = "https://api.accounts.firefox.com/v1";
	Activity mActivity;
	private static final int CHILD_REQUEST_CODE = 2;

	@Override
	public void createAccount(String email, String password,
			Map<String, Boolean> engines,
			final FxAccountAbstractSetupActivity mactivity,  final int errorArray[], final TextView remoteErrorTextView) {
		final SBrowserFFShowRMErr browserFFShowRMErr = new SBrowserFFShowRMErr();
		String serverURI = DEFAULT_AUTH_SERVER_ENDPOINT;
		PasswordStretcher passwordStretcher = new QuickPasswordStretcher(
				password);
		// This delegate creates a new Android account on success, opens the
		// appropriate "success!" activity, and finishes this activity.
		RequestDelegate<LoginResponse> delegate = new AddAccountDelegate(email,
				passwordStretcher, serverURI, engines, mactivity) {
			@Override
			public void handleError(Exception e) {
				browserFFShowRMErr.showRemoteError(e, errorArray, remoteErrorTextView);
			}

			@Override
			public void handleFailure(FxAccountClientRemoteException e) {
				showRemoteError(e, errorArray, remoteErrorTextView);
			}
		};

		Executor executor = Executors.newSingleThreadExecutor();
		FxAccountClient client = new FxAccountClient20(serverURI, executor);
		try {

			remoteErrorTextView.setVisibility(View.INVISIBLE);
			new FxAccountCreateAccountTask(mactivity, mactivity, email,
					passwordStretcher, client, delegate).execute();
		} catch (Exception e) {
			showRemoteError(e,
					null, remoteErrorTextView);
		}

	}

	public void showRemoteError(Exception e, int errorArray[],
			TextView remoteErrorTextView) {
			
		if (e instanceof IOException) {
			remoteErrorTextView
					.setText(R.string.fxaccount_remote_error_COULD_NOT_CONNECT);
		} else if (e instanceof FxAccountClientRemoteException) {
			remoteErrorTextView.setText(((FxAccountClientRemoteException) e)
					.getErrorMessageStringResource(errorArray));
		} 
		Logger.warn("SBROWSERFFCLIENTSETUP", "Got exception; showing error message: "
				+ remoteErrorTextView.getText().toString(), e);
		remoteErrorTextView.setVisibility(View.VISIBLE);
	}
	

	protected void doSigninInstead(final String email, final String password) {
		Intent intent = new Intent(mActivity, FxAccountSignInActivity.class);
		if (email != null) {
			intent.putExtra("email", email);
		}
		if (password != null) {
			intent.putExtra("password", password);
		}
		// Per http://stackoverflow.com/a/8992365, this triggers a known bug
		// with
		// the soft keyboard not being shown for the started activity. Why,
		// Android, why?
		intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		mActivity.startActivityForResult(intent, CHILD_REQUEST_CODE);
	}

	@Override
	public void handleonClickofCreatAcct(String email, String password,
			Map<String, Boolean> engines, String yearedit, String[] yearItems,
			FxAccountAbstractSetupActivity mactivity, int errorArray[], TextView rmTxt) {
		// if (!( mactivity).updateButtonState()) {
		// return;
		// }
		if (FxAccountAgeLockoutHelper.passesAgeCheck(yearedit, yearItems)) {
			createAccount(email, password, engines, mactivity, errorArray, rmTxt);
		} else {
			FxAccountAgeLockoutHelper.lockOut(SystemClock.elapsedRealtime());
			(mactivity).setResult(0);
			
			  Intent intent = new Intent(mactivity, FxAccountCreateAccountNotAllowedActivity.class);
			    intent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
			    (mactivity).startActivity(intent);
			
		}
	}
}
