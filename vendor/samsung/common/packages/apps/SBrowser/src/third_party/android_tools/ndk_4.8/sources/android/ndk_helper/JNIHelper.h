/*
 * Copyright 2013 The Android Open Source Project
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

#pragma once

#include <jni.h>
#include <errno.h>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <string>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, ndkHelper::JNIHelper::getAppName(), __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, ndkHelper::JNIHelper::getAppName(), __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, ndkHelper::JNIHelper::getAppName(), __VA_ARGS__))

namespace ndkHelper {

jclass retrieveClass(JNIEnv *jni, ANativeActivity* activity, const char* className);

/******************************************************************
 * Helpers to invoke Java methods
 * To use this class, add NDKHelper.java as a corresponding helpers in Java side
 */
class JNIHelper
{
private:
    static std::string _appName;

    ANativeActivity* _activity;
    jobject _objJNIHelper;
    jclass _clsJNIHelper;

    // POSIX mutex
    mutable pthread_mutex_t _mutex; //Mutex between app thread

    jstring getExternalFilesDir( JNIEnv *env );
    jclass retrieveClass(JNIEnv *jni, ANativeActivity* activity,
            const char* className);

    JNIHelper();
    ~JNIHelper();
public:
    void init( ANativeActivity* activity );
    bool readFile( const char* fileName, std::vector<uint8_t>& buffer );
    uint32_t loadTexture(const char* fileName );
    std::string convertString( const char* str, const char* encode );
    std::string getExternalFilesDir();

    //Audio helpers
    int32_t getNativeAudioBufferSize();
    int32_t getNativeAudioSampleRate();

    //Static members
    static JNIHelper* getInstance();
    static const char* getAppName() {
        return _appName.c_str();
    };

};

};  //namespace ndkHelper
