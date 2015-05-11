/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.telephony.TelephonyManager;

/**
 * A <code>ClientsDataDelegate</code> implementation that persists to a
 * <code>SharedPreferences</code> instance.
 */
public class SharedPreferencesClientsDataDelegate implements ClientsDataDelegate {
  protected final SharedPreferences sharedPreferences;

  public SharedPreferencesClientsDataDelegate(SharedPreferences sharedPreferences) {
    this.sharedPreferences = sharedPreferences;
  }

  @Override
  public synchronized String getAccountGUID() {
    String accountGUID = sharedPreferences.getString(SyncConfiguration.PREF_ACCOUNT_GUID, null);
    if (accountGUID == null) {
    	WifiManager manager = (WifiManager) FxAccountSyncAdapter.getAppContext().getSystemService(Context.WIFI_SERVICE);
		if (manager != null)
		{
			WifiInfo info = manager.getConnectionInfo();
			if (info != null)
				accountGUID = info.getMacAddress();
		}
		if (accountGUID == null)
		{
	  		TelephonyManager tm = (TelephonyManager) FxAccountSyncAdapter.getAppContext().getSystemService(Context.TELEPHONY_SERVICE); 
	  		if (tm != null)
	  		{
	  			accountGUID = tm.getDeviceId();
	  		} 
		}
		if (accountGUID == null)
		{
			accountGUID = Build.SERIAL;
		}
		
		
		UserHandle uh = Process.myUserHandle();
  	    UserManager um = (UserManager) FxAccountSyncAdapter.getAppContext().getSystemService(Context.USER_SERVICE);
  	    long userSerialNumber = -1;
  	    if(um != null){
  	        userSerialNumber = um.getSerialNumberForUser(uh);  	     
  	    }
  	  accountGUID += "_" + userSerialNumber;
      sharedPreferences.edit().putString(SyncConfiguration.PREF_ACCOUNT_GUID, accountGUID).commit();
    }
    return accountGUID;
  }

  @Override
  public synchronized String getClientName() {
    String clientName = sharedPreferences.getString(SyncConfiguration.PREF_CLIENT_NAME, null);
    if (clientName == null) {
    //  clientName = android.os.Build.MODEL;
    	clientName =
			  Settings.System.getString(FxAccountSyncAdapter.getAppContext().getContentResolver(),
					  "device_name");
      sharedPreferences.edit().putString(SyncConfiguration.PREF_CLIENT_NAME, clientName).commit();
    }
    return clientName;
  }

  @Override
  public synchronized void setClientsCount(int clientsCount) {
    sharedPreferences.edit().putLong(SyncConfiguration.PREF_NUM_CLIENTS, (long) clientsCount).commit();
  }

  @Override
  public boolean isLocalGUID(String guid) {
    return getAccountGUID().equals(guid);
  }

  @Override
  public synchronized int getClientsCount() {
    return (int) sharedPreferences.getLong(SyncConfiguration.PREF_NUM_CLIENTS, 0);
  }
}
