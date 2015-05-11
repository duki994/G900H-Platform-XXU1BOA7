package org.sec.android.app.sbrowser.firefox.controller;


import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import com.sec.android.app.sbrowser.firefox.FxAccountAuthenticator;
import com.sec.android.app.sbrowser.firefox.ISBrowserFxSyncAuth;

public class SbrowserFxSyncAuthImpl implements ISBrowserFxSyncAuth {

	// Lazily initialized by <code>getAuthenticator</code>.
	protected FxAccountAuthenticator accountAuthenticator = null;
	Context mContext = null;

	protected FxAccountAuthenticator getAuthenticator() {
		if (accountAuthenticator == null) {
			accountAuthenticator = new FxAccountAuthenticator(mContext);
		}

		return accountAuthenticator;
	}

	@Override
	public void oncreate(Context context, boolean value) {
		// TODO Auto-generated method stub
		mContext = context;
		accountAuthenticator = getAuthenticator();
	}

	@Override
	public IBinder onBind(Intent intent) { 
		if (intent.getAction().equals(
				android.accounts.AccountManager.ACTION_AUTHENTICATOR_INTENT)) {
			
			
			IBinder binder =  getAuthenticator().getIBinder();
			Log.e("manoj", "manoj===================" + binder);
			return binder;
		}
		
		return null;
	}

}
