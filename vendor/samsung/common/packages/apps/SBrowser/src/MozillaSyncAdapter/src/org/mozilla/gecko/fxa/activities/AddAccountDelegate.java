package org.mozilla.gecko.fxa.activities;

import java.util.Map;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.setup.Constants;

import com.sec.android.app.sbrowser.firefox.FxAccountAbstractSetupActivity;
import com.sec.android.app.sbrowser.firefox.FxAccountConfirmAccountActivity;
import com.sec.android.app.sbrowser.firefox.FxAccountVerifiedAccountActivity;

import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;

public abstract class AddAccountDelegate implements
		RequestDelegate<LoginResponse> {
	public final String email;
	public final PasswordStretcher passwordStretcher;
	public final String serverURI;
	public final Map<String, Boolean> selectedEngines;
	private static final String LOG_TAG = FxAccountAbstractSetupActivity.class
			.getSimpleName();
	Context mContext = null;
	Activity mActivity = null;

	public AddAccountDelegate(String email,
			PasswordStretcher passwordStretcher, String serverURI,
			Map<String, Boolean> engines, Activity activity) {
		this(email, passwordStretcher, serverURI, engines);
		mActivity = activity;
	}

	public AddAccountDelegate(String email,
			PasswordStretcher passwordStretcher, String serverURI,
			Map<String, Boolean> selectedEngines) {
		if (email == null) {
			throw new IllegalArgumentException("email must not be null");
		}
		if (passwordStretcher == null) {
			throw new IllegalArgumentException(
					"passwordStretcher must not be null");
		}
		if (serverURI == null) {
			throw new IllegalArgumentException("serverURI must not be null");
		}
		this.email = email;
		this.passwordStretcher = passwordStretcher;
		this.serverURI = serverURI;
		// selectedEngines can be null, which means don't write
		// userSelectedEngines to prefs. This makes any created meta/global
		// record
		// have the default set of engines to sync.
		this.selectedEngines = selectedEngines;
	}

	@Override
	public void handleSuccess(LoginResponse result) {
		Logger.info(LOG_TAG, "Got success response; adding Android account.");

		// We're on the UI thread, but it's okay to create the account here.
		AndroidFxAccount fxAccount;
		try {
			final String profile = Constants.DEFAULT_PROFILE;
			final String tokenServerURI = FxAccountConstants.DEFAULT_TOKEN_SERVER_ENDPOINT;
			// It is crucial that we use the email address provided by the
			// server
			// (rather than whatever the user entered), because the user's keys
			// are
			// wrapped and salted with the initial email they provided to
			// /create/account. Of course, we want to pass through what the user
			// entered locally as much as possible, so we create the Android
			// account
			// with their entered email address, etc.
			// The passwordStretcher should have seen this email address before,
			// so
			// we shouldn't be calculating the expensive stretch twice.
			byte[] quickStretchedPW = passwordStretcher
					.getQuickStretchedPW(result.remoteEmail.getBytes("UTF-8"));
			byte[] unwrapkB = FxAccountUtils
					.generateUnwrapBKey(quickStretchedPW);
			State state = new Engaged(email, result.uid, result.verified,
					unwrapkB, result.sessionToken, result.keyFetchToken);
			fxAccount = AndroidFxAccount.addAndroidAccount(mActivity, email,
					profile, serverURI, tokenServerURI, state);
			if (fxAccount == null) {
				throw new RuntimeException("Could not add Android account.");
			}

			if (selectedEngines != null) {
				Logger.info(LOG_TAG,
						"User has selected engines; storing to prefs.");
				SyncConfiguration.storeSelectedEnginesToPrefs(
						fxAccount.getSyncPrefs(), selectedEngines);
			}
		} catch (Exception e) {
			handleError(e);
			return;
		}
		// For great debugging.
		if (FxAccountConstants.LOG_PERSONAL_INFORMATION) {
			fxAccount.dump();
		}

		// The GetStarted activity has called us and needs to return a result to
		// the authenticator.
		final Intent intent = new Intent();
		intent.putExtra(AccountManager.KEY_ACCOUNT_NAME, email);
		intent.putExtra(AccountManager.KEY_ACCOUNT_TYPE,
				FxAccountConstants.ACCOUNT_TYPE);
		// intent.putExtra(AccountManager.KEY_AUTHTOKEN, accountType);
		mActivity.setResult(mActivity.RESULT_OK, intent);

		// Show success activity depending on verification status.
		Intent successIntent = makeSuccessIntent(email, result);
		mActivity.startActivity(successIntent);
		mActivity.finish();
	}

	public Intent makeSuccessIntent(String email, LoginResponse result) {
		Intent successIntent;
		if (result.verified) {
			successIntent = new Intent(mActivity,
					FxAccountVerifiedAccountActivity.class);
		} else {
			successIntent = new Intent(mActivity,
					FxAccountConfirmAccountActivity.class);
		}
		// Per http://stackoverflow.com/a/8992365, this triggers a known bug
		// with
		// the soft keyboard not being shown for the started activity. Why,
		// Android, why?
		successIntent.setFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		return successIntent;
	}
}
