// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.app.ProgressDialog;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.provider.MediaStore;
import android.text.TextUtils;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.ui.R;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A dialog that is triggered from a file input field that allows a user to select a file based on
 * a set of accepted file types. The path of the selected file is passed to the native dialog.
 */
@JNINamespace("ui")
public class SelectFileDialog implements WindowAndroid.IntentCallback{
    private static final String IMAGE_TYPE = "image/";
    private static final String VIDEO_TYPE = "video/";
    private static final String AUDIO_TYPE = "audio/";
    private static final String ALL_IMAGE_TYPES = IMAGE_TYPE + "*";
    private static final String ALL_VIDEO_TYPES = VIDEO_TYPE + "*";
    private static final String ALL_AUDIO_TYPES = AUDIO_TYPE + "*";
    private static final String ANY_TYPES = "*/*";
    private static final String CAPTURE_IMAGE_DIRECTORY = "browser-photos";

    private final long mNativeSelectFileDialog;
    private List<String> mFileTypes;
    private boolean mCapture;
    private Uri mCameraOutputUri;
    //Samsung change ++
    private SbrSelectFileDownLoader mSbrSelectFileDownLoader;
    private static String  GALLERY_PICASA_PROVIDER = "com.sec.android.gallery3d.provider";
    private static String  DOWNLOAD_MESSAGE = "Starting Download";
    private static String  DOWNLOADABLE_FILE_NAME = "Attachment.jpg";    
    private String MYFILES_INTENT = "com.sec.android.app.myfiles.PICK_DATA";
    //Samsung change --

    private SelectFileDialog(long nativeSelectFileDialog) {
        mNativeSelectFileDialog = nativeSelectFileDialog;
    }

    /**
     * Creates and starts an intent based on the passed fileTypes and capture value.
     * @param fileTypes MIME types requested (i.e. "image/*")
     * @param capture The capture value as described in http://www.w3.org/TR/html-media-capture/
     * @param window The WindowAndroid that can show intents
     */
    @CalledByNative
    private void selectFile(String[] fileTypes, boolean capture, WindowAndroid window) {
        mFileTypes = new ArrayList<String>(Arrays.asList(fileTypes));
        mCapture = capture;

        Intent chooser = new Intent(Intent.ACTION_CHOOSER);
        Intent camera = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        mCameraOutputUri = Uri.fromFile(getFileForImageCapture());
        camera.putExtra(MediaStore.EXTRA_OUTPUT, mCameraOutputUri);
        Intent camcorder = new Intent(MediaStore.ACTION_VIDEO_CAPTURE);
        Intent soundRecorder = new Intent(
                MediaStore.Audio.Media.RECORD_SOUND_ACTION);
        
        //Samsung change ++
        Intent intentMyFiles = new Intent(MYFILES_INTENT);
        //Samsung change --

        // Quick check - if the |capture| parameter is set and |fileTypes| has the appropriate MIME
        // type, we should just launch the appropriate intent. Otherwise build up a chooser based on
        // the accept type and then display that to the user.
        if (captureCamera()) {
            if (window.showIntent(camera, this, R.string.low_memory_error)) return;
        } else if (captureCamcorder()) {
            if (window.showIntent(camcorder, this, R.string.low_memory_error)) return;
        } else if (captureMicrophone()) {
            if (window.showIntent(soundRecorder, this, R.string.low_memory_error)) return;
        }

        Intent getContentIntent = new Intent(Intent.ACTION_GET_CONTENT);
        getContentIntent.addCategory(Intent.CATEGORY_OPENABLE);
        ArrayList<Intent> extraIntents = new ArrayList<Intent>();
        if (!noSpecificType()) {
            // Create a chooser based on the accept type that was specified in the webpage. Note
            // that if the web page specified multiple accept types, we will have built a generic
            // chooser above.
            if (shouldShowImageTypes()) {
                extraIntents.add(camera);
                getContentIntent.setType(ALL_IMAGE_TYPES);
            } else if (shouldShowVideoTypes()) {
                extraIntents.add(camcorder);
                getContentIntent.setType(ALL_VIDEO_TYPES);
            } else if (shouldShowAudioTypes()) {
                extraIntents.add(soundRecorder);
                getContentIntent.setType(ALL_AUDIO_TYPES);
            }
        }

        if (extraIntents.isEmpty()) {
            // We couldn't resolve an accept type, so fallback to a generic chooser.
            getContentIntent.setType(ANY_TYPES);
            extraIntents.add(camera);
            extraIntents.add(camcorder);
            extraIntents.add(soundRecorder);
            //Samsung change ++
            extraIntents.add(intentMyFiles);
            //Samsung change --
        }

        chooser.putExtra(Intent.EXTRA_INITIAL_INTENTS,
                extraIntents.toArray(new Intent[] { }));

        chooser.putExtra(Intent.EXTRA_INTENT, getContentIntent);

        if (!window.showIntent(chooser, this, R.string.low_memory_error)) {
            onFileNotSelected();
        }
    }

