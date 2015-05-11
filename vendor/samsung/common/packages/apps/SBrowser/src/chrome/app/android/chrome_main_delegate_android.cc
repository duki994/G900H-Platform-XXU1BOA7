// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_main_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "chrome/browser/android/chrome_startup_flags.h"
#include "chrome/browser/android/uma_utils.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "content/public/browser/browser_main_runner.h"

#ifdef S_BUMPED_UP_FD_LIMIT

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/main_function_params.h"

// To disable, inside adb shell,
// $ echo "chrome --disable-bumpedup-openfd-limit" > /data/local/tmp/chromium-testshell-command-line
// and restart the browser if already running.
const char kDisableBumpedupOpenFDLimit[] = "disable-bumpedup-openfd-limit";

// Sets the file descriptor soft limit to |new_soft_limit| or the OS hard limit, whichever is lower.
static void SetNewOpenFDLimit(size_t new_soft_limit) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    LOG(INFO) <<"[BROWSER][STARTUP][SetNewOpenFDLimit] soft limit = "
		<< limits.rlim_cur <<", hard limit = " << limits.rlim_max
		<< ", new soft limit = " << new_soft_limit;

    size_t new_limit = new_soft_limit;
    if (limits.rlim_max > 0 && limits.rlim_max < new_soft_limit) {
      new_limit = limits.rlim_max;
    }

    limits.rlim_cur = new_limit;
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      LOG(INFO) << "[BROWSER][STARTUP][SetNewOpenFDLimit] Failed to set fd limit.";
    }
  } else {
    LOG(INFO) << "[BROWSER][STARTUP][SetNewOpenFDLimit] Failed to get fd limit.";
  }
}

// To change open-fd limit via command line, inside adb shell,
// $ echo "chrome --file-descriptor-limit=1024" > /data/local/tmp/chromium-testshell-command-line
// and restart the browser if already running.
static void PreRunInitialization(const CommandLine& command_line) {
  const std::string fd_limit_string =
     command_line.GetSwitchValueASCII(switches::kFileDescriptorLimit);

  int fd_limit = 4*1024;  //  Usually soft-limit is set to 1024(default), bumped up by 4 times.
  if (!fd_limit_string.empty()) {
    base::StringToInt(fd_limit_string, &fd_limit);
  }

  // Set RLIMIT_NOFILE to increase soft limit from 1024(default) to 4096.
  SetNewOpenFDLimit(fd_limit);
}

#endif


// ChromeMainDelegateAndroid is created when the library is loaded. It is always
// done in the process's main Java thread. But for non browser process, e.g.
// renderer process, it is not the native Chrome's main thread.
ChromeMainDelegateAndroid::ChromeMainDelegateAndroid() {
}

ChromeMainDelegateAndroid::~ChromeMainDelegateAndroid() {
}

void ChromeMainDelegateAndroid::SandboxInitialized(
    const std::string& process_type) {
  ChromeMainDelegate::SandboxInitialized(process_type);
}

int ChromeMainDelegateAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  TRACE_EVENT0("startup", "ChromeMainDelegateAndroid::RunProcess")
  if (process_type.empty()) {

#ifdef S_BUMPED_UP_FD_LIMIT
   // Setting new open-fd soft limit for browser process only.
   if (!main_function_params.command_line.HasSwitch(
         kDisableBumpedupOpenFDLimit)) {
       PreRunInitialization(main_function_params.command_line);
   }
#endif

    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterApplicationNativeMethods(env);

    // Because the browser process can be started asynchronously as a series of
    // UI thread tasks a second request to start it can come in while the
    // first request is still being processed. Chrome must keep the same
    // browser runner for the second request.
    // Also only record the start time the first time round, since this is the
    // start time of the application, and will be same for all requests.
    if (!browser_runner_.get()) {
      base::Time startTime = chrome::android::GetMainEntryPointTime();
      startup_metric_utils::RecordSavedMainEntryPointTime(startTime);
      browser_runner_.reset(content::BrowserMainRunner::Create());
    }
    return browser_runner_->Initialize(main_function_params);
  }

  return ChromeMainDelegate::RunProcess(process_type, main_function_params);
}

bool ChromeMainDelegateAndroid::BasicStartupComplete(int* exit_code) {
  SetChromeSpecificCommandLineFlags();
  return ChromeMainDelegate::BasicStartupComplete(exit_code);
}

bool ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(JNIEnv* env) {
  return chrome::android::RegisterJni(env);
}
