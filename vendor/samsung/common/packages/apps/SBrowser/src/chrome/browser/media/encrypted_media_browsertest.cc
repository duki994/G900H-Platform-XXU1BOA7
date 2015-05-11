// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/media/media_browsertest.h"
#include "chrome/browser/media/test_license_server.h"
#include "chrome/browser/media/wv_test_license_server_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test_utils.h"
#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.

#if defined(ENABLE_PEPPER_CDMS)
// Platform-specific filename relative to the chrome executable.
const char kClearKeyCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
    "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
    "libclearkeycdmadapter.so";
#endif

const char kClearKeyCdmPluginMimeType[] = "application/x-ppapi-clearkey-cdm";
#endif  // defined(ENABLE_PEPPER_CDMS)

// Available key systems.
const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";
const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyInitializeFailKeySystem[] =
    "org.chromium.externalclearkey.initializefail";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";

// Supported media types.
const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";
#if defined(USE_PROPRIETARY_CODECS)
const char kMP4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMP4VideoOnly[] = "video/mp4; codecs=\"avc1.4D4041\"";
#endif  // defined(USE_PROPRIETARY_CODECS)

// Sessions to load.
const char kNoSessionToLoad[] = "";
const char kLoadableSession[] = "LoadableSession";
const char kUnknownSession[] = "UnknownSession";

// EME-specific test results and errors.
const char kEmeKeyError[] = "KEYERROR";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char kFileIOTestSuccess[] = "FILEIOTESTSUCCESS";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

// MSE is available on all desktop platforms and on Android 4.1 and later.
static bool IsMSESupported() {
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 16) {
    VLOG(0) << "MSE is only supported in Android 4.1 and later.";
    return false;
  }
#endif  // defined(OS_ANDROID)
  return true;
}