    /**
     * Get a file for the image capture in the CAPTURE_IMAGE_DIRECTORY directory.
     */
    private File getFileForImageCapture() {
        File externalDataDir = Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DCIM);
        File cameraDataDir = new File(externalDataDir.getAbsolutePath() +
                File.separator + CAPTURE_IMAGE_DIRECTORY);
        if (!cameraDataDir.exists() && !cameraDataDir.mkdirs()) {
            cameraDataDir = externalDataDir;
        }
        File photoFile = new File(cameraDataDir.getAbsolutePath() +
                File.separator + System.currentTimeMillis() + ".jpg");
        return photoFile;
    }

    /**
     * @return the display name of the @code uri if present in the database
     *  or an empty string otherwise.
     */
    private String resolveFileName(Uri uri, ContentResolver contentResolver) {
        if (contentResolver == null || uri == null) return "";
        Cursor cursor = null;
        try {
            cursor = contentResolver.query(uri, null, null, null, null);

            if (cursor != null && cursor.getCount() >= 1) {
                cursor.moveToFirst();
                int index = cursor.getColumnIndex(MediaStore.MediaColumns.DISPLAY_NAME);
                if (index > -1) return cursor.getString(index);
            }
        } catch (NullPointerException e) {
            // Some android models don't handle the provider call correctly.
            // see crbug.com/345393
            return "";
        } finally {
            if (cursor != null ) {
                cursor.close();
            }
        }
        return "";
    }

    /**
     * Callback method to handle the intent results and pass on the path to the native
     * SelectFileDialog.
     * @param window The window that has access to the application activity.
     * @param resultCode The result code whether the intent returned successfully.
     * @param contentResolver The content resolver used to extract the path of the selected file.
     * @param results The results of the requested intent.
     */
    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode,
            ContentResolver contentResolver, Intent results) {
        if (resultCode != Activity.RESULT_OK) {
            onFileNotSelected();
            return;
        }

        if (results == null) {
            // If we have a successful return but no data, then assume this is the camera returning
            // the photo that we requested.
            nativeOnFileSelected(mNativeSelectFileDialog, mCameraOutputUri.getPath(), "");

            // Broadcast to the media scanner that there's a new photo on the device so it will
            // show up right away in the gallery (rather than waiting until the next time the media
            // scanner runs).
            window.sendBroadcast(new Intent(
                    Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, mCameraOutputUri));
            return;
        }

        if (ContentResolver.SCHEME_FILE.equals(results.getData().getScheme())) {
            nativeOnFileSelected(mNativeSelectFileDialog,
                    results.getData().getSchemeSpecificPart(), "");
            return;
        }

        if (ContentResolver.SCHEME_CONTENT.equals(results.getScheme())  && !GALLERY_PICASA_PROVIDER.equals(results.getData().getAuthority())) {
            nativeOnFileSelected(mNativeSelectFileDialog,
                                 results.getData().toString(),
                                 resolveFileName(results.getData(),
                                                 contentResolver));
            return;
        }

      //Samsung change ++
        boolean isAsyncTaskRunning = false;
        
        if (ContentResolver.SCHEME_CONTENT.equals(results.getScheme())  && GALLERY_PICASA_PROVIDER.equals(results.getData().getAuthority())) {
        	if(mSbrSelectFileDownLoader == null){
            	mSbrSelectFileDownLoader = new SbrSelectFileDownLoader(SelectFileDialog.this);
            }
        	mSbrSelectFileDownLoader.startDownload(results,contentResolver,window);
        	isAsyncTaskRunning = true;
        }
        
        if(!isAsyncTaskRunning){
	        onFileNotSelected();
	        window.showError(R.string.opening_file_error);
        }
      //Samsung change --
    }

    private void onFileNotSelected() {
        nativeOnFileNotSelected(mNativeSelectFileDialog);
    }

    //Samsung change ++
    public void setResult(String filePath, String displayName, boolean result){
    	if(!result){
    		nativeOnFileNotSelected(mNativeSelectFileDialog);
    	}else{
    		nativeOnFileSelected(mNativeSelectFileDialog,
    				filePath, displayName);
    	}
    	mSbrSelectFileDownLoader = null;
    }
  //Samsung change --
    
    private boolean noSpecificType() {
        // We use a single Intent to decide the type of the file chooser we display to the user,
        // which means we can only give it a single type. If there are multiple accept types
        // specified, we will fallback to a generic chooser (unless a capture parameter has been
        // specified, in which case we'll try to satisfy that first.
        return mFileTypes.size() != 1 || mFileTypes.contains(ANY_TYPES);
    }

    private boolean shouldShowTypes(String allTypes, String specificType) {
        if (noSpecificType() || mFileTypes.contains(allTypes)) return true;
        return acceptSpecificType(specificType);
    }

    private boolean shouldShowImageTypes() {
        return shouldShowTypes(ALL_IMAGE_TYPES, IMAGE_TYPE);
    }

    private boolean shouldShowVideoTypes() {
        return shouldShowTypes(ALL_VIDEO_TYPES, VIDEO_TYPE);
    }

    private boolean shouldShowAudioTypes() {
        return shouldShowTypes(ALL_AUDIO_TYPES, AUDIO_TYPE);
    }

    private boolean acceptsSpecificType(String type) {
        return mFileTypes.size() == 1 && TextUtils.equals(mFileTypes.get(0), type);
    }

    private boolean captureCamera() {
        return mCapture && acceptsSpecificType(ALL_IMAGE_TYPES);
    }

    private boolean captureCamcorder() {
        return mCapture && acceptsSpecificType(ALL_VIDEO_TYPES);
    }

    private boolean captureMicrophone() {
        return mCapture && acceptsSpecificType(ALL_AUDIO_TYPES);
    }

    private boolean acceptSpecificType(String accept) {
        for (String type : mFileTypes) {
            if (type.startsWith(accept)) {
                return true;
            }
        }
        return false;
    }
    
    //Samsung change ++
    // FIXME: need native changes to create
    // customised selectfileDialog:nagaraj.l@samsung.com
    private static class SbrSelectFileDownLoader {

	    private SelectFileDialog mSelectFileDialog;
	    private ProgressDialog mProgressDialog;

	    public SbrSelectFileDownLoader(SelectFileDialog selectFileDialog) {
		mSelectFileDialog = selectFileDialog;
	    }

	    public void startDownload(Intent results, ContentResolver contentResolver, final WindowAndroid window) {

		Cursor c = contentResolver.query(results.getData(),
			new String[] { MediaStore.Images.Media.DATA }, null, null, null);
		if (c != null) {
		    if (c.getCount() == 1) {
			c.moveToFirst();
			final String path = c.getString(0);

			if (path != null) {
			    // (com.android.gallery3d.provider) we download the
			    // images for Picasa Web Album images & then pass to
			    // native engine
			    new AsyncTask<String, Void, String>() {
			    	
			    boolean isSuccess = true;
				@Override
				protected void onPreExecute() {

				    CharSequence message = DOWNLOAD_MESSAGE;
				    mProgressDialog = ProgressDialog.show(window.getContext(), null,
					    message, true, true, null);

				    mProgressDialog
					    .setOnCancelListener(new DialogInterface.OnCancelListener() {
						@Override
						public void onCancel(DialogInterface arg0) {
						    cancel(true);
						    isSuccess = false;
						    mSelectFileDialog.setResult("", "", false);
						}
					    });
				}

				@Override
				protected String doInBackground(String... arg0) {

				    File downloadPath = Environment
					    .getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES);
				    File downloadedFile = new File(downloadPath,DOWNLOADABLE_FILE_NAME );
				    // Make sure the Pictures directory exists.
				    downloadPath.mkdirs();
				    URL url;
				    InputStream input = null;
				    OutputStream output = null;

				    try {
					url = new URL(arg0[0]);

					/* Open a connection to that URL. */
					HttpURLConnection ucon = (HttpURLConnection) url.openConnection();
					input = new BufferedInputStream(ucon.getInputStream());
					output = new FileOutputStream(downloadedFile);

					byte data[] = new byte[1024];
					int count;
					while ((count = input.read(data)) != -1) {
					    output.write(data, 0, count);
					}

					output.flush();
					input.close();
					output.close();
				    } catch (MalformedURLException e) {
				    isSuccess = false;
					e.printStackTrace();
				    } catch (IOException e) {
				    isSuccess = false;
					e.printStackTrace();
				    } finally {
					try {
					    if (input != null)
						input.close();
					} catch (IOException e) {
					    e.printStackTrace();
					}
					try {
					    if (output != null)
						output.close();
					} catch (IOException e) {
					    e.printStackTrace();
					}
				    }
				    return downloadedFile.getAbsolutePath();
				}

				@Override
				protected void onPostExecute(String result) {
					if (isSuccess) {
						mSelectFileDialog.setResult(result, "",
								true);
					} else {
					        window.showError(R.string.opening_file_error);
						mSelectFileDialog.setResult("", "", false);
					}
				    if (mProgressDialog != null) {
					mProgressDialog.dismiss();
					mProgressDialog = null;
				    }
				}

			    }.execute(path);
			}
		    }
		    c.close();
		}
	    }

	}
  //Samsung change --

    @CalledByNative
    private static SelectFileDialog create(long nativeSelectFileDialog) {
        return new SelectFileDialog(nativeSelectFileDialog);
    }

    private native void nativeOnFileSelected(long nativeSelectFileDialogImpl,
            String filePath, String displayName);
    private native void nativeOnFileNotSelected(long nativeSelectFileDialogImpl);
}
