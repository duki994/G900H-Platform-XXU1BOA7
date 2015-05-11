package org.sec.android.app.sbrowser.firefox.controller;

import java.io.UnsupportedEncodingException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClient20.LoginResponse;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.background.fxa.PasswordStretcher;
import org.mozilla.gecko.background.fxa.QuickPasswordStretcher;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.activities.AddAccountDelegate;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask.FxAccountSignInTask;

import android.app.Activity;
import android.util.Log;
import android.widget.TextView;

import com.sec.android.app.sbrowser.firefox.ISBrowserFFSignIn;
import com.sec.android.app.sbrowser.firefox.ProgressDisplay;

public class SbrowserFFSignIn implements ISBrowserFFSignIn{

	@Override
	public void signInInterface(String email, String password, final Activity activity, final TextView remoteErrorTextView, final int errorArray[]) {

	    String serverURI = FxAccountConstants.DEFAULT_AUTH_SERVER_ENDPOINT;
	    PasswordStretcher passwordStretcher = new QuickPasswordStretcher(password);
	    final SBrowserFFShowRMErr browserFFShowRMErr = new SBrowserFFShowRMErr();
	    // This delegate creates a new Android account on success, opens the
	    // appropriate "success!" activity, and finishes this activity.
	    
	    
	    
	    RequestDelegate<LoginResponse> delegate = new AddAccountDelegate(email, passwordStretcher, serverURI, null, activity) {
	      @Override
	      public void handleError(Exception e) {
	    	  browserFFShowRMErr.showRemoteError(e, errorArray, remoteErrorTextView);
	      }
 
	      @Override
	      public void handleFailure(FxAccountClientRemoteException e) {
	    	  browserFFShowRMErr.showRemoteError(e, errorArray, remoteErrorTextView);
	      }
	    };

	    Executor executor = Executors.newSingleThreadExecutor();
	    FxAccountClient client = new FxAccountClient20(serverURI, executor);
	    try {
	      new FxAccountSignInTask((activity),(ProgressDisplay)activity, email, passwordStretcher, client, delegate).execute();
	    } catch (UnsupportedEncodingException e) {
	    	Log.w("FxAccountSignInTask", "Caught UnsupportedEncodingException : " + e);
	    }
	  
	}

}