static bool IsParentKeySystemOf(const std::string& parent_key_system,
                                const std::string& key_system) {
  std::string prefix = parent_key_system + '.';
  return key_system.substr(0, prefix.size()) == prefix;
}

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  EncryptedMediaTestBase() : is_pepper_cdm_registered_(false) {}

  bool IsExternalClearKey(const std::string& key_system) {
    return key_system == kExternalClearKeyKeySystem ||
           IsParentKeySystemOf(kExternalClearKeyKeySystem, key_system);
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  bool IsWidevine(const std::string& key_system) {
    return key_system == kWidevineKeySystem;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

  void RunEncryptedMediaTestPage(const std::string& html_page,
                                 const std::string& key_system,
                                 std::vector<StringPair>* query_params,
                                 const std::string& expected_title) {
    StartLicenseServerIfNeeded(key_system, query_params);
    RunMediaTestPage(html_page, query_params, expected_title, true);
  }

  // Tests |html_page| using |media_file| (with |media_type|) and |key_system|.
  // When |session_to_load| is not empty, the test will try to load
  // |session_to_load| with stored keys, instead of creating a new session
  // and trying to update it with licenses.
  // When |force_invalid_response| is true, the test will provide invalid
  // responses, which should trigger errors.
  // TODO(xhwang): Find an easier way to pass multiple configuration test
  // options.
  void RunEncryptedMediaTest(const std::string& html_page,
                             const std::string& media_file,
                             const std::string& media_type,
                             const std::string& key_system,
                             SrcType src_type,
                             const std::string& session_to_load,
                             bool force_invalid_response,
                             const std::string& expected_title) {
    if (src_type == MSE && !IsMSESupported()) {
      VLOG(0) << "Skipping test - MSE not supported.";
      return;
    }
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediaFile", media_file));
    query_params.push_back(std::make_pair("mediaType", media_type));
    query_params.push_back(std::make_pair("keySystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("useMSE", "1"));
    if (force_invalid_response)
      query_params.push_back(std::make_pair("forceInvalidResponse", "1"));
    if (!session_to_load.empty())
      query_params.push_back(std::make_pair("sessionToLoad", session_to_load));
    RunEncryptedMediaTestPage(html_page, key_system, &query_params,
                              expected_title);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& media_type,
                                   const std::string& key_system,
                                   SrcType src_type) {
    std::string expected_title = kEnded;
    if (!IsPlayBackPossible(key_system))
      expected_title = kEmeKeyError;

    RunEncryptedMediaTest("encrypted_media_player.html", media_file, media_type,
                          key_system, src_type, kNoSessionToLoad, false,
                          expected_title);
    // Check KeyMessage received for all key systems.
    bool receivedKeyMessage = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(video.receivedKeyMessage);",
        &receivedKeyMessage));
    EXPECT_TRUE(receivedKeyMessage);
  }


  void StartLicenseServerIfNeeded(const std::string& key_system,
                                  std::vector<StringPair>* query_params) {
    scoped_ptr<TestLicenseServerConfig> config = GetServerConfig(key_system);
    if (!config)
      return;
    license_server_.reset(new TestLicenseServer(config.Pass()));
    EXPECT_TRUE(license_server_->Start());
    query_params->push_back(std::make_pair("licenseServerURL",
        license_server_->GetServerURL()));
  }

  bool IsPlayBackPossible(const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system) && !GetServerConfig(key_system))
      return false;
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return true;
  }

  scoped_ptr<TestLicenseServerConfig> GetServerConfig(
      const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system)) {
      scoped_ptr<TestLicenseServerConfig> config =
         scoped_ptr<TestLicenseServerConfig>(new WVTestLicenseServerConfig());
      if (config->IsPlatformSupported())
        return config.Pass();
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return scoped_ptr<TestLicenseServerConfig>();
  }

 protected:
  scoped_ptr<TestLicenseServer> license_server_;

  // We want to fail quickly when a test fails because an error is encountered.
  virtual void AddWaitForTitles(content::TitleWatcher* title_watcher) OVERRIDE {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeKeyError));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_ANDROID)
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
#endif  // defined(OS_ANDROID)
  }

  void SetUpCommandLineForKeySystem(const std::string& key_system,
                                    CommandLine* command_line) {
    if (GetServerConfig(key_system))
      // Since the web and license servers listen on different ports, we need to
      // disable web-security to send license requests to the license server.
      // TODO(shadi): Add port forwarding to the test web server configuration.
      command_line->AppendSwitch(switches::kDisableWebSecurity);

#if defined(ENABLE_PEPPER_CDMS)
    if (IsExternalClearKey(key_system)) {
      RegisterPepperCdm(command_line, kClearKeyCdmAdapterFileName, key_system);
    }
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
    else if (IsWidevine(key_system)) {
      RegisterPepperCdm(command_line, kWidevineCdmAdapterFileName, key_system);
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
#endif  // defined(ENABLE_PEPPER_CDMS)
  }

 private:
#if defined(ENABLE_PEPPER_CDMS)
  void RegisterPepperCdm(CommandLine* command_line,
                         const std::string& adapter_name,
                         const std::string& key_system) {
    DCHECK(!is_pepper_cdm_registered_)
        << "RegisterPepperCdm() can only be called once.";
    is_pepper_cdm_registered_ = true;

    // Append the switch to register the Clear Key CDM Adapter.
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    base::FilePath plugin_lib = plugin_dir.AppendASCII(adapter_name);
    EXPECT_TRUE(base::PathExists(plugin_lib)) << plugin_lib.value();
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL("#CDM#0.1.0.0;"));
#if defined(OS_WIN)
    pepper_plugin.append(base::ASCIIToWide(GetPepperType(key_system)));
#else
    pepper_plugin.append(GetPepperType(key_system));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }

  // Adapted from key_systems.cc.
  std::string GetPepperType(const std::string& key_system) {
    if (IsExternalClearKey(key_system))
      return kClearKeyCdmPluginMimeType;
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system))
      return kWidevineCdmPluginMimeType;
#endif  // WIDEVINE_CDM_AVAILABLE

    NOTREACHED();
    return "";
  }
#endif  // defined(ENABLE_PEPPER_CDMS)

  bool is_pepper_cdm_registered_;
};

#if defined(ENABLE_PEPPER_CDMS)
// Tests encrypted media playback using ExternalClearKey key system in
// decrypt-and-decode mode.
class ECKEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  // We use special |key_system| names to do non-playback related tests, e.g.
  // kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest("encrypted_media_player.html",
                          "bear-a-enc_a.webm",
                          kWebMAudioOnly,
                          key_system,
                          SRC,
                          kNoSessionToLoad,
                          false,
                          expected_title);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
  }
};

#if defined(WIDEVINE_CDM_AVAILABLE)
// Tests encrypted media playback using Widevine key system.
class WVEncryptedMediaTest : public EncryptedMediaTestBase {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kWidevineKeySystem, command_line);
  }
};

