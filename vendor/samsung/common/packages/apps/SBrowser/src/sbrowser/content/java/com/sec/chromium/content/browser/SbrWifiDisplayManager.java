/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.sec.chromium.content.browser;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.app.AlertDialog;
import android.app.Presentation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnKeyListener;
import android.content.DialogInterface.OnShowListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.WfdAppState;
import android.hardware.display.WifiDisplay;
import android.hardware.display.WifiDisplayStatus;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pManager;
import android.net.wifi.p2p.WifiP2pManager.Channel;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.Parcelable;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.sec.android.app.sbrowser.R;
import com.sec.android.app.sbrowser.SBrowserMainActivity;
import com.sec.chromium.content.browser.SbrContentVideoViewNew;
import com.sec.chromium.content.browser.SbrContentVideoView.VideoSurfaceView;
import com.sec.chromium.content.browser.SbrVideoControllerView.MediaPlayerControl;

public class SbrWifiDisplayManager{
    private static final String TAG = "SbrWifiDisplayManager";
    
    private Context                 mContext;
    private AlertDialog             mDialog = null;
    protected DisplayManager        mDisplayManager = null;
    protected WifiDisplayStatus     mWifiDisplayStatus = null;
    private final IntentFilter      mIntentFilter = new IntentFilter();
    private boolean                 mReceiverRegistered = false;
    private boolean                 mConnecting = false;
    private boolean                 mWasPlaying = false;
    private ChangePlayerAdapter     mChangePlayerAdapter = null;
    private ArrayList<ChangePlayer> mPlayerList = null;
    private String                  mConnectedDeviceName = null;
    private ProgressBar             mRefreshProgressBar = null;

    private FrameLayout             mMainView;
    private VideoSurfaceView        mPresentationView = null;
    private VideoPresentation       mPresentation = null;
    private SbrContentVideoViewNew  mVideoView = null;

    public static final int         DISMISS_PROGRESS_ICON = 100;
    public static final int         UPDATE_CHANGE_PLAYER_LIST = 200;
    public static final int         PROCEED_CHANGE_PLAYER_SHOW = 300;

    // KMS for SideSync
    public static final String SIDESYNC_KMS_SINK_SERVICE_CLASS = "com.sec.android.sidesync.kms.sink.service.SideSyncServerService";
    public static final String SIDESYNC_KMS_SOURCE_SERVICE_CLASS = "com.sec.android.sidesync.kms.source.service.SideSyncService";

    private final Handler mPopupHandler = new Handler() {
        public void handleMessage(final Message msg) {
            switch (msg.what) {
                case DISMISS_PROGRESS_ICON:
                    mPopupHandler.removeMessages(DISMISS_PROGRESS_ICON);
                    stopScanWifiDisplays();
                    mRefreshProgressBar.setVisibility(View.GONE);
                    break;
                case UPDATE_CHANGE_PLAYER_LIST:
                    mPopupHandler.removeMessages(UPDATE_CHANGE_PLAYER_LIST);
                    updateWifiDisplayList();
                    break;
                case PROCEED_CHANGE_PLAYER_SHOW:
                    mPopupHandler.removeMessages(PROCEED_CHANGE_PLAYER_SHOW);
                    showDevicePopup(mWasPlaying);
                    break;
                default:
                    break;
            }
        };
    };

    public SbrWifiDisplayManager(Context context){
        mContext = context;
        mDisplayManager = (DisplayManager) mContext.getSystemService(Context.DISPLAY_SERVICE);
    }
    
    public void showDevicePopup(boolean wasPlaying){
        mWasPlaying = wasPlaying;
        AlertDialog.Builder dialog = new AlertDialog.Builder(mContext);
        LayoutInflater inflater = (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View bodyLayout = inflater.inflate(R.layout.video_change_player, null);
        ListView mDeviceListView = (ListView) bodyLayout.findViewById(R.id.device_list);

        ((TextView)bodyLayout.findViewById(R.id.allshare_title)).setText(R.string.video_available_devices_cap);
        
        dialog.setTitle(mContext.getString(R.string.video_select_device));
        dialog.setView(bodyLayout);

        mRefreshProgressBar = (ProgressBar) bodyLayout.findViewById(R.id.allshare_refresh_progressbar);
        mRefreshProgressBar.setVisibility(View.VISIBLE);

        dialog.setPositiveButton(R.string.accessibility_button_refresh, null);
        dialog.setNegativeButton(R.string.bookmark_cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int arg1) {
                dialog.dismiss();
            }
        });

        makeWfdDeviceList();
        
        mChangePlayerAdapter = new ChangePlayerAdapter(mContext, mPlayerList);
        if(mPlayerList == null) return;
        
