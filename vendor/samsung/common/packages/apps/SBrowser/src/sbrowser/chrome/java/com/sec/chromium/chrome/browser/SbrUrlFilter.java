package com.sec.chromium.chrome.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("net")
public class SbrUrlFilter {
	public interface UrlFilter {
		public boolean isBlockedUrl(String url);
	}

	private static UrlFilter sUrlFilter = null;

	public static void setUrlFilter(UrlFilter filter) {
		sUrlFilter = filter;
	}

	@CalledByNative
	private static boolean isBlockedUrl(String url) {
		if (sUrlFilter != null)
			return sUrlFilter.isBlockedUrl(url);
		return false;
	}
}
