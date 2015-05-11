// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.JNINamespace;
import org.chromium.content.browser.input.gamepad_mapping.CanonicalAxisIndex;
import org.chromium.content.browser.input.gamepad_mapping.CanonicalButtonIndex;

import java.util.Arrays;

/**
 * Device specific input data converter for Gamepad API.
 * Implemented per device by subclasses.
 */
@JNINamespace("content")
abstract class GamepadDataMapper {
    protected static final String PS3_SIXAXIS_DEVICE_NAME = "Sony PLAYSTATION(R)3 Controller";
    protected static final String SAMSUNG_EI_GP20_DEVICE_NAME = "Samsung Game Pad EI-GP20";
    protected static final String STANDARD_GAMEPAD_NAME = "(STANDARD_GAMEPAD)";
    protected static final String STANDARD_MAPPING = "standard";

    protected final WebGamepadData mData;

    public abstract WebGamepadData map(float[] axes, boolean[] buttons);

    // Factory method.
    public static GamepadDataMapper createDataMapper(String deviceName) {
        if (deviceName.equals(PS3_SIXAXIS_DEVICE_NAME))
            return new PS3SixAxisGamepadDataMapper();
        if (deviceName.equals(SAMSUNG_EI_GP20_DEVICE_NAME))
            return new SamsungEI_GP20GamepadDataMapper();

        return new GenericGamepadDataMapper(deviceName);
    }

    protected GamepadDataMapper(String deviceName) {
        mData = new WebGamepadData();
        mData.id = deviceName + " " + STANDARD_GAMEPAD_NAME;
        mData.mapping = STANDARD_MAPPING;
        mData.axes = new float[CanonicalAxisIndex.NUM_CANONICAL_AXES];
        mData.buttons = new float[CanonicalButtonIndex.NUM_CANONICAL_BUTTONS];
    }

    protected void clearAxesAndButtons() {
        Arrays.fill(mData.axes, 0);
        Arrays.fill(mData.buttons, 0);
    }

    // Most of the time we jut use 1 for pressed and 0 for not pressed.
    // Note that however some devices can represent some buttons of the standard gamepad
    // with analog values so we still want to store the button values as floats.
    // For example the Samsung EI-GP20 maps dpad to AXIS_HAT_X and AXIS_HAT_Y.
    protected float buttonValue(boolean pressed) { return pressed ? 1.0f : 0.0f; }

    protected void mapCommonXYAxes(float[] axes) {
        mData.axes[CanonicalAxisIndex.AXIS_LEFT_STICK_X] = axes[MotionEvent.AXIS_X];
        mData.axes[CanonicalAxisIndex.AXIS_LEFT_STICK_Y] = axes[MotionEvent.AXIS_Y];
    }

    protected void mapCommonTriggerAxes(float[] axes) {
        mData.buttons[CanonicalButtonIndex.BUTTON_LEFT_SHOULDER] = axes[MotionEvent.AXIS_LTRIGGER];
        mData.buttons[CanonicalButtonIndex.BUTTON_RIGHT_SHOULDER] = axes[MotionEvent.AXIS_RTRIGGER];
    }

    protected void mapCommonTriggerButtons(boolean[] buttons) {
        boolean l1 = buttons[KeyEvent.KEYCODE_BUTTON_L1];
        boolean r1 = buttons[KeyEvent.KEYCODE_BUTTON_R1];
        mData.buttons[CanonicalButtonIndex.BUTTON_LEFT_TRIGGER] = buttonValue(l1);
        mData.buttons[CanonicalButtonIndex.BUTTON_RIGHT_TRIGGER] = buttonValue(r1);
    }

    protected void mapCommonThumbstickButtons(boolean[] buttons) {
        boolean thumbL = buttons[KeyEvent.KEYCODE_BUTTON_THUMBL];
        boolean thumbR = buttons[KeyEvent.KEYCODE_BUTTON_THUMBR];
        mData.buttons[CanonicalButtonIndex.BUTTON_LEFT_THUMBSTICK] = buttonValue(thumbL);
        mData.buttons[CanonicalButtonIndex.BUTTON_RIGHT_THUMBSTICK] = buttonValue(thumbR);
    }

    protected void mapCommonStartSelectButtons(boolean[] buttons) {
        boolean select = buttons[KeyEvent.KEYCODE_BUTTON_SELECT];
        boolean start = buttons[KeyEvent.KEYCODE_BUTTON_START];
        mData.buttons[CanonicalButtonIndex.BUTTON_BACK_SELECT] = buttonValue(select);
        mData.buttons[CanonicalButtonIndex.BUTTON_START] = buttonValue(start);
    }

    protected void mapCommonDpadButtons(boolean[] buttons) {
        boolean dpadDown = buttons[KeyEvent.KEYCODE_DPAD_DOWN];
        boolean dpadUp = buttons[KeyEvent.KEYCODE_DPAD_UP];
        boolean dpadLeft = buttons[KeyEvent.KEYCODE_DPAD_LEFT];
        boolean dpadRight = buttons[KeyEvent.KEYCODE_DPAD_RIGHT];
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_DOWN] = buttonValue(dpadDown);
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_UP] = buttonValue(dpadUp);
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_LEFT] = buttonValue(dpadLeft);
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_RIGHT] = buttonValue(dpadRight);
    }

    protected void mapCommonXYABButtons(boolean[] buttons) {
        boolean bA = buttons[KeyEvent.KEYCODE_BUTTON_A];
        boolean bB = buttons[KeyEvent.KEYCODE_BUTTON_B];
        boolean bX = buttons[KeyEvent.KEYCODE_BUTTON_X];
        boolean bY = buttons[KeyEvent.KEYCODE_BUTTON_Y];
        mData.buttons[CanonicalButtonIndex.BUTTON_PRIMARY] = buttonValue(bA);
        mData.buttons[CanonicalButtonIndex.BUTTON_SECONDARY] = buttonValue(bB);
        mData.buttons[CanonicalButtonIndex.BUTTON_TERTIARY] = buttonValue(bX);
        mData.buttons[CanonicalButtonIndex.BUTTON_QUATERNARY] = buttonValue(bY);
    }
}

