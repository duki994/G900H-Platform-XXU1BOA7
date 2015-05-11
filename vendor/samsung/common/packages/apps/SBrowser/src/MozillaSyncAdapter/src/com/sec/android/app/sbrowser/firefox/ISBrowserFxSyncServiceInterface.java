package com.sec.android.app.sbrowser.firefox;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

public interface ISBrowserFxSyncServiceInterface {
	public void oncreate(Context context, boolean value, Bundle extras);

	public IBinder onBind(Intent intent, Bundle extras);
}
