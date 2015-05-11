package com.sec.chromium.content.browser;

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.List;

import org.chromium.content.browser.LoadUrlParams;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import com.samsung.android.hermes.HermesServiceManager;
import com.samsung.android.hermes.HermesServiceManager.HermesResult;
import com.samsung.android.hermes.KerykeionResult;
import com.samsung.android.hermes.HermesServiceManager.IHermesCallBack;

import android.app.Activity;
import android.graphics.Rect;
import android.util.Log;

public class SbrSmartParsingManager {
    private static final String TAG = SbrSmartParsingManager.class.getSimpleName();
    SbrContentViewCore mSbrCore = null;
    String mSelectedText = null;
    int mSelectionId = 0;
    private Hashtable<String, ArrayList<KerykeionResult>> mResultTable = null;
    private boolean mHermesUIShown = false;
    
    public SbrSmartParsingManager(SbrContentViewCore core) {   
        mSbrCore = core;        
        mSelectionId = 0;
        mResultTable = new Hashtable<String, ArrayList<KerykeionResult>>();
    }
    
    public void extractMeaningfulWordFromSelection(String selectedText, int selectionId) {
        Log.d(TAG, "extractMeaningfulWordFromSelection: request hermes framework for parsing selected data!!");
        if(selectedText == null || selectedText.isEmpty()) {
            Log.e(TAG, "extractMeaningfulWordFromSelection: selected text is empty " + selectionId);
        }
        mSelectedText = selectedText;
        mSelectionId = selectionId;
        
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    final HermesServiceManager hermesManager = new HermesServiceManager(mSbrCore.getContext());
                    if (hermesManager == null) {
                        Log.e(TAG, "hermesManager = null!!");
                        return;
                    }
                    hermesManager.setHermesCallBack(new IHermesCallBack() {

                        @SuppressWarnings("unchecked")
                        @Override
                        public void onCompleted(Object result) {
                            Log.d(TAG, "extractMeaningfulWordFromSelection - onCompleted: result =" + result);
                            synchronized (hermesManager) {
                                if (result instanceof HermesResult) {
                                    final int uniqueId = ((HermesResult) result).getId(); // If you need Unique id, you can use this. 
                                    Object tempResult = ((HermesResult) result).getData(); 
                                    Log.d(TAG, "extractMeaningfulWordFromSelection - uniqueId="+uniqueId+", tempResult =" + tempResult);
                                    if (tempResult instanceof List<?> && ((List<KerykeionResult>)tempResult).size() > 0) {
                                        storeAndHighlightMeaningfulWords(uniqueId, ((List<KerykeionResult>) tempResult));
                                    } 
                                }
                            } 
                        }
                    });
                    
                    hermesManager.analysis(HermesServiceManager.GET_LINKS,
                            mSelectedText, mSelectionId);

                } catch (IllegalArgumentException e) {
                    e.printStackTrace();
                } catch (IllegalStateException e) {
                    e.printStackTrace();
                }
            }
        }).start();

    }
    
    private void storeAndHighlightMeaningfulWords(final int uniqueId, List<KerykeionResult> dataList) {
        ArrayList<KerykeionResult> resultList = new ArrayList<KerykeionResult>(dataList.size());
        final JSONArray meaningfulWordArray = new JSONArray();
        for(int i=0 ;i<dataList.size(); i++) {
            try {
                KerykeionResult selectedData = (dataList).get(i);
                resultList.add(selectedData);
                String str = (String) selectedData.getResult();
                meaningfulWordArray.put(i, str);
            } catch(JSONException e) {
                e.printStackTrace();
            }
        } 
        mResultTable.put(""+uniqueId, resultList);
        Log.d(TAG, "extractMeaningfulWordFromSelection show meaningful word highlight!!");
        //process result and highlight data
        ((Activity)mSbrCore.getContext()).runOnUiThread(new Runnable() {
            @Override
            public void run() {
                performMeaningfulWordHighlight(meaningfulWordArray, uniqueId);
            }
        });
    }

    private void performMeaningfulWordHighlight(final JSONArray meaningfulWordArray, final int id) {        
        Log.d(TAG, "performMeaningfulWordHighlight: performing MeaningfulWordHighlight [arr = "+meaningfulWordArray+"] !!");
        mSbrCore.loadUrl(new LoadUrlParams("javascript:meaningfulWordHighlight("+meaningfulWordArray.toString()+", "+id+");"));
    }
    
    public void showMeaningfulUI(Rect r, int selectionId, int searchId, String word) {
        Log.d(TAG, "showMeaningfulUI: selectionId = " + selectionId + ", searchId = " + searchId
                + ", word = " + word + ", rect = " + r);
        KerykeionResult tempResult = null;
        ArrayList<KerykeionResult> resultList = mResultTable.get(""+selectionId);
        
        tempResult = resultList.get(searchId);
            //only for simple string
        if(tempResult != null && !word.equals((String)tempResult.getResult())) {
            Log.e(TAG, "showMeaningfulUI: Words NOT MATCHING = " + (String)tempResult.getResult());
            //return;
        }

        
        if(tempResult == null) {
            Log.e(TAG, "showMeaningfulUI Error : unable to find result with word = " + word);
            return;
        }
        dismissMeaningfulUI();
        
        final HermesServiceManager hermesManager = new HermesServiceManager(mSbrCore.getContext());
        if (hermesManager != null) {
            Log.d(TAG, "showMeaningfulUI - call showHermes!!");
            hermesManager.showHermes(tempResult, r);
            mHermesUIShown = true;
        }

    }
    
    public void dismissMeaningfulUI() {
        if(mHermesUIShown) {
            final HermesServiceManager hermesManager = new HermesServiceManager(mSbrCore.getContext());
            if (hermesManager != null) {
                Log.d(TAG, "dismissMeaningfulUI - already being shown, hence send dismissHermes!!");
                hermesManager.dismissHermes();
            }
        }
    }
    
    public void clearParsingData() {
        if(mResultTable!= null) {
            mResultTable.clear();
            mResultTable = null;
        }
    }
}
