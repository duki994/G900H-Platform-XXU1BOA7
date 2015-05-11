// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/avfoundation_glue.h"

#include <dlfcn.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "media/base/media_switches.h"

namespace {

// This class is used to retrieve AVFoundation NSBundle and library handle. It
// must be used as a LazyInstance so that it is initialised once and in a
// thread-safe way. Normally no work is done in constructors: LazyInstance is
// an exception.
class AVFoundationInternal {
 public:
  AVFoundationInternal() {
    bundle_ = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];

    const char* path = [[bundle_ executablePath] fileSystemRepresentation];
    CHECK(path);
    library_handle_ = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    CHECK(library_handle_) << dlerror();

    struct {
      NSString** loaded_string;
      const char* symbol;
    } av_strings[] = {
        {&AVCaptureDeviceWasConnectedNotification_,
         "AVCaptureDeviceWasConnectedNotification"},
        {&AVCaptureDeviceWasDisconnectedNotification_,
         "AVCaptureDeviceWasDisconnectedNotification"},
        {&AVMediaTypeVideo_, "AVMediaTypeVideo"},
        {&AVMediaTypeAudio_, "AVMediaTypeAudio"},
        {&AVMediaTypeMuxed_, "AVMediaTypeMuxed"},
        {&AVCaptureSessionRuntimeErrorNotification_,
         "AVCaptureSessionRuntimeErrorNotification"},
        {&AVCaptureSessionDidStopRunningNotification_,
         "AVCaptureSessionDidStopRunningNotification"},
        {&AVCaptureSessionErrorKey_, "AVCaptureSessionErrorKey"},
        {&AVCaptureSessionPreset320x240_, "AVCaptureSessionPreset320x240"},
        {&AVCaptureSessionPreset640x480_, "AVCaptureSessionPreset640x480"},
        {&AVCaptureSessionPreset1280x720_, "AVCaptureSessionPreset1280x720"},
        {&AVVideoScalingModeKey_, "AVVideoScalingModeKey"},
        {&AVVideoScalingModeResizeAspect_, "AVVideoScalingModeResizeAspect"},
    };
    for (size_t i = 0; i < arraysize(av_strings); ++i) {
      *av_strings[i].loaded_string = *reinterpret_cast<NSString**>(
          dlsym(library_handle_, av_strings[i].symbol));
      DCHECK(*av_strings[i].loaded_string) << dlerror();
    }
  }

  NSBundle* bundle() const { return bundle_; }
  void* library_handle() const { return library_handle_; }

  NSString* AVCaptureDeviceWasConnectedNotification() const {
    return AVCaptureDeviceWasConnectedNotification_;
  }
  NSString* AVCaptureDeviceWasDisconnectedNotification() const {
    return AVCaptureDeviceWasDisconnectedNotification_;
  }
  NSString* AVMediaTypeVideo() const { return AVMediaTypeVideo_; }
  NSString* AVMediaTypeAudio() const { return AVMediaTypeAudio_; }
  NSString* AVMediaTypeMuxed() const { return AVMediaTypeMuxed_; }
  NSString* AVCaptureSessionRuntimeErrorNotification() const {
    return AVCaptureSessionRuntimeErrorNotification_;
  }
  NSString* AVCaptureSessionDidStopRunningNotification() const {
    return AVCaptureSessionDidStopRunningNotification_;
  }
  NSString* AVCaptureSessionErrorKey() const {
    return AVCaptureSessionErrorKey_;
  }
  NSString* AVCaptureSessionPreset320x240() const {
    return AVCaptureSessionPreset320x240_;
  }
  NSString* AVCaptureSessionPreset640x480() const {
    return AVCaptureSessionPreset640x480_;
  }
  NSString* AVCaptureSessionPreset1280x720() const {
    return AVCaptureSessionPreset1280x720_;
  }
  NSString* AVVideoScalingModeKey() const { return AVVideoScalingModeKey_; }
  NSString* AVVideoScalingModeResizeAspect() const {
    return AVVideoScalingModeResizeAspect_;
  }