#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // defined(ENABLE_PEPPER_CDMS)

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - bool: True to load media using MSE, otherwise use src.
//
// Note: Only parameterized (*_P) tests can be used. Non-parameterized (*_F)
// tests will crash at GetParam(). To add non-parameterized tests, use
// EncryptedMediaTestBase or one of its subclasses (e.g. WVEncryptedMediaTest).
class EncryptedMediaTest : public EncryptedMediaTestBase,
  public testing::WithParamInterface<std::tr1::tuple<const char*, SrcType> > {
 public:
  std::string CurrentKeySystem() {
    return std::tr1::get<0>(GetParam());
  }

  SrcType CurrentSourceType() {
    return std::tr1::get<1>(GetParam());
  }

  void TestSimplePlayback(const std::string& encrypted_media,
                          const std::string& media_type) {
    RunSimpleEncryptedMediaTest(
        encrypted_media, media_type, CurrentKeySystem(), CurrentSourceType());
  }

  void RunInvalidResponseTest() {
    RunEncryptedMediaTest("encrypted_media_player.html",
                          "bear-320x240-av-enc_av.webm",
                          kWebMAudioVideo,
                          CurrentKeySystem(),
                          CurrentSourceType(),
                          kNoSessionToLoad,
                          true,
                          kEmeKeyError);
  }

  void TestFrameSizeChange() {
    RunEncryptedMediaTest("encrypted_frame_size_change.html",
                          "frame_size_change-av-enc-v.webm",
                          kWebMAudioVideo,
                          CurrentKeySystem(),
                          CurrentSourceType(),
                          kNoSessionToLoad,
                          false,
                          kEnded);
  }

  void TestConfigChange() {
    DCHECK(IsMSESupported());
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    RunEncryptedMediaTestPage("mse_config_change.html",
                              CurrentKeySystem(),
                              &query_params,
                              kEnded);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(CurrentKeySystem(), command_line);
  }
};

using ::testing::Combine;
using ::testing::Values;

#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(SRC_ClearKey, EncryptedMediaTest,
    Combine(Values(kClearKeyKeySystem), Values(SRC)));
#endif  // !defined(OS_ANDROID)

INSTANTIATE_TEST_CASE_P(MSE_ClearKey, EncryptedMediaTest,
    Combine(Values(kClearKeyKeySystem), Values(MSE)));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if defined(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey, EncryptedMediaTest,
    Combine(Values(kExternalClearKeyKeySystem), Values(SRC)));
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey, EncryptedMediaTest,
    Combine(Values(kExternalClearKeyKeySystem), Values(MSE)));
// To reduce test time, only run ExternalClearKeyDecryptOnly with MSE.
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKeyDecryptOnly, EncryptedMediaTest,
    Combine(Values(kExternalClearKeyDecryptOnlyKeySystem), Values(MSE)));
#endif // defined(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// This test doesn't fully test playback with Widevine. So we only run Widevine
// test with MSE (no SRC) to reduce test time. Also, on Android EME only works
// with MSE and we cannot run this test with SRC.
INSTANTIATE_TEST_CASE_P(MSE_Widevine, EncryptedMediaTest,
    Combine(Values(kWidevineKeySystem), Values(MSE)));
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a-enc_a.webm", kWebMAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_a.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_av.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v-enc_v.webm", kWebMVideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av-enc_v.webm", kWebMAudioVideo);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, InvalidResponseKeyError) {
  RunInvalidResponseTest();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  if (CurrentSourceType() != MSE || !IsMSESupported()) {
    VLOG(0) << "Skipping test - ConfigChange test requires MSE.";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    VLOG(0) << "Skipping test - ConfigChange test requires video playback.";
    return;
  }
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  // Times out on Windows XP. http://crbug.com/171937
#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    VLOG(0) << "Skipping test - FrameSizeChange test requires video playback.";
    return;
  }
  TestFrameSizeChange();
}

#if defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    VLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    VLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// The parent key system cannot be used in generateKeyRequest.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, ParentThrowsException) {
  RunEncryptedMediaTest("encrypted_media_player.html",
                        "bear-a-enc_a.webm",
                        kWebMAudioOnly,
                        "com.widevine",
                        MSE,
                        kNoSessionToLoad,
                        false,
                        kEmeNotSupportedError);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if defined(ENABLE_PEPPER_CDMS)
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem, kEmeKeyError);
}

// When CDM crashes, we should still get a decode error.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, CDMCrashDuringDecode) {
  TestNonPlaybackCases(kExternalClearKeyCrashKeySystem, kError);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, FileIOTest) {
  TestNonPlaybackCases(kExternalClearKeyFileIOTestKeySystem,
                       kFileIOTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadLoadableSession) {
  RunEncryptedMediaTest("encrypted_media_player.html",
                        "bear-320x240-v-enc_v.webm",
                        kWebMVideoOnly,
                        kExternalClearKeyKeySystem,
                        SRC,
                        kLoadableSession,
                        false,
                        kEnded);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadUnknownSession) {
  // TODO(xhwang): Add a specific error for this failure, e.g. kSessionNotFound.
  RunEncryptedMediaTest("encrypted_media_player.html",
                        "bear-320x240-v-enc_v.webm",
                        kWebMVideoOnly,
                        kExternalClearKeyKeySystem,
                        SRC,
                        kUnknownSession,
                        false,
                        kEmeKeyError);
}
#endif  // defined(ENABLE_PEPPER_CDMS)
