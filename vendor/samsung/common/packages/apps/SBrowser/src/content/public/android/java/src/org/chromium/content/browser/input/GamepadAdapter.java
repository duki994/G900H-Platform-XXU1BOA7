// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.hardware.input.InputManager;
import android.view.InputDevice;
import android.view.InputDevice.MotionRange;
import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Java counterpart of GamepadPlatformDataFetcherAndroid.
 * Manages game input devices and feed Gamepad API with input data.
 * GamepadPlatformDataFetcherAndroid is merely a wrepper around this.
 * Native callable methods called by GamepadPlatformDataFetcherAndroid on the poller thread
 * which is a native thread without a java looper. Events are processed on the UI thread.
 */
@JNINamespace("content")
public class GamepadAdapter implements InputManager.InputDeviceListener {

    private static final int NUM_WEB_GAMEPADS = 4;

    private static GamepadAdapter instance;

    private InputManager mInputManager;
    private InputDeviceHandler[] mDeviceHandlers;
    private final Object mDeviceHandlersLock = new Object();
    private boolean mDataRequested;
    private boolean mIsPaused;
    private long mNativeDataFetcher;
    private int mAttachedToWindowCounter;

    private static void initializeInstance() {
        if (instance == null) {
            instance = new GamepadAdapter();
        }
    }

    /**
     * Notifies GamepadAdapter that a {@link ContentView} is attached to a window so it should
     * be prepared for input. Must be called before {@link onMotionEvent} or {@link onKeyEvent}.
     */
    public static void onAttachedToWindow(Context context) {
        assert ThreadUtils.runningOnUiThread();
        initializeInstance();
        instance.attachedToWindow(context);
    }

    private void attachedToWindow(Context context) {
        if (mAttachedToWindowCounter++ == 0) {
            mInputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
            initializeDevices();
            mInputManager.registerInputDeviceListener(this, null);
        }
    }

    private static boolean isAttached() {
        return instance != null && instance.mAttachedToWindowCounter > 0;
    }

    /**
     * Notifies GamepadAdapter that a {@link ContentView} is detached from it's window.
     */
    public static void onDetachedFromWindow() {
        assert ThreadUtils.runningOnUiThread();
        assert isAttached();
        instance.detachedFromWindow();
    }

    private void detachedFromWindow() {
        if (--mAttachedToWindowCounter == 0) {
            synchronized (mDeviceHandlersLock) {
                for (int i = 0; i < NUM_WEB_GAMEPADS; i++) {
                    mDeviceHandlers[i] = null;
                }
            }
            mInputManager.unregisterInputDeviceListener(this);
            mInputManager = null;
        }
    }

    private void initializeDevices() {
        assert ThreadUtils.runningOnUiThread();
        InputDeviceHandler[] handlers = new InputDeviceHandler[NUM_WEB_GAMEPADS];
        int[] ids = mInputManager.getInputDeviceIds();
        if (ids == null) return;

        int activeDevices = 0;
        for (int i = 0; i < ids.length && activeDevices < NUM_WEB_GAMEPADS; i++) {
            InputDevice device = mInputManager.getInputDevice(ids[i]);
            if (isGameDevice(device)) {
                handlers[activeDevices++] = new InputDeviceHandler(device);
            }
        }
        synchronized (mDeviceHandlersLock) {
            mDeviceHandlers = handlers;
        }
    }

    // ---------------------------------------------------
    // Implementation of InputManager.InputDeviceListener.
    @Override
    public void onInputDeviceAdded(int deviceId) {
        ThreadUtils.assertOnUiThread();
        InputDevice device = mInputManager.getInputDevice(deviceId);
        if (device == null || !isGameDevice(device))
            return;
        int index = nextAvailableIndex();
        if (index == -1)
            return;
        synchronized (mDeviceHandlersLock) {
            mDeviceHandlers[index] = new InputDeviceHandler(device);
        }
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        ThreadUtils.assertOnUiThread();
        int index = indexForDeviceId(deviceId);
        if (index == -1)
            return;
        synchronized (mDeviceHandlersLock) {
            mDeviceHandlers[index] = null;
        }
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        ThreadUtils.assertOnUiThread();
        int index = indexForDeviceId(deviceId);
        if (index == -1) {
            index = nextAvailableIndex();
            if (index == -1) return;
        }
        InputDevice device = mInputManager.getInputDevice(deviceId);
        synchronized (mDeviceHandlersLock) {
            mDeviceHandlers[index] = null;
            if (isGameDevice(device)) {
                mDeviceHandlers[index] = new InputDeviceHandler(device);
            }
        }
    }
    // ---------------------------------------------------

    /**
     * Handles motion events from gamepad devices.
     *
     * @return True if the event has been consumed.
     */
    public static boolean onMotionEvent(MotionEvent event) {
        assert isAttached();
        return instance.handleMotionEvent(event);
    }

    private boolean handleMotionEvent(MotionEvent event) {
        if (!mDataRequested) return false;
        InputDeviceHandler handler = handlerForDeviceId(event.getDeviceId());
        if (handler == null) return false;
        if (!isGameEvent(event)) return false;

        handler.handleMotionEvent(event);
        return true;
    }

    /**
     * Handles key events from gamepad devices.
     *
     * @return True if the event has been consumed.
     */
    public static boolean onKeyEvent(KeyEvent event) {
        assert isAttached();
        return instance.handleKeyEvent(event);
    }