 private:
  NSBundle* bundle_;
  void* library_handle_;
  // The following members are replicas of the respectives in AVFoundation.
  NSString* AVCaptureDeviceWasConnectedNotification_;
  NSString* AVCaptureDeviceWasDisconnectedNotification_;
  NSString* AVMediaTypeVideo_;
  NSString* AVMediaTypeAudio_;
  NSString* AVMediaTypeMuxed_;
  NSString* AVCaptureSessionRuntimeErrorNotification_;
  NSString* AVCaptureSessionDidStopRunningNotification_;
  NSString* AVCaptureSessionErrorKey_;
  NSString* AVCaptureSessionPreset320x240_;
  NSString* AVCaptureSessionPreset640x480_;
  NSString* AVCaptureSessionPreset1280x720_;
  NSString* AVVideoScalingModeKey_;
  NSString* AVVideoScalingModeResizeAspect_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

}  // namespace

static base::LazyInstance<AVFoundationInternal> g_avfoundation_handle =
    LAZY_INSTANCE_INITIALIZER;

bool AVFoundationGlue::IsAVFoundationSupported() {
  // DeviceMonitorMac will initialize this static bool from the main UI thread
  // once, during Chrome startup so this construction is thread safe.
  static bool is_av_foundation_supported = base::mac::IsOSLionOrLater() &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAVFoundation) && [AVFoundationBundle() load];
  return is_av_foundation_supported;
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  return g_avfoundation_handle.Get().bundle();
}

void* AVFoundationGlue::AVFoundationLibraryHandle() {
  return g_avfoundation_handle.Get().library_handle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return g_avfoundation_handle.Get().AVCaptureDeviceWasConnectedNotification();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureDeviceWasDisconnectedNotification();
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return g_avfoundation_handle.Get().AVMediaTypeVideo();
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return g_avfoundation_handle.Get().AVMediaTypeAudio();
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return g_avfoundation_handle.Get().AVMediaTypeMuxed();
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return g_avfoundation_handle.Get().AVCaptureSessionRuntimeErrorNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureSessionDidStopRunningNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return g_avfoundation_handle.Get().AVCaptureSessionErrorKey();
}

NSString* AVFoundationGlue::AVCaptureSessionPreset320x240() {
  return g_avfoundation_handle.Get().AVCaptureSessionPreset320x240();
}

NSString* AVFoundationGlue::AVCaptureSessionPreset640x480() {
  return g_avfoundation_handle.Get().AVCaptureSessionPreset640x480();
}

NSString* AVFoundationGlue::AVCaptureSessionPreset1280x720() {
  return g_avfoundation_handle.Get().AVCaptureSessionPreset1280x720();
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return g_avfoundation_handle.Get().AVVideoScalingModeKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspect() {
  return g_avfoundation_handle.Get().AVVideoScalingModeResizeAspect();
}

Class AVFoundationGlue::AVCaptureSessionClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureSession"];
}

Class AVFoundationGlue::AVCaptureVideoDataOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureVideoDataOutput"];
}

@implementation AVCaptureDeviceGlue

+ (NSArray*)devices {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  if ([avcClass respondsToSelector:@selector(devices)]) {
    return [avcClass performSelector:@selector(devices)];
  }
  return nil;
}

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  return [avcClass performSelector:@selector(deviceWithUniqueID:)
                        withObject:deviceUniqueID];
}

@end  // @implementation AVCaptureDeviceGlue

@implementation AVCaptureDeviceInputGlue

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError {
  return [[AVFoundationGlue::AVFoundationBundle()
      classNamed:@"AVCaptureDeviceInput"] deviceInputWithDevice:device
                                                          error:outError];
}

@end  // @implementation AVCaptureDeviceInputGlue
