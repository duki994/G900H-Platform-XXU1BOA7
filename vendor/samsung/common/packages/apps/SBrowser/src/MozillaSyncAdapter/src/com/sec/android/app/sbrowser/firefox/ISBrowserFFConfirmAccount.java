package com.sec.android.app.sbrowser.firefox;

import android.accounts.Account;
import android.content.Context;

public interface ISBrowserFFConfirmAccount {

	void resendCode(Context context, Account account, int successRID, int failureRID);
}
