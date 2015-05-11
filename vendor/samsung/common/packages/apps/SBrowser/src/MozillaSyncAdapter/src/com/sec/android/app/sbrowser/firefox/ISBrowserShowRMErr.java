package com.sec.android.app.sbrowser.firefox;

import android.widget.TextView;

public interface ISBrowserShowRMErr {
	public void showRemoteError(Exception e, int errorArray[],
			TextView txtvw);
}
