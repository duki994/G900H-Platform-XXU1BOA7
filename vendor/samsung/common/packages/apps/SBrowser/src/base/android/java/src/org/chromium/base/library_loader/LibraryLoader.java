// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.util.Log;
import android.content.res.AssetManager;
import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.base.JNINamespace;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.lzma.LzmaDecompressor;

import java.util.Arrays;
import java.util.ArrayList;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/**
 * This class provides functionality to load and register the native libraries.
 * Callers are allowed to separate loading the libraries from initializing them.
 * This may be an advantage for Android Webview, where the libraries can be loaded
 * by the zygote process, but then needs per process initialization after the
 * application processes are forked from the zygote process.
 *
 * The libraries may be loaded and initialized from any thread. Synchronization
 * primitives are used to ensure that overlapping requests from different
 * threads are handled sequentially.
 *
 * See also base/android/library_loader/library_loader_hooks.cc, which contains
 * the native counterpart to this class.
 */
@JNINamespace("base::android")
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    // Guards all access to the libraries
    private static final Object sLock = new Object();

    // One-way switch becomes true when the libraries are loaded.
    private static boolean sLoaded = false;

    // One-way switch becomes true when the libraries are initialized (
    // by calling nativeLibraryLoaded, which forwards to LibraryLoaded(...) in
    // library_loader_hooks.cc).
    private static boolean sInitialized = false;
    
    private static ArrayList<String> loadedSBrowseLib = new ArrayList<String>();
    private static File mAppLibDir;
    private static final int BUFFER_SIZE = 102400;
    private static String[] MANDATORY_LIBS = null;

    // TODO(cjhopman): Remove this once it's unused.
    /**
     * Doesn't do anything.
     */
    @Deprecated
    public static void setLibraryToLoad(String library) {
    }

    /**
     *  This method blocks until the library is fully loaded and initialized.
     */
    public static void ensureInitialized() throws ProcessInitException {
        synchronized (sLock) {
            if (sInitialized) {
                // Already initialized, nothing to do.
                return;
            }
            loadAlreadyLocked();
            initializeAlreadyLocked(CommandLine.getJavaSwitchesOrNull());
        }
    }

    /**
     * Checks if library is fully loaded and initialized.
     */
    public static boolean isInitialized() {
        synchronized (sLock) {
            return sInitialized;
        }
    }

    /**
     * Loads the library and blocks until the load completes. The caller is responsible
     * for subsequently calling ensureInitialized().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     *
     * @throws ProcessInitException if the native library failed to load.
     */
    public static void loadNow() throws ProcessInitException {
        synchronized (sLock) {
            loadAlreadyLocked();
        }
    }


    /**
     * initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     * @param initCommandLine The command line arguments that native command line will
     * be initialized with.
     */
    public static void initialize(String[] initCommandLine) throws ProcessInitException {
        synchronized (sLock) {
            initializeAlreadyLocked(initCommandLine);
        }
    }


    // Invoke System.loadLibrary(...), triggering JNI_OnLoad in native code
    private static void loadAlreadyLocked() throws ProcessInitException {
        try {
            if (!sLoaded) {
                assert !sInitialized;

                long startTime = System.currentTimeMillis();
                boolean useChromiumLinker = Linker.isUsed();

                if (useChromiumLinker)
                    Linker.prepareLibraryLoad();

                for (String library : NativeLibraries.LIBRARIES) {
                    Log.i(TAG, "Loading: " + library);

                    if(isSBrowserLibLoaded(library)) {
                        continue;
                    }                   
					
                    if (useChromiumLinker)
                        Linker.loadLibrary(library);
                    else
                        System.loadLibrary(library);
                }
                if (useChromiumLinker)
                    Linker.finishLibraryLoad();
                long stopTime = System.currentTimeMillis();
                Log.i("[APP_LAUNCH] " + TAG,
                       String.format("Time to load native libraries: %d ms (timestamps %d-%d)",
                       stopTime - startTime, startTime % 10000, stopTime % 10000));
                sLoaded = true;
            }
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED, e);
        }
        // Check that the version of the library we have loaded matches the version we expect
        Log.i(TAG, String.format(
                "Expected native library version number \"%s\"," +
                        "actual native library version number \"%s\"",
                NativeLibraries.VERSION_NUMBER,
                nativeGetVersionNumber()));
        if (!NativeLibraries.VERSION_NUMBER.equals(nativeGetVersionNumber())) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_WRONG_VERSION);
        }
    }
    
    public static boolean isSBrowserLibLoaded(String lib) {
         for (String loadedLib : loadedSBrowseLib) {
            if(loadedLib.contains("lib" + lib))
               return true;
         }
         return false;
    }

    public static void setSBrowserLoadedLib(String lib) {
         loadedSBrowseLib.add(lib);
    }

    public static boolean sBrowserMandatoryLibsExist() {
           for (String mandatoryLib : MANDATORY_LIBS) {
                File library = new File(mAppLibDir, mandatoryLib);
    
                if (!library.exists()) {   
                   return false;
                }    
           }
    
           return true;
    } 
    
    public static void loadSBrowserMandatoryLibs() {
        for (String mandatoryLib : MANDATORY_LIBS) {
            File library = new File(mAppLibDir, mandatoryLib);
    
            if (library.exists()) {    
                try {
                   Log.w(TAG, "Loading " + mandatoryLib); 
                   System.load(mAppLibDir.getAbsolutePath() + "/" + mandatoryLib);
                }
                catch (UnsatisfiedLinkError e) {
                   Log.w(TAG, "loadSBrowserMandatoryLibs exception: " + e.getMessage());   
                }
            
               setSBrowserLoadedLib(mandatoryLib);          
            }      
        }        
    }
    
    public static void loadLzmaLib() {
    	
     	if(isSBrowserLibLoaded("lzma"))
     	   return; 

      System.loadLibrary("lzma");         
      setSBrowserLoadedLib("lzma");
    }

    public static void unZipNativeLib(Context context, File appLibDir, String... mandatoryLibs) {            
      	 AssetManager manager = context.getResources().getAssets();
      	 mAppLibDir = appLibDir; 
      	 MANDATORY_LIBS = mandatoryLibs;

      	 loadLzmaLib();

        if (!mAppLibDir.exists()) {
           boolean success = mAppLibDir.mkdir();
     
           if (success)
               Log.i(TAG, "unZipNativeLib app_lib folder created ");
           else
               Log.i(TAG, "unZipNativeLib failed to create app_lib folder");
        }                       
     
        if (sBrowserMandatoryLibsExist()) {
            Log.i(TAG, "unZipNativeLib mandatory libs exist...");
            return;
        }   

        try { 
          for (String mandatoryLib : MANDATORY_LIBS) {
            File library = new File(mAppLibDir, mandatoryLib);
    
            if(Arrays.asList(manager.list("")).contains(mandatoryLib + ".lzma")) {             
               unZipFile(library, manager, mandatoryLib + ".lzma");
               library.setReadable(true, false);
            }        	
          }
       }
       catch (IOException IOException) {
          Log.i(TAG, "unZipNativeLib mIOException ...");
       } 

       //libsbrowser needs to be accessed by a renderer process.
       mAppLibDir.setExecutable(true, false);
       mAppLibDir.setReadable(true, false);  

    }

    public static void unZipFile(File paramFile, AssetManager manager, String libZipFile ) {        
      LzmaDecompressor decompressor;
      decompressor = new LzmaDecompressor(204800); 
      decompressor.Initialize(paramFile.getAbsolutePath());
      InputStream localInputStream = null;
      int m;
          
      try {        
           
         localInputStream = manager.open(libZipFile);
         byte[] buf = new byte[BUFFER_SIZE];      
         int count;
         while ((count = localInputStream.read(buf)) != -1) {                      
              m = decompressor.DecompressChunk((byte[])buf, count);              
              if (m < 0)
                 throw new IOException("unZipFile Decompressing " + paramFile.getAbsolutePath() + " failed!");   
         }                       
      } 
      catch (IOException IOException) {
         decompressor.Deinitialize();
      }        
      finally {  
       		try { 
           if (localInputStream != null)
              localInputStream.close();            
         }
         catch (IOException IOException) {
            Log.i(TAG, "unZipFile mIOException ...");
         }
      } 

      decompressor.Deinitialize(); 
      decompressor.delete();                                                
    }

    // Invoke base::android::LibraryLoaded in library_loader_hooks.cc
    private static void initializeAlreadyLocked(String[] initCommandLine)
            throws ProcessInitException {
        if (sInitialized) {
            return;
        }
        if (!nativeLibraryLoaded(initCommandLine)) {
            Log.e(TAG, "error calling nativeLibraryLoaded");
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_FAILED_TO_REGISTER_JNI);
        }
        // From this point on, native code is ready to use and checkIsReady()
        // shouldn't complain from now on (and in fact, it's used by the
        // following calls).
        sInitialized = true;
        CommandLine.enableNativeProxy();
        TraceEvent.setEnabledToMatchNative();
        // Record histogram for the Chromium linker.
        if (Linker.isUsed())
            nativeRecordChromiumAndroidLinkerHistogram(Linker.loadAtFixedAddressFailed(),
                                                    SysUtils.isLowEndDevice());
    }

    // Only methods needed before or during normal JNI registration are during System.OnLoad.
    // nativeLibraryLoaded is then called to register everything else.  This process is called
    // "initialization".  This method will be mapped (by generated code) to the LibraryLoaded
    // definition in base/android/library_loader/library_loader_hooks.cc.
    //
    // Return true on success and false on failure.
    private static native boolean nativeLibraryLoaded(String[] initCommandLine);

    // Method called to record statistics about the Chromium linker operation,
    // i.e. whether the library failed to be loaded at a fixed address, and
    // whether the device is 'low-memory'.
    private static native void nativeRecordChromiumAndroidLinkerHistogram(
         boolean loadedAtFixedAddressFailed,
         boolean isLowMemoryDevice);

    // Get the version of the native library. This is needed so that we can check we
    // have the right version before initializing the (rest of the) JNI.
    private static native String nativeGetVersionNumber();
}
