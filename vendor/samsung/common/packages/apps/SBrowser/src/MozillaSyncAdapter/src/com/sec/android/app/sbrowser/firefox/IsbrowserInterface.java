package com.sec.android.app.sbrowser.firefox;

import android.widget.TextView;

public interface IsbrowserInterface {
	public void linkTextViewInterface(TextView view, String text, String url);
	public void linkifyTextViewInterface(TextView textView, boolean underlining);
}
