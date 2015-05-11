package com.sec.android.app.sbrowser.firefox;

import java.util.Map;

import android.content.SharedPreferences;

public interface ISBroserFFSyncConfiguration {
	public void storeSelectedEnginesToPrefsInterface(SharedPreferences prefs, Map<String, Boolean> selectedEngines);
}
