package org.sec.android.app.sbrowser.firefox.controller;

import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import com.sec.android.app.sbrowser.firefox.ISBrowserFxSyncServiceInterface;

public class SbrowserFxSyncImpl implements ISBrowserFxSyncServiceInterface{
	 private static FxAccountSyncAdapter syncAdapter = null;
	@Override
	public void oncreate(Context context, boolean value, Bundle extras) {
		 if (syncAdapter == null) {
		        syncAdapter = new FxAccountSyncAdapter(context, true, extras);
		      }
	}

	@Override
	public IBinder onBind(Intent intent, Bundle extras) {
		return syncAdapter.getSyncAdapterBinder();
	}

}
