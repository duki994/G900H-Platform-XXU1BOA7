package com.sec.android.app.sbrowser.firefox;

import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

public interface ISBrowserFxSyncAuth {
	public void oncreate(Context context, boolean value);

	public IBinder onBind(Intent intent);
}