// This is a last resort if we find a device that we don't know about.
// The Android API is general enough that this can be better than nothing
// but we should not really rely on this.
class GenericGamepadDataMapper extends GamepadDataMapper {
    private final String mDeviceName;

    GenericGamepadDataMapper(String deviceName) {
        super(deviceName);
        mDeviceName = deviceName;
    }

    // Find something for right stick x and y.
    private void mapRightSticks(float[] axes) {
        int position = CanonicalAxisIndex.AXIS_RIGHT_STICK_X;
        // position + 1 is AXIS_RIGHT_STICK_Y.
        assert mData.axes.length > position + 1;
        float x = axes[MotionEvent.AXIS_RX];
        float y = axes[MotionEvent.AXIS_RY];
        if (x != 0 || y != 0) {
            mData.axes[position++] = x;
            mData.axes[position] = y;
            return;
        }
        x = axes[MotionEvent.AXIS_Z];
        y = axes[MotionEvent.AXIS_RZ];
        if (x != 0 || y != 0) {
            mData.axes[position++] = x;
            mData.axes[position] = y;
            return;
        }
        x = axes[MotionEvent.AXIS_HAT_X];
        y = axes[MotionEvent.AXIS_HAT_Y];
        if (x != 0 || y != 0) {
            mData.axes[position++] = x;
            mData.axes[position] = y;
        }
    }

    public WebGamepadData map(float[] axes, boolean[] buttons) {
        clearAxesAndButtons();

        mapCommonXYAxes(axes);
        mapRightSticks(axes);
        mapCommonTriggerAxes(axes);
        mapCommonXYABButtons(buttons);
        mapCommonTriggerButtons(buttons);
        mapCommonThumbstickButtons(buttons);
        mapCommonStartSelectButtons(buttons);
        mapCommonDpadButtons(buttons);

        // TODO(b.kelemen): meta is missing.

        return mData;
    }
}

class PS3SixAxisGamepadDataMapper extends GamepadDataMapper {
    PS3SixAxisGamepadDataMapper() {
        super(PS3_SIXAXIS_DEVICE_NAME);
    }

    public WebGamepadData map(float[] axes, boolean[] buttons) {
        clearAxesAndButtons();

        mapCommonXYAxes(axes);

        mData.axes[CanonicalAxisIndex.AXIS_RIGHT_STICK_X] = axes[MotionEvent.AXIS_Z];
        mData.axes[CanonicalAxisIndex.AXIS_RIGHT_STICK_Y] = axes[MotionEvent.AXIS_RZ];

        mapCommonTriggerAxes(axes);

        boolean bA = buttons[KeyEvent.KEYCODE_BUTTON_A];
        boolean bB = buttons[KeyEvent.KEYCODE_BUTTON_B];
        boolean bX = buttons[KeyEvent.KEYCODE_BUTTON_X];
        boolean bY = buttons[KeyEvent.KEYCODE_BUTTON_Y];
        mData.buttons[CanonicalButtonIndex.BUTTON_PRIMARY] = buttonValue(bX);
        mData.buttons[CanonicalButtonIndex.BUTTON_SECONDARY] = buttonValue(bY);
        mData.buttons[CanonicalButtonIndex.BUTTON_TERTIARY] = buttonValue(bA);
        mData.buttons[CanonicalButtonIndex.BUTTON_QUATERNARY] = buttonValue(bB);

        mapCommonTriggerButtons(buttons);
        mapCommonThumbstickButtons(buttons);
        mapCommonStartSelectButtons(buttons);
        mapCommonDpadButtons(buttons);

        // TODO(b.kelemen): PS button is missing. Looks like it is swallowed by Android
        // but probably there is a way to configure otherwise and in this case we should
        // handle it.

        return mData;
    }
}

class SamsungEI_GP20GamepadDataMapper extends GamepadDataMapper {
    SamsungEI_GP20GamepadDataMapper() {
        super(SAMSUNG_EI_GP20_DEVICE_NAME);
    }

    public WebGamepadData map(float[] axes, boolean[] buttons) {
        clearAxesAndButtons();

        mapCommonXYAxes(axes);

        mData.axes[CanonicalAxisIndex.AXIS_RIGHT_STICK_X] = axes[MotionEvent.AXIS_RX];
        mData.axes[CanonicalAxisIndex.AXIS_RIGHT_STICK_Y] = axes[MotionEvent.AXIS_RY];

        mapCommonXYABButtons(buttons);
        mapCommonTriggerButtons(buttons);
        mapCommonStartSelectButtons(buttons);

        float hatX = axes[MotionEvent.AXIS_HAT_X];
        float hatY = axes[MotionEvent.AXIS_HAT_Y];
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_LEFT] = hatX == -1 ? 1 : 0;
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_RIGHT] = hatX == 1 ? 1 : 0;
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_UP] = hatY == -1 ? 1 : 0;
        mData.buttons[CanonicalButtonIndex.BUTTON_DPAD_DOWN] = hatY == 1 ? 1 : 0;

        return mData;
    }
};

// TODO(b.kelemen): add more implementations. It would be nice to support at least those that are
// supported on Linux.