        mDeviceListView.setAdapter(mChangePlayerAdapter);
        mDeviceListView.setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if(parent.getCount() > position) {
                    Log.d(TAG, "[html5media] showDevicePopup. onItemClick. : " + position);
                    mConnecting = true;
                    if(position > -1)
                        selectWfdDevice(position);
                }
                if(mDialog != null)
                    mDialog.dismiss();
            }
        });

        mDialog = dialog.create();
        mDialog.setOnShowListener(new OnShowListener() {
            public void onShow(DialogInterface dialog) {
                registerWifiDisplayReceiver();

                mPopupHandler.removeCallbacksAndMessages(null);
                mPopupHandler.sendMessageDelayed(mPopupHandler.obtainMessage(DISMISS_PROGRESS_ICON), 10000);
                
                mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
                    public void onClick(View view) {
                        mPopupHandler.removeMessages(DISMISS_PROGRESS_ICON);
                        updateWifiDisplayList();
                        scanWifiDisplays();
                        mRefreshProgressBar.setVisibility(View.VISIBLE);
                        mPopupHandler.sendMessageDelayed(mPopupHandler.obtainMessage(DISMISS_PROGRESS_ICON), 10000);
                    }
                });
                makeWfdDeviceList();
            }
        });

        mDialog.setCanceledOnTouchOutside(true);
        mDialog.setOnDismissListener(new OnDismissListener() {
            public void onDismiss(final DialogInterface dialog) {
                if(!mConnecting){
                    unregisterWifiDisplayReceiver();
                    if (mDisplayManager != null) {
                        Log.d(TAG,"[html5media] Popup onDismiss.");
                        try {
                            mDisplayManager.setActivityState(WfdAppState.PAUSE);
                            mDisplayManager.setActivityState(WfdAppState.TEARDOWN);
                        } catch (SecurityException e) {
                        }
                    }
                }

                mDialog = null;
            }
        });

        mDialog.setOnKeyListener(new OnKeyListener() {
            @Override
            public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event) {
                switch (event.getKeyCode()) {
                    case KeyEvent.KEYCODE_POWER:
                        if (mVideoView != null)
                            return mVideoView.dispatchKeyEvent(event);
                    default:
                        return false;
                }
            }
        });

        mDialog.show();
    }

    public void dismissDevicePopup() {
        if(mDialog != null)
            mDialog.dismiss();
    }

    public boolean isShowingDevicePopup() {
        if(mDialog != null)
            return mDialog.isShowing();
        else
            return false;
    }

    public void scanWifiDisplays() {
        Log.d(TAG, "[html5media] scanWifiDisplays");
        if(mDisplayManager != null) {
            try {
                mDisplayManager.setActivityState(WfdAppState.SETUP);
                mDisplayManager.scanWifiDisplays();
            } catch (SecurityException e) {
            }
        }
    }

    public void stopScanWifiDisplays() {
        Log.d(TAG, "[html5media] stopScanWifiDisplays");
        if(mDisplayManager != null
            && mDisplayManager.getWifiDisplayStatus().getActiveDisplayState() == WifiDisplayStatus.SCAN_STATE_NOT_SCANNING)
            mDisplayManager.stopScanWifiDisplays();
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if(DisplayManager.ACTION_WIFI_DISPLAY_STATUS_CHANGED.equals(action)) {
                mWifiDisplayStatus = (WifiDisplayStatus) intent.getParcelableExtra(DisplayManager.EXTRA_WIFI_DISPLAY_STATUS);
                if(mWifiDisplayStatus == null || isPresentationStarted())
                    return;
                
                Log.d(TAG, "[html5media] WFD status changed. scan : " + mWifiDisplayStatus.getScanState()
                    + ", activeDisplay : " + mWifiDisplayStatus.getActiveDisplayState());
                
                if(mWifiDisplayStatus.getActiveDisplayState() == WifiDisplayStatus.DISPLAY_STATE_CONNECTED){
                    stopScanWifiDisplays();
                    unregisterWifiDisplayReceiver();
                    mConnectedDeviceName = mWifiDisplayStatus.getActiveDisplay().getDeviceName();
                    Log.d(TAG,"[html5media] connected. " + mConnectedDeviceName);
                    
                    Display[] displays = mDisplayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
                    for (Display display : displays) {
                        if(mConnectedDeviceName.equals(display.getName())){
                            showPresentation(display);
                            break;
                        }
                    }
                    return;
                }
                else {
                    makeWfdDeviceList();
                }
            }
        }
    };

    public void registerWifiDisplayReceiver() {
        Log.d(TAG,"[html5media] registerWifiDisplayReceiver");
        mIntentFilter.addAction(DisplayManager.ACTION_WIFI_DISPLAY_STATUS_CHANGED);
        mContext.registerReceiver(mReceiver, mIntentFilter);
        mReceiverRegistered = true;
    }

    public void unregisterWifiDisplayReceiver() {
        Log.d(TAG,"[html5media] unregisterWifiDisplayReceiver");
        if(mReceiverRegistered) {
            mReceiverRegistered = false;
            if(mReceiver != null)
                mContext.unregisterReceiver(mReceiver);
        }
    }

    public void disconnectWifiDisplay(){
        Log.d(TAG, "[html5media] disconnectWifiDisplay");
        if(mDisplayManager != null)
            mDisplayManager.disconnectWifiDisplay();
    }

    private void makeWfdDeviceList() {
        Log.d(TAG,"[html5media] makeWfdDeviceList");

        List<Parcelable> availableDisplays = new ArrayList<Parcelable>();
        String wfdName = null;
        String deviceType = null;
        WifiDisplay wfDisplay = null;
        String[] tokens = null;

        if(mPlayerList == null)
            mPlayerList = new ArrayList<ChangePlayer>();
        else
            mPlayerList.clear();

        // 1. add connected device
        if(isWfdConnected()){
            mConnectedDeviceName = mDisplayManager != null ? mDisplayManager.getWifiDisplayStatus().getActiveDisplay().getDeviceName() : "";
            if(mConnectedDeviceName != null)
                mPlayerList.add(new ChangePlayer(mConnectedDeviceName, false));
        }

        // 2. add MyDevice
        mPlayerList.add(new ChangePlayer(mContext.getString(R.string.my_device), true));

        // 3. add rest
        if(mWifiDisplayStatus != null) {
            for (WifiDisplay d : mWifiDisplayStatus.getDisplays()) {
                if(d.isAvailable()) availableDisplays.add(d);
            }
        }
        else{
            updateWifiDisplayList();
            return;
        }
        
        Iterator<Parcelable> iter = availableDisplays.iterator();
        while (iter.hasNext()) {
            wfDisplay = (WifiDisplay)iter.next();
            wfdName = wfDisplay.getDeviceName();
            if(wfdName.equals(mConnectedDeviceName))
                continue;
            
            if(TextUtils.isEmpty(wfdName))
                wfdName = wfDisplay.getDeviceAddress();

            deviceType = wfDisplay.getPrimaryDeviceType();
            if(deviceType != null) {
                Log.d(TAG, "[html5media] makeWfdDeviceList. primaryDeviceType : " + deviceType);
                tokens = deviceType.split("-");
            }
            mPlayerList.add(new ChangePlayer(wfdName, false));
        }

        updateWifiDisplayList();
    }

    private void selectWfdDevice(int which) {
        Log.d(TAG, "[html5media] selectWfdDevice.");

        String wfdName = null;
        WifiDisplay wfDisplay = null;
        String connectingDeviceName = String.format("%s", mPlayerList.get(which).getDeviceName());
        List<Parcelable> availableDisplays = new ArrayList<Parcelable>();

        // selecting currently connected device
        if(isWfdConnected()){
            if(mConnectedDeviceName.equals(connectingDeviceName)){
                mConnecting = false;
                return;
            }
        }
        // selecting MyDevice
        if(mPlayerList.get(which).isMyDevice()){
            // return if MyDevice is already selected
            if(mConnectedDeviceName == null
                || mConnectedDeviceName.equals(mContext.getString(R.string.my_device))
                || !isPresentationStarted() || mVideoView == null)
                return;
            
            mVideoView.pause();
            stopScanWifiDisplays();
            dismissPresentation();
            mVideoView.showContentVideoView();
            disconnectWifiDisplay();
            mConnecting = false;
            mConnectedDeviceName = mContext.getString(R.string.my_device);
            return;
        }

        if(mWifiDisplayStatus != null) {
            for (WifiDisplay d : mWifiDisplayStatus.getDisplays()) {
                if(d.isAvailable()) availableDisplays.add(d);
            }
        }
        
        for (Parcelable d : availableDisplays) {
            wfDisplay = (WifiDisplay)d;
            wfdName = wfDisplay.getDeviceName();
            if(mPlayerList.get(which).getDeviceName().equals(wfdName)) {
                if(mDisplayManager != null) {
                    Log.d(TAG, "[html5media] connectWifiDisplay. deviceAddress : " + wfDisplay.getDeviceAddress());
                    mDisplayManager.setActivityState(WfdAppState.RESUME);
                    mDisplayManager.connectWifiDisplayWithMode(WifiDisplayStatus.CONN_STATE_NORMAL, wfDisplay.getDeviceAddress());                    
                    stopScanWifiDisplays();
                    break;
                }
                break;
            }
        }
    }

    public void updateWifiDisplayList() {
        Log.d(TAG,"[html5media] updateWifiDisplayList");
        if(mDialog != null && mDialog.isShowing() && mPlayerList != null && mChangePlayerAdapter != null) {
            ((SBrowserMainActivity)mContext).runOnUiThread(new Runnable() {
                public void run() {
                    mChangePlayerAdapter.notifyDataSetChanged();
                }
            });
        }
    }

    public void setVideoView(SbrContentVideoViewNew videoView){
        mVideoView = videoView;
    }

    public boolean isWfdConnected() {
        return (mDisplayManager != null) && (mDisplayManager.getWifiDisplayStatus().getActiveDisplayState() == WifiDisplayStatus.DISPLAY_STATE_CONNECTED);
    }

    private boolean checkSideSyncConnected() {
        if (mContext == null || mContext.getContentResolver() == null)
            return false;

        if (Settings.System.getInt(mContext.getContentResolver(), "sidesync_source_connect", 0) == 1)
            return true;

        ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            for (RunningServiceInfo serviceInfo : am.getRunningServices(Integer.MAX_VALUE)) {
                if (serviceInfo != null) {
                    String mServiceName = serviceInfo.service.getClassName();
                    if (SIDESYNC_KMS_SINK_SERVICE_CLASS.equals(mServiceName)
                            || SIDESYNC_KMS_SOURCE_SERVICE_CLASS.equals(mServiceName))
                        return true;
                }
            }
        }
        return false;
    }

    private void showPresentation(Display display) {
        Log.d(TAG,"[html5media] showPresentation.");
        
        mPresentation = new VideoPresentation(mContext, display);
        mMainView = (FrameLayout) View.inflate(mContext, R.layout.video_externalpresentation, null);

        WindowManager.LayoutParams windowAttributes = createLayoutParams();
        Window window = mPresentation.getWindow();
        window.requestFeature(Window.FEATURE_NO_TITLE);
        window.setBackgroundDrawableResource(android.R.color.transparent);
        window.setAttributes(windowAttributes);
        mPresentation.setContentView(mMainView);

        if (mVideoView != null) {
            mPresentationView = mVideoView.getSurfaceView();
            mPresentationView.setBackgroundColor(Color.TRANSPARENT);
            mPresentationView.setIsPresentationMode(true);

            // remove video surface view from SbrContentVideoView but not nullify the surface view.
            mVideoView.removeSurfaceView(false);

            // add the detached view to presentation
            mMainView.addView(mPresentationView, new FrameLayout.LayoutParams(
                              ViewGroup.LayoutParams.MATCH_PARENT,
                              ViewGroup.LayoutParams.MATCH_PARENT,
                              Gravity.CENTER));

            mPresentation.show();
        }
    }

    public void showPresentationAutoplay(boolean wasPlaying) {
        Log.d(TAG,"[html5media] showPresentationAutoplay.");        

        if (!isWfdConnected() || isPresentationStarted() || checkSideSyncConnected())
            return;

        mWasPlaying = wasPlaying;

        if (mConnectedDeviceName == null)
            mConnectedDeviceName = mDisplayManager.getWifiDisplayStatus().getActiveDisplay().getDeviceName();
        
        Display[] displays = mDisplayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        for (Display display : displays) {
            if (mConnectedDeviceName.equals(display.getName())) {
                showPresentation(display);
                return;
            }
        }
    }

    private WindowManager.LayoutParams createLayoutParams() {
        int windowType = WindowManager.LayoutParams.TYPE_PHONE;
        try {
            Field field = WindowManager.LayoutParams.class.getField("TYPE_FAKE_APPLICATION");
            if (field != null) {
                windowType = field.getInt(field);
            }
        } catch (SecurityException e) {
        } catch (NoSuchFieldException e) {
        } catch (IllegalArgumentException e) {
        } catch (IllegalAccessException e) {
        }

        WindowManager.LayoutParams lp;

        // type, flag, format
        lp = new WindowManager.LayoutParams(windowType,
                WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.RGBA_8888); // format

        lp.width = WindowManager.LayoutParams.MATCH_PARENT;
        lp.height = WindowManager.LayoutParams.MATCH_PARENT;
        lp.gravity = Gravity.BOTTOM;
        lp.setTitle(getClass().getName());

        return lp;
    }

    // for normal cases
    public void dismissPresentation() {
        dismissPresentation(false);
    }

    public void dismissPresentation(boolean exitingFullscreen){
        Log.d(TAG, "[html5media] dismissPresentation");
        if(mMainView != null && mPresentationView != null) {
            // only if it's called from SbrContentVideoViewNew.exitFullscreen()
            if(exitingFullscreen)
                mPresentationView.setIsPresentationMode(false);

            mMainView.removeView(mPresentationView);

            try {
                mPresentation.dismiss();
                mPresentation = null;
                mMainView = null;
                mPresentationView = null;
            } catch (IllegalArgumentException e) {
                e.printStackTrace();
            }
        }
        
        if(mDisplayManager != null){
            try{
                mDisplayManager.setActivityState(WfdAppState.RESUME);
            }catch(SecurityException e){
            }
        }
    }

    public void onActivityResume(){
        mConnecting = false;
    }

    public void onPlayerError(){
        dismissPresentation();
    }

    public boolean isPresentationStarted(){
        return mPresentation != null && mPresentation.isStarted();
    }

    public class ChangePlayer {
        private String mDeviceName;
        private boolean mIsMyDevice;

        public ChangePlayer(String name, boolean isMyDevice) {
            mDeviceName = name;
            mIsMyDevice = isMyDevice;
        }
        public String getDeviceName() {
            return mDeviceName;
        }
        public boolean isMyDevice(){
            return mIsMyDevice;
        }
    }
    
    public class ChangePlayerAdapter extends ArrayAdapter<ChangePlayer> {
        private LayoutInflater vi;
        private String deviceName;        
        private String DEVICE_NAME = "device_name";
        private Context mContext;

        public ChangePlayerAdapter(Context context, ArrayList<ChangePlayer> items) {
            super(context, 0, items);
            mContext = context;
            vi = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            deviceName = Settings.System.getString(context.getContentResolver(), DEVICE_NAME);
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View v = convertView;
            ViewHolder holder = null;
            
            if(v == null) {
                v = vi.inflate(R.layout.videoplayer_change_player_list, null);
                holder = new ViewHolder();
                holder.thubmail = (ImageView) v.findViewById(R.id.playericon);
                holder.dmrName = (TextView) v.findViewById(R.id.playername);
                holder.dmrDescription = (TextView) v.findViewById(R.id.playerdescription);
                v.setTag(holder);
            } else {
                holder = (ViewHolder) v.getTag();
            }
            
            CheckedTextView checkIcon = (CheckedTextView) v.findViewById(R.id.selected_player_check);
            if(position == 0)
                checkIcon.setChecked(true);
            else
                checkIcon.setChecked(false);

            ChangePlayer deviceItem = getItem(position);
            if(deviceItem != null) {
                if(holder.dmrName != null)
                    holder.dmrName.setText(deviceItem.getDeviceName());

                if(holder.dmrDescription != null) {
                    if(deviceItem.isMyDevice())
                        holder.dmrDescription.setText(Settings.System.getString(mContext.getContentResolver(), "device_name"));
                    else
                        holder.dmrDescription.setText(R.string.changeplayer_descrpition_mirroron);
                }

                if (holder.thubmail != null) {
                    if(deviceItem.isMyDevice()){
                        holder.thubmail.setImageDrawable(mContext.getResources().getDrawable(R.drawable.ic_widi_telephone));
                    } else {
                        holder.thubmail.setImageDrawable(mContext.getResources().getDrawable(R.drawable.ic_widi_displays));
                    }
                }
            }

            return v;
        }

        private class ViewHolder {
            public ImageView thubmail;
            public TextView dmrName;            
            public TextView dmrDescription;
        }
    }

    private final class VideoPresentation extends Presentation {
        private boolean mIsStarted;

        public VideoPresentation(Context context, Display display) {
            super(context, display);
            mIsStarted = false;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            // Be sure to call the super class.
            super.onCreate(savedInstanceState);
        }

        @Override
        protected void onStart (){
            Log.d(TAG,"[html5media] VideoPresentation. onStart");
            mConnecting = false;
            mIsStarted = true;

            if(mVideoView != null)
                mVideoView.onPresentationStart(mWasPlaying);
            
            super.onStart();
        }

        @Override
        protected void onStop (){
            Log.d(TAG,"[html5media] VideoPresentation. onStop");
            if(mVideoView != null && mIsStarted)
                mVideoView.onPresentationStop();

            mIsStarted = false;
            super.onStop();
        }

        @Override public void onDisplayChanged (){
            return;
        }

        public boolean isStarted(){
            return mIsStarted;
        }

        public void setIsStarted(boolean isStarted){
            mIsStarted = isStarted;
        }
    }

}
