package org.sec.android.app.sbrowser.firefox.controller;

import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountClient10.RequestDelegate;
import org.mozilla.gecko.background.fxa.FxAccountClient20;
import org.mozilla.gecko.background.fxa.FxAccountClientException.FxAccountClientRemoteException;
import org.mozilla.gecko.fxa.activities.FxAccountSetupTask;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;
import org.mozilla.gecko.fxa.login.Engaged;

import android.accounts.Account;
import android.content.Context;
import android.widget.Toast;

import com.sec.android.app.sbrowser.firefox.ISBrowserFFConfirmAccount;

public class SBrowserFFConfirmAccountController implements ISBrowserFFConfirmAccount{

	@Override
	public void resendCode(Context context, Account account, int successRID, int failureRID) {
		AndroidFxAccount fxAccount = new AndroidFxAccount(context, account);
		
	    RequestDelegate<Void> delegate = new ResendCodeDelegate(context, successRID, failureRID);

	    byte[] sessionToken;
	    try {
	      sessionToken = ((Engaged) fxAccount.getState()).getSessionToken();
	    } catch (Exception e) {
	      delegate.handleError(e);
	      return;
	    }
	    if (sessionToken == null) {
	      delegate.handleError(new IllegalStateException("sessionToken should not be null"));
	      return;
	    }

	    Executor executor = Executors.newSingleThreadExecutor();
	    FxAccountClient client = new FxAccountClient20(fxAccount.getAccountServerURI(), executor);
	    new FxAccountResendCodeTask(context, sessionToken, client, delegate).execute();
	  
	}
	
	
	  public static class FxAccountResendCodeTask extends FxAccountSetupTask<Void> {
		    protected static final String LOG_TAG = FxAccountResendCodeTask.class.getSimpleName();

		    protected final byte[] sessionToken;

		    public FxAccountResendCodeTask(Context context, byte[] sessionToken, FxAccountClient client, RequestDelegate<Void> delegate) {
		      super(context, null, client, delegate);
		      this.sessionToken = sessionToken;
		    }

		    @Override
		    protected InnerRequestDelegate<Void> doInBackground(Void... arg0) {
		      try {
		        client.resendCode(sessionToken, innerDelegate);
		        latch.await();
		        return innerDelegate;
		      } catch (Exception e) {
		        Logger.error("SBrowserFFConfirmAccountController", "Got exception signing in.", e);
		        delegate.handleError(e);
		      }
		      return null;
		    }
		  }

		  protected static class ResendCodeDelegate implements RequestDelegate<Void> {
		    public final Context context;
		    public final int mSuccessRID;
		    public final int mFailureRID;
		    
		    public ResendCodeDelegate(Context context, int successRID, int failureRID) {
		      this.context = context;
		      mSuccessRID = successRID;
		      mFailureRID = failureRID;
		    }

		    @Override
		    public void handleError(Exception e) {
		      Logger.warn("SBrowserFFConfirmAccountController", "Got exception requesting fresh confirmation link; ignoring.", e);
		      Toast.makeText(context, mFailureRID, Toast.LENGTH_LONG).show();
		    }

		    @Override
		    public void handleFailure(FxAccountClientRemoteException e) {
		      handleError(e);
		    }

		    @Override
		    public void handleSuccess(Void result) {
		      Toast.makeText(context, mSuccessRID, Toast.LENGTH_SHORT).show();
		    }
		  }

}
