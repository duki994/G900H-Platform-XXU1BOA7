// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Build;
import android.util.Log;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Exposes system related information about the current device.
 */
public class SysUtils {
    // Any device that runs this or an older version of the system cannot be considered 'low-end'
    private static final int ANDROID_LOW_MEMORY_ANDROID_SDK_THRESHOLD =
            Build.VERSION_CODES.JELLY_BEAN_MR2;

    // A device reporting strictly more total memory in megabytes cannot be considered 'low-end'.
    private static final long ANDROID_LOW_MEMORY_DEVICE_THRESHOLD_MB = 512;

    private static final String TAG = "SysUtils";

    private static Boolean sLowEndDevice;

    private SysUtils() { }

    /**
     * Return the amount of physical memory on this device in kilobytes.
     * Note: the only reason this is public is for testability reason.
     * @return Amount of physical memory in kilobytes, or 0 if there was
     *         an error trying to access the information.
     *
     * Note that this is CalledByNative for testing purpose only.
     */
    @CalledByNative
    public static int amountOfPhysicalMemoryKB() {
        // Extract total memory RAM size by parsing /proc/meminfo, note that
        // this is exactly what the implementation of sysconf(_SC_PHYS_PAGES)
        // does. However, it can't be called because this method must be
        // usable before any native code is loaded.

        // An alternative is to use ActivityManager.getMemoryInfo(), but this
        // requires a valid ActivityManager handle, which can only come from
        // a valid Context object, which itself cannot be retrieved
        // during early startup, where this method is called. And making it
        // an explicit parameter here makes all call paths _much_ more
        // complicated.

        Pattern pattern = Pattern.compile("^MemTotal:\\s+([0-9]+) kB$");
        try {
            FileReader fileReader = new FileReader("/proc/meminfo");
            try {
                BufferedReader reader = new BufferedReader(fileReader);
                try {
                    String line;
                    for (;;) {
                        line = reader.readLine();
                        if (line == null) {
                            Log.w(TAG, "/proc/meminfo lacks a MemTotal entry?");
                            break;
                        }
                        Matcher m = pattern.matcher(line);
                        if (!m.find()) continue;

                        int totalMemoryKB = Integer.parseInt(m.group(1));
                        // Sanity check.
                        if (totalMemoryKB <= 1024) {
                            Log.w(TAG, "Invalid /proc/meminfo total size in kB: " + m.group(1));
                            break;
                        }

                        return totalMemoryKB;
                    }

                } catch (IOException e) {
		    e.printStackTrace();
		} finally {
                    try {
                    	if(reader != null){
                    	    reader.close();
                        }	
		    } catch (IOException e) {
			e.printStackTrace();
		    }
                }
            } finally {
                try {
                	if(fileReader != null){
                	    fileReader.close();
                	}	
		    } catch (IOException e) {
			e.printStackTrace();
		    }
            }
        } catch (FileNotFoundException e) {
            Log.w(TAG, "Cannot get total physical size from /proc/meminfo", e);
        }

        return 0;
    }

    /**
     * @return Whether or not this device should be considered a low end device.
     */
    @CalledByNative
    public static boolean isLowEndDevice() {
        if (sLowEndDevice == null) {
            sLowEndDevice = detectLowEndDevice();
        }
        return sLowEndDevice.booleanValue();
    }

    /**
     * @return Whether isLowEndDevice() has ever been called.
     */
    public static boolean isLowEndStateInitialized() {
        return (sLowEndDevice != null);
    }

    private static boolean detectLowEndDevice() {
        if (CommandLine.isInitialized()) {
            if (CommandLine.getInstance().hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)) {
                return true;
            }
            if (CommandLine.getInstance().hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)) {
                return false;
            }
        }

        if (Build.VERSION.SDK_INT <= ANDROID_LOW_MEMORY_ANDROID_SDK_THRESHOLD) {
            return false;
        }

        int ramSizeKB = amountOfPhysicalMemoryKB();
        return (ramSizeKB > 0 && ramSizeKB / 1024 < ANDROID_LOW_MEMORY_DEVICE_THRESHOLD_MB);
    }
}