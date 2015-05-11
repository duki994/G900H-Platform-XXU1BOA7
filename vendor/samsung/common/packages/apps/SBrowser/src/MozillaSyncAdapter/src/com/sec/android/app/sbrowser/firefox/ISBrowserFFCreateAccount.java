package com.sec.android.app.sbrowser.firefox;

import java.util.Map;

import android.widget.TextView;

public interface ISBrowserFFCreateAccount {
	public void createAccount(String email, String password,
			Map<String, Boolean> engines,
			FxAccountAbstractSetupActivity mactivity, int errorArray[],
			TextView rmTxtView);

	void handleonClickofCreatAcct(String email, String password,
			Map<String, Boolean> engines, String yearedit, String[] yearItems,
			FxAccountAbstractSetupActivity mactivity, int errorArray[],
			TextView rmTxtView);
}
