// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/user_agent/user_agent_util.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include <algorithm>
#include "ui/base/l10n/l10n_util_android.h"
#endif

// Generated
#include "webkit_version.h"  // NOLINT

#if defined(SBROWSER_CSC_FEATURE)
#include "base/android/sbr/sbr_feature.h"
#endif

namespace webkit_glue {

std::string GetWebKitVersion() {
  return base::StringPrintf("%d.%d (%s)",
                            WEBKIT_VERSION_MAJOR,
                            WEBKIT_VERSION_MINOR,
                            WEBKIT_SVN_REVISION);
}

std::string GetWebKitRevision() {
  return WEBKIT_SVN_REVISION;
}

#if defined(OS_ANDROID)
std::string GetAndroidDeviceName() {
  return base::SysInfo::GetDeviceName();
}

std::string GetApplicationVersion(const std::string& os_info) {
  if (strstr(os_info.c_str(), "Linux; Android") != NULL) {
      return "SamsungBrowser/2.1 ";
  }
  return "";
}

std::string GetApplicationLocale() {
  std::string locale = l10n_util::GetDefaultLocale();
  std::transform(locale.begin(), locale.end(), locale.begin(), ::tolower);
  return locale;
}
#endif

std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS) ||\
    defined(OS_ANDROID)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32)) {  // NOLINT
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

#if defined(OS_WIN)
  std::string architecture_token;
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->wow64_status() == base::win::OSInfo::WOW64_ENABLED) {
    architecture_token = "; WOW64";
  } else {
    base::win::OSInfo::WindowsArchitecture windows_architecture =
        os_info->architecture();
    if (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)
      architecture_token = "; Win64; x64";
    else if (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)
      architecture_token = "; Win64; IA64";
  }
#endif

#if defined(OS_ANDROID)
  std::string android_version_str;
  base::StringAppendF(
      &android_version_str, "%d.%d", os_major_version, os_minor_version);
  if (os_bugfix_version != 0)
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);

  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  std::string android_build_codename = base::SysInfo::GetAndroidBuildCodename();
  std::string android_device_name = GetAndroidDeviceName();

#if defined(SBROWSER_CSC_FEATURE)
  if(strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "KTT") == 0) {
#if defined(S_SYSINFO_GETANDROIDBUILDPDA)
     base::StringAppendF(&android_device_name, "/%s", base::SysInfo::GetAndroidBuildPDA().substr(5).c_str());
#endif
  }
  else if((strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "VODA") == 0) ||
          (strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "TMO") == 0))	{
#if defined(S_SYSINFO_GETANDROIDBUILDPDA)
     base::StringAppendF(&android_device_name, "/%s", base::SysInfo::GetAndroidBuildPDA().c_str());
#endif
  }
  else if (strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "ORANGE") == 0) {
     base::StringAppendF(&android_device_name, "-ORANGE");
  }
  else if (strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "VZW") == 0) {
     base::StringAppendF(&android_device_name, " 4G");  
  }
  else if (strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "USCC") == 0) {
     base::StringAppendF(&android_device_name, " USCC");    
  }
  else if (strcmp(base::android::sbr::getString("CscFeature_Web_SetUserAgent").c_str(), "KDO") == 0) {
     base::StringAppendF(&android_device_name, "-parrot");      
  }
#endif

  if ("REL" == android_build_codename && android_device_name.size() > 0) {
#if defined(SBROWSER_CSC_FEATURE)    
    if( !base::android::sbr::getEnableStatus( "CscFeature_Web_Bool_DisableUserAgentVendor" )) {
#endif
        base::StringAppendF(&android_info_str, "; SAMSUNG %s",
                      android_device_name.c_str());
#if defined(SBROWSER_CSC_FEATURE)    
    } else {
        base::StringAppendF(&android_info_str, "; %s",
                      android_device_name.c_str());
    }
#endif
    semicolon_inserted = true;
  }

  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0) {
    if (!semicolon_inserted) {
      android_info_str += ";";
    }
    android_info_str += " Build/" + android_build_id;
  }
#endif

  base::StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d%s",
      os_major_version,
      os_minor_version,
      architecture_token.c_str()
#elif defined(OS_MACOSX)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#elif defined(OS_CHROMEOS)
      "CrOS "
      "%s %d.%d.%d",
      cputype.c_str(),   // e.g. i686
      os_major_version,
      os_minor_version,
      os_bugfix_version
#elif defined(OS_ANDROID)
      "Android %s%s",
      android_version_str.c_str(),
      android_info_str.c_str()
#else
      "%s %s",
      unixinfo.sysname,  // e.g. Linux
      cputype.c_str()    // e.g. i686
#endif
  );  // NOLINT

  return os_cpu;
}

int GetWebKitMajorVersion() {
  return WEBKIT_VERSION_MAJOR;
}

int GetWebKitMinorVersion() {
  return WEBKIT_VERSION_MINOR;
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  const char kUserAgentPlatform[] =
#if defined(OS_WIN)
      "";
#elif defined(OS_MACOSX)
      "Macintosh; ";
#elif defined(USE_X11)
      "X11; ";           // strange, but that's what Firefox uses
#elif defined(OS_ANDROID)
      "Linux; ";
#else
      "Unknown; ";
#endif

  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", kUserAgentPlatform,
                      webkit_glue::BuildOSCpuInfo().c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  // Derived from Safari's UA string.
  // This is done to expose our product name in a manner that is maximally
  // compatible with Safari, we hope!!
  std::string user_agent;
  base::StringAppendF(
      &user_agent,
#if defined(OS_ANDROID)
      "Mozilla/5.0 (%s) AppleWebKit/%d.%d (KHTML, like Gecko) %s%s Safari/%d.%d",
#else
      "Mozilla/5.0 (%s) AppleWebKit/%d.%d (KHTML, like Gecko) %s Safari/%d.%d",
#endif
      os_info.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR,
#if defined(OS_ANDROID)
      webkit_glue::GetApplicationVersion(os_info).c_str(),
#endif
      product.c_str(),
      WEBKIT_VERSION_MAJOR,
      WEBKIT_VERSION_MINOR);
  return user_agent;
}
}  // namespace webkit_glue