    private boolean handleKeyEvent(KeyEvent event) {
        if (!mDataRequested) return false;
        if (event.getAction() != KeyEvent.ACTION_DOWN
                && event.getAction() != KeyEvent.ACTION_UP) {
            return false;
        }
        InputDeviceHandler handler = handlerForDeviceId(event.getDeviceId());
        if (handler == null) return false;
        int keyCode = event.getKeyCode();
        if (!isGameKey(keyCode)) return false;

        boolean isDown = event.getAction() == KeyEvent.ACTION_DOWN;
        handler.handleKeyEvent(keyCode, isDown, event.getEventTime());
        return true;
    }

    @CalledByNative
    static void setDataRequested(long nativeDataFetcher, boolean requested) {
        initializeInstance();
        instance.mNativeDataFetcher = nativeDataFetcher;
        instance.mDataRequested = requested;
    }

    // Called on polling thread.
    @CalledByNative
    static void getGamepadData() {
        assert instance != null;
        instance.collectGamepadData();
    }

    private void collectGamepadData() {
        assert mNativeDataFetcher != 0;

        if (!mDataRequested) {
            // Clear input to avoid anomalies because we don't watch events when data is not
            // requested. We can miss the release of a button and falsely report that it's pushed
            // when data is requested again. We do this here after setting mDataRequested to
            // true instead in setDataRequested when setting it to false because this
            // way no locking is needed. This can race with onMotionEvent or onKeyEvent
            // but it couldn't result in inconsistent data.
            mDataRequested = true;
            clearInput();
        }

        synchronized (mDeviceHandlersLock) {
            for (int i = 0; i < NUM_WEB_GAMEPADS; i++) {
                if (mDeviceHandlers[i] == null) {
                    nativeRefreshDevice(mNativeDataFetcher, i, false, null, null, 0, null, null);
                } else {
                    InputDeviceHandler handler = mDeviceHandlers[i];
                    WebGamepadData data = handler.produceWebData();
                    nativeRefreshDevice(mNativeDataFetcher, i, true, data.id, data.mapping,
                            handler.getTimestamp(), data.axes, data.buttons);
                }
            }
        }
    }

    void clearInput() {
        for (int i = 0; i < mDeviceHandlers.length; i++) {
            if (mDeviceHandlers[i] != null) {
                mDeviceHandlers[i].clearInput();
            }
        }
    }

    private int nextAvailableIndex() {
        for (int i = 0; i < mDeviceHandlers.length; i++) {
            if (mDeviceHandlers[i] == null) return i;
        }
        return -1;
    }

    private int indexForDeviceId(int deviceId) {
        for (int i = 0; i < mDeviceHandlers.length; i++) {
            if (mDeviceHandlers[i] != null &&
                mDeviceHandlers[i].getInputDevice().getId() == deviceId)
                return i;
        }
        return -1;
    }

    private InputDeviceHandler handlerForDeviceId(int deviceId) {
        int index = indexForDeviceId(deviceId);
        return index == -1 ? null : mDeviceHandlers[index];
    }

    private static boolean isGameDevice(InputDevice device) {
        return (device.getSources() & InputDevice.SOURCE_JOYSTICK) != 0;
    }

    private static boolean isGameEvent(MotionEvent event) {
        return (event.getSource() & InputDevice.SOURCE_JOYSTICK) != 0
                    && event.getAction() == MotionEvent.ACTION_MOVE;
    }

    private static boolean isGameKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_CENTER:
                return true;
            default:
                return KeyEvent.isGamepadButton(keyCode);
        }
    }

    private static class InputDeviceHandler {
        private final InputDevice mDevice;
        private long mTimestamp;
        private final int[] mAxes;

        // Apparently all axis id's and keycodes are less then 256. Given this the most effective
        // representation of an associative array is simply an array.
        private final float[] mAxisValues = new float[256];
        private final boolean[] mButtonsPressedStates = new boolean[256];

        private final GamepadDataMapper mMapper;
        private final Object mLock = new Object();

        InputDevice getInputDevice() { return mDevice; }
        long getTimestamp() { return mTimestamp; }

        InputDeviceHandler(InputDevice device) {
            assert isGameDevice(device);
            mDevice = device;
            mMapper = GamepadDataMapper.createDataMapper(device.getName());

            List<MotionRange> ranges = device.getMotionRanges();
            mAxes = new int[ranges.size()];
            int i = 0;
            for (MotionRange range : ranges) {
                if ((range.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
                    int axis = range.getAxis();
                    assert axis < 256;
                    mAxes[i++] = axis;
                }
            }
        }

        // Called on UI thread.
        void handleMotionEvent(MotionEvent event) {
            synchronized (mLock) {
                mTimestamp = event.getEventTime();
                for (int i = 0; i < mAxes.length; i++) {
                    int axis = mAxes[i];
                    mAxisValues[axis] = event.getAxisValue(axis);
                }
            }
        }

        // Called on UI thread.
        void handleKeyEvent(int keyCode, boolean isDown, long timestamp) {
            synchronized (mLock) {
                mTimestamp = timestamp;
                assert keyCode < 256;
                mButtonsPressedStates[keyCode] = isDown;
            }
        }

        // Called on polling thread.
        WebGamepadData produceWebData() {
            synchronized (mLock) {
                return mMapper.map(mAxisValues, mButtonsPressedStates);
            }
        }

        // Called on polling thread.
        void clearInput() {
            synchronized (mLock) {
                Arrays.fill(mAxisValues, 0);
                Arrays.fill(mButtonsPressedStates, false);
            }
        }
    }

    private native void nativeRefreshDevice(long nativeGamepadPlatformDataFetcherAndroid,
            int index, boolean connected, String id, String mapping, long timestamp,
            float[] axes, float[] buttons);
}
