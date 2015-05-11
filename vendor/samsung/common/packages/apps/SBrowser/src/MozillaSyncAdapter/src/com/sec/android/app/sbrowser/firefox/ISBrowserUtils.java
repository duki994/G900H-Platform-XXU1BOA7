package com.sec.android.app.sbrowser.firefox;

import java.io.UnsupportedEncodingException;
import java.security.NoSuchAlgorithmException;

public interface ISBrowserUtils {
	public String getPrefsPathInterface(final String product,
			final String username, final String serverURL,
			final String profile, final long version)
			throws NoSuchAlgorithmException, UnsupportedEncodingException;
}
