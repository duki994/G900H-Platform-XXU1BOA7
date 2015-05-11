// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

#include "ppapi/native_client/src/trusted/plugin/plugin.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/text_input_controller.h"

#include "ppapi/native_client/src/trusted/plugin/file_utils.h"
#include "ppapi/native_client/src/trusted/plugin/json_manifest.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_subprocess.h"
#include "ppapi/native_client/src/trusted/plugin/nexe_arch.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/service_runtime.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

namespace {

const char* const kTypeAttribute = "type";
// The "src" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expected to contain
// JSON matching ISAs with .nexe URLs).
const char* const kSrcManifestAttribute = "src";
// The "nacl" attribute of the <embed> tag.  We use the value of this attribute
// to find the manifest file when NaCl is registered as a plug-in for another
// MIME type because the "src" attribute is used to supply us with the resource
// of that MIME type that we're supposed to display.
const char* const kNaClManifestAttribute = "nacl";
// The pseudo-ISA used to indicate portable native client.
const char* const kPortableISA = "portable";
// This is a pretty arbitrary limit on the byte size of the NaCl manfest file.
// Note that the resulting string object has to have at least one byte extra
// for the null termination character.
const size_t kNaClManifestMaxFileBytes = 1024 * 1024;

// Define an argument name to enable 'dev' interfaces. To make sure it doesn't
// collide with any user-defined HTML attribute, make the first character '@'.
const char* const kDevAttribute = "@dev";

// URL schemes that we treat in special ways.
const char* const kChromeExtensionUriScheme = "chrome-extension";
const char* const kDataUriScheme = "data";

// Up to 20 seconds
const int64_t kTimeSmallMin = 1;         // in ms
const int64_t kTimeSmallMax = 20000;     // in ms
const uint32_t kTimeSmallBuckets = 100;

// Up to 3 minutes, 20 seconds
const int64_t kTimeMediumMin = 10;         // in ms
const int64_t kTimeMediumMax = 200000;     // in ms
const uint32_t kTimeMediumBuckets = 100;

// Up to 33 minutes.
const int64_t kTimeLargeMin = 100;         // in ms
const int64_t kTimeLargeMax = 2000000;     // in ms
const uint32_t kTimeLargeBuckets = 100;

const int64_t kSizeKBMin = 1;
const int64_t kSizeKBMax = 512*1024;     // very large .nexe
const uint32_t kSizeKBBuckets = 100;

const PPB_NaCl_Private* GetNaClInterface() {
  pp::Module *module = pp::Module::Get();
  CHECK(module);
  return static_cast<const PPB_NaCl_Private*>(
      module->GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));
}

}  // namespace

bool Plugin::EarlyInit(int argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("Plugin::EarlyInit (instance=%p)\n",
                 static_cast<void*>(this)));

#ifdef NACL_OSX
  // TODO(kochi): For crbug.com/102808, this is a stopgap solution for Lion
  // until we expose IME API to .nexe. This disables any IME interference
  // against key inputs, so you cannot use off-the-spot IME input for NaCl apps.
  // This makes discrepancy among platforms and therefore we should remove
  // this hack when IME API is made available.
  // The default for non-Mac platforms is still off-the-spot IME mode.
  pp::TextInputController(this).SetTextInputType(PP_TEXTINPUT_TYPE_NONE);
#endif

  for (int i = 0; i < argc; ++i) {
    std::string name(argn[i]);
    std::string value(argv[i]);
    args_[name] = value;
  }

  // Set up the factory used to produce DescWrappers.
  wrapper_factory_ = new nacl::DescWrapperFactory();
  if (NULL == wrapper_factory_) {
    return false;
  }
  PLUGIN_PRINTF(("Plugin::Init (wrapper_factory=%p)\n",
                 static_cast<void*>(wrapper_factory_)));

  PLUGIN_PRINTF(("Plugin::Init (return 1)\n"));
  // Return success.
  return true;
}

void Plugin::ShutDownSubprocesses() {
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (this=%p)\n",
                 static_cast<void*>(this)));
  PLUGIN_PRINTF(("Plugin::ShutDownSubprocesses (%s)\n",
                 main_subprocess_.detailed_description().c_str()));

  // Shut down service runtime. This must be done before all other calls so
  // they don't block forever when waiting for the upcall thread to exit.
  main_subprocess_.Shutdown();

  PLUGIN_PRINTF(("Plugin::ShutDownSubprocess (this=%p, return)\n",
                 static_cast<void*>(this)));
}

void Plugin::HistogramTimeSmall(const std::string& name,
                                int64_t ms) {
  if (ms < 0) return;
  uma_interface_.HistogramCustomTimes(name,
                                      ms,
                                      kTimeSmallMin, kTimeSmallMax,
                                      kTimeSmallBuckets);
}

void Plugin::HistogramTimeMedium(const std::string& name,
                                 int64_t ms) {
  if (ms < 0) return;
  uma_interface_.HistogramCustomTimes(name,
                                      ms,
                                      kTimeMediumMin, kTimeMediumMax,
                                      kTimeMediumBuckets);
}

void Plugin::HistogramTimeLarge(const std::string& name,
                                int64_t ms) {
  if (ms < 0) return;
  uma_interface_.HistogramCustomTimes(name,
                                      ms,
                                      kTimeLargeMin, kTimeLargeMax,
                                      kTimeLargeBuckets);
}

void Plugin::HistogramSizeKB(const std::string& name,
                             int32_t sample) {
  if (sample < 0) return;
  uma_interface_.HistogramCustomCounts(name,
                                       sample,
                                       kSizeKBMin, kSizeKBMax,
                                       kSizeKBBuckets);
}

void Plugin::HistogramEnumerate(const std::string& name,
                                int sample,
                                int maximum,
                                int out_of_range_replacement) {
  if (sample < 0 || sample >= maximum) {
    if (out_of_range_replacement < 0)
      // No replacement for bad input, abort.
      return;
    else
      // Use a specific value to signal a bad input.
      sample = out_of_range_replacement;
  }
  uma_interface_.HistogramEnumeration(name, sample, maximum);
}

void Plugin::HistogramEnumerateOsArch(const std::string& sandbox_isa) {
  enum NaClOSArch {
    kNaClLinux32 = 0,
    kNaClLinux64,
    kNaClLinuxArm,
    kNaClMac32,
    kNaClMac64,
    kNaClMacArm,
    kNaClWin32,
    kNaClWin64,
    kNaClWinArm,
    kNaClOSArchMax
  };

  NaClOSArch os_arch = kNaClOSArchMax;
#if NACL_LINUX
  os_arch = kNaClLinux32;
#elif NACL_OSX
  os_arch = kNaClMac32;
#elif NACL_WINDOWS
  os_arch = kNaClWin32;
#endif

  if (sandbox_isa == "x86-64")
    os_arch = static_cast<NaClOSArch>(os_arch + 1);
  if (sandbox_isa == "arm")
    os_arch = static_cast<NaClOSArch>(os_arch + 2);

  HistogramEnumerate("NaCl.Client.OSArch", os_arch, kNaClOSArchMax, -1);
}

void Plugin::HistogramEnumerateLoadStatus(PluginErrorCode error_code,
                                          bool is_installed) {
  HistogramEnumerate("NaCl.LoadStatus.Plugin", error_code, ERROR_MAX,
                     ERROR_UNKNOWN);

  // Gather data to see if being installed changes load outcomes.
  const char* name = is_installed ? "NaCl.LoadStatus.Plugin.InstalledApp" :
      "NaCl.LoadStatus.Plugin.NotInstalledApp";
  HistogramEnumerate(name, error_code, ERROR_MAX, ERROR_UNKNOWN);
}

void Plugin::HistogramEnumerateSelLdrLoadStatus(NaClErrorCode error_code,
                                                bool is_installed) {
  HistogramEnumerate("NaCl.LoadStatus.SelLdr", error_code,
                     NACL_ERROR_CODE_MAX, LOAD_STATUS_UNKNOWN);

  // Gather data to see if being installed changes load outcomes.
  const char* name = is_installed ? "NaCl.LoadStatus.SelLdr.InstalledApp" :
      "NaCl.LoadStatus.SelLdr.NotInstalledApp";
  HistogramEnumerate(name, error_code, NACL_ERROR_CODE_MAX,
                     LOAD_STATUS_UNKNOWN);
}

void Plugin::HistogramEnumerateManifestIsDataURI(bool is_data_uri) {
  HistogramEnumerate("NaCl.Manifest.IsDataURI", is_data_uri, 2, -1);
}

void Plugin::HistogramHTTPStatusCode(const std::string& name,
                                     int status) {
  // Log the status codes in rough buckets - 1XX, 2XX, etc.
  int sample = status / 100;
  // HTTP status codes only go up to 5XX, using "6" to indicate an internal
  // error.
  // Note: installed files may have "0" for a status code.
  if (status < 0 || status >= 600)
    sample = 6;
  HistogramEnumerate(name, sample, 7, 6);
}

bool Plugin::LoadNaClModuleFromBackgroundThread(
    nacl::DescWrapper* wrapper,
    NaClSubprocess* subprocess,
    const Manifest* manifest,
    const SelLdrStartParams& params) {
  CHECK(!pp::Module::Get()->core()->IsMainThread());
  ServiceRuntime* service_runtime =
      new ServiceRuntime(this, manifest, false,
                         pp::BlockUntilComplete(), pp::BlockUntilComplete());
  subprocess->set_service_runtime(service_runtime);
  PLUGIN_PRINTF(("Plugin::LoadNaClModuleFromBackgroundThread "
                 "(service_runtime=%p)\n",
                 static_cast<void*>(service_runtime)));

  // Now start the SelLdr instance.  This must be created on the main thread.
  bool service_runtime_started;
  pp::CompletionCallback sel_ldr_callback =
      callback_factory_.NewCallback(&Plugin::SignalStartSelLdrDone,
                                    &service_runtime_started,
                                    service_runtime);
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&Plugin::StartSelLdrOnMainThread,
                                    service_runtime, params,
                                    sel_ldr_callback);
  pp::Module::Get()->core()->CallOnMainThread(0, callback, 0);
  service_runtime->WaitForSelLdrStart();
  PLUGIN_PRINTF(("Plugin::LoadNaClModuleFromBackgroundThread "
                 "(service_runtime_started=%d)\n",
                 service_runtime_started));
  if (!service_runtime_started) {
    return false;
  }

  // Now actually load the nexe, which can happen on a background thread.
  bool nexe_loaded = service_runtime->LoadNexeAndStart(
      wrapper, pp::BlockUntilComplete());
  PLUGIN_PRINTF(("Plugin::LoadNaClModuleFromBackgroundThread "
                 "(nexe_loaded=%d)\n",
                 nexe_loaded));
  return nexe_loaded;
}

void Plugin::StartSelLdrOnMainThread(int32_t pp_error,
                                     ServiceRuntime* service_runtime,
                                     const SelLdrStartParams& params,
                                     pp::CompletionCallback callback) {
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("Plugin::StartSelLdrOnMainThread: non-PP_OK arg "
                   "-- SHOULD NOT HAPPEN\n"));
    pp::Module::Get()->core()->CallOnMainThread(0, callback, pp_error);
    return;
  }
  service_runtime->StartSelLdr(params, callback);
}

void Plugin::SignalStartSelLdrDone(int32_t pp_error,
                                   bool* started,
                                   ServiceRuntime* service_runtime) {
  *started = (pp_error == PP_OK);
  service_runtime->SignalStartSelLdrDone();
}

void Plugin::LoadNaClModule(nacl::DescWrapper* wrapper,
                            bool enable_dyncode_syscalls,
                            bool enable_exception_handling,
                            bool enable_crash_throttling,
                            const pp::CompletionCallback& init_done_cb,
                            const pp::CompletionCallback& crash_cb) {
  nacl::scoped_ptr<nacl::DescWrapper> scoped_wrapper(wrapper);
  CHECK(pp::Module::Get()->core()->IsMainThread());
  // Before forking a new sel_ldr process, ensure that we do not leak
  // the ServiceRuntime object for an existing subprocess, and that any
  // associated listener threads do not go unjoined because if they
  // outlive the Plugin object, they will not be memory safe.
  ShutDownSubprocesses();
  SelLdrStartParams params(manifest_base_url(),
                           true /* uses_irt */,
                           true /* uses_ppapi */,
                           enable_dev_interfaces_,
                           enable_dyncode_syscalls,
                           enable_exception_handling,
                           enable_crash_throttling);
  ErrorInfo error_info;
  ServiceRuntime* service_runtime =
      new ServiceRuntime(this, manifest_.get(), true, init_done_cb, crash_cb);
  main_subprocess_.set_service_runtime(service_runtime);
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (service_runtime=%p)\n",
                 static_cast<void*>(service_runtime)));
  if (NULL == service_runtime) {
    error_info.SetReport(
        ERROR_SEL_LDR_INIT,
        "sel_ldr init failure " + main_subprocess_.description());
    ReportLoadError(error_info);
    return;
  }

  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &Plugin::LoadNexeAndStart, scoped_wrapper.release(), service_runtime,
      crash_cb);
  StartSelLdrOnMainThread(
      static_cast<int32_t>(PP_OK), service_runtime, params, callback);
}

void Plugin::LoadNexeAndStart(int32_t pp_error,
                              nacl::DescWrapper* wrapper,
                              ServiceRuntime* service_runtime,
                              const pp::CompletionCallback& crash_cb) {
  nacl::scoped_ptr<nacl::DescWrapper> scoped_wrapper(wrapper);
  if (pp_error != PP_OK)
    return;

  // Now actually load the nexe, which can happen on a background thread.
  bool nexe_loaded = service_runtime->LoadNexeAndStart(wrapper, crash_cb);
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (nexe_loaded=%d)\n",
                 nexe_loaded));
  if (nexe_loaded) {
    PLUGIN_PRINTF(("Plugin::LoadNaClModule (%s)\n",
                   main_subprocess_.detailed_description().c_str()));
  }
}

bool Plugin::LoadNaClModuleContinuationIntern(ErrorInfo* error_info) {
  if (!main_subprocess_.StartSrpcServices()) {
    // The NaCl process probably crashed. On Linux, a crash causes this error,
    // while on other platforms, the error is detected below, when we attempt to
    // start the proxy. Report a module initialization error here, to make it
    // less confusing for developers.
    NaClLog(LOG_ERROR, "LoadNaClModuleContinuationIntern: "
            "StartSrpcServices failed\n");
    error_info->SetReport(ERROR_START_PROXY_MODULE,
                          "could not initialize module.");
    return false;
  }
  PP_ExternalPluginResult ipc_result =
      nacl_interface_->StartPpapiProxy(pp_instance());
  if (ipc_result == PP_EXTERNAL_PLUGIN_OK) {
    // Log the amound of time that has passed between the trusted plugin being
    // initialized and the untrusted plugin being initialized.  This is
    // (roughly) the cost of using NaCl, in terms of startup time.
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.NaClOverhead",
        static_cast<float>(NaClGetTimeOfDayMicroseconds() - init_time_)
            / NACL_MICROS_PER_MILLI);
  } else if (ipc_result == PP_EXTERNAL_PLUGIN_ERROR_MODULE) {
    NaClLog(LOG_ERROR, "LoadNaClModuleContinuationIntern: "
            "Got PP_EXTERNAL_PLUGIN_ERROR_MODULE\n");
    error_info->SetReport(ERROR_START_PROXY_MODULE,
                          "could not initialize module.");
    return false;
  } else if (ipc_result == PP_EXTERNAL_PLUGIN_ERROR_INSTANCE) {
    error_info->SetReport(ERROR_START_PROXY_INSTANCE,
                          "could not create instance.");
    return false;
  }
  PLUGIN_PRINTF(("Plugin::LoadNaClModule (%s)\n",
                 main_subprocess_.detailed_description().c_str()));
  return true;
}

NaClSubprocess* Plugin::LoadHelperNaClModule(nacl::DescWrapper* wrapper,
                                             const Manifest* manifest,
                                             ErrorInfo* error_info) {
  nacl::scoped_ptr<NaClSubprocess> nacl_subprocess(
      new NaClSubprocess("helper module", NULL, NULL));
  if (NULL == nacl_subprocess.get()) {
    error_info->SetReport(ERROR_SEL_LDR_INIT,
                          "unable to allocate helper subprocess.");
    return NULL;
  }

  // Do not report UMA stats for translator-related nexes.
  // TODO(sehr): define new UMA stats for translator related nexe events.
  // NOTE: The PNaCl translator nexes are not built to use the IRT.  This is
  // done to save on address space and swap space.
  // TODO(jvoung): See if we still need the uses_ppapi variable, now that
  // LaunchSelLdr always happens on the main thread.
  SelLdrStartParams params(manifest_base_url(),
                           false /* uses_irt */,
                           false /* uses_ppapi */,
                           enable_dev_interfaces_,
                           false /* enable_dyncode_syscalls */,
                           false /* enable_exception_handling */,
                           true /* enable_crash_throttling */);
  if (!LoadNaClModuleFromBackgroundThread(wrapper, nacl_subprocess.get(),
                                          manifest, params)) {
    return NULL;
  }
  // We need not wait for the init_done callback.  We can block
  // here in StartSrpcServices, since helper NaCl modules
  // are spawned from a private thread.
  //
  // TODO(bsy): if helper module crashes, we should abort.
  // crash_cb is not used here, so we are relying on crashes
  // being detected in StartSrpcServices or later.
  //
  // NB: More refactoring might be needed, however, if helper
  // NaCl modules have their own manifest.  Currently the
  // manifest is a per-plugin-instance object, not a per
  // NaClSubprocess object.
  if (!nacl_subprocess->StartSrpcServices()) {
    error_info->SetReport(ERROR_SRPC_CONNECTION_FAIL,
                          "SRPC connection failure for " +
                          nacl_subprocess->description());
    return NULL;
  }

  PLUGIN_PRINTF(("Plugin::LoadHelperNaClModule (%s)\n",
                 nacl_subprocess.get()->detailed_description().c_str()));

  return nacl_subprocess.release();
}

std::string Plugin::LookupArgument(const std::string& key) const {
  std::map<std::string, std::string>::const_iterator it = args_.find(key);
  if (it != args_.end())
    return it->second;
  return std::string();
}

const char* const Plugin::kNaClMIMEType = "application/x-nacl";
const char* const Plugin::kPnaclMIMEType = "application/x-pnacl";

bool Plugin::NexeIsContentHandler() const {
  // Tests if the MIME type is not a NaCl MIME type.
  // If the MIME type is foreign, then this NEXE is being used as a content
  // type handler rather than directly by an HTML document.
  return
      !mime_type().empty() &&
      mime_type() != kNaClMIMEType &&
      mime_type() != kPnaclMIMEType;
}


Plugin* Plugin::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("Plugin::New (pp_instance=%" NACL_PRId32 ")\n", pp_instance));
  Plugin* plugin = new Plugin(pp_instance);
  PLUGIN_PRINTF(("Plugin::New (plugin=%p)\n", static_cast<void*>(plugin)));
  return plugin;
}


// All failures of this function will show up as "Missing Plugin-in", so
// there is no need to log to JS console that there was an initialization
// failure. Note that module loading functions will log their own errors.
bool Plugin::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("Plugin::Init (argc=%" NACL_PRIu32 ")\n", argc));
  HistogramEnumerateOsArch(GetSandboxISA());
  init_time_ = NaClGetTimeOfDayMicroseconds();
  url_util_ = pp::URLUtil_Dev::Get();
  if (url_util_ == NULL)
    return false;

  PLUGIN_PRINTF(("Plugin::Init (url_util_=%p)\n",
                 static_cast<const void*>(url_util_)));

  bool status = EarlyInit(static_cast<int>(argc), argn, argv);
  if (status) {
    // Look for the developer attribute; if it's present, enable 'dev'
    // interfaces.
    enable_dev_interfaces_ = args_.find(kDevAttribute) != args_.end();

    mime_type_ = LookupArgument(kTypeAttribute);
    std::transform(mime_type_.begin(), mime_type_.end(), mime_type_.begin(),
                   tolower);

    std::string manifest_url;
    if (NexeIsContentHandler()) {
      // For content handlers 'src' will be the URL for the content
      // and 'nacl' will be the URL for the manifest.
      manifest_url = LookupArgument(kNaClManifestAttribute);
      // For content handlers the NEXE runs in the security context of the
      // content it is rendering and the NEXE itself appears to be a
      // cross-origin resource stored in a Chrome extension.
    } else {
      manifest_url = LookupArgument(kSrcManifestAttribute);
    }
    // Use the document URL as the base for resolving relative URLs to find the
    // manifest.  This takes into account the setting of <base> tags that
    // precede the embed/object.
    CHECK(url_util_ != NULL);
    pp::Var base_var = url_util_->GetDocumentURL(this);
    if (!base_var.is_string()) {
      PLUGIN_PRINTF(("Plugin::Init (unable to find document url)\n"));
      return false;
    }
    set_plugin_base_url(base_var.AsString());
    if (manifest_url.empty()) {
      // TODO(sehr,polina): this should be a hard error when scripting
      // the src property is no longer allowed.
      PLUGIN_PRINTF(("Plugin::Init:"
                     " WARNING: no 'src' property, so no manifest loaded.\n"));
      if (args_.find(kNaClManifestAttribute) != args_.end()) {
        PLUGIN_PRINTF(("Plugin::Init:"
                       " WARNING: 'nacl' property is incorrect. Use 'src'.\n"));
      }
    } else {
      // Issue a GET for the manifest_url.  The manifest file will be parsed to
      // determine the nexe URL.
      // Sets src property to full manifest URL.
      RequestNaClManifest(manifest_url.c_str());
    }
  }

  PLUGIN_PRINTF(("Plugin::Init (status=%d)\n", status));
  return status;
}

Plugin::Plugin(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      main_subprocess_("main subprocess", NULL, NULL),
      nexe_error_reported_(false),
      wrapper_factory_(NULL),
      enable_dev_interfaces_(false),
      is_installed_(false),
      init_time_(0),
      ready_time_(0),
      nexe_size_(0),
      time_of_last_progress_event_(0),
      exit_status_(-1),
      nacl_interface_(NULL),
      uma_interface_(this) {
  PLUGIN_PRINTF(("Plugin::Plugin (this=%p, pp_instance=%"
                 NACL_PRId32 ")\n", static_cast<void*>(this), pp_instance));
  callback_factory_.Initialize(this);
  nexe_downloader_.Initialize(this);
  nacl_interface_ = GetNaClInterface();
  CHECK(nacl_interface_ != NULL);
  set_nacl_ready_state(UNSENT);
  set_last_error_string("");
  // We call set_exit_status() here to ensure that the 'exitStatus' property is
  // set. This can only be called when nacl_interface_ is not NULL.
  set_exit_status(-1);
}


Plugin::~Plugin() {
  int64_t shutdown_start = NaClGetTimeOfDayMicroseconds();

  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p)\n",
                 static_cast<void*>(this)));
  // Destroy the coordinator while the rest of the data is still there
  pnacl_coordinator_.reset(NULL);

  if (!nexe_error_reported()) {
    HistogramTimeLarge(
        "NaCl.ModuleUptime.Normal",
        (shutdown_start - ready_time_) / NACL_MICROS_PER_MILLI);
  }

  for (std::map<nacl::string, NaClFileInfoAutoCloser*>::iterator it =
           url_file_info_map_.begin();
       it != url_file_info_map_.end();
       ++it) {
    delete it->second;
  }
  url_downloaders_.erase(url_downloaders_.begin(), url_downloaders_.end());

  // ShutDownSubprocesses shuts down the main subprocess, which shuts
  // down the main ServiceRuntime object, which kills the subprocess.
  // As a side effect of the subprocess being killed, the reverse
  // services thread(s) will get EOF on the reverse channel(s), and
  // the thread(s) will exit.  In ServiceRuntime::Shutdown, we invoke
  // ReverseService::WaitForServiceThreadsToExit(), so that there will
  // not be an extent thread(s) hanging around.  This means that the
  // ~Plugin will block until this happens.  This is a requirement,
  // since the renderer should be free to unload the plugin code, and
  // we cannot have threads running code that gets unloaded before
  // they exit.
  //
  // By waiting for the threads here, we also ensure that the Plugin
  // object and the subprocess and ServiceRuntime objects is not
  // (fully) destroyed while the threads are running, so resources
  // that are destroyed after ShutDownSubprocesses (below) are
  // guaranteed to be live and valid for access from the service
  // threads.
  //
  // The main_subprocess object, which wraps the main service_runtime
  // object, is dtor'd implicitly after the explicit code below runs,
  // so the main service runtime object will not have been dtor'd,
  // though the Shutdown method may have been called, during the
  // lifetime of the service threads.
  ShutDownSubprocesses();

  delete wrapper_factory_;

  HistogramTimeSmall(
      "NaCl.Perf.ShutdownTime.Total",
      (NaClGetTimeOfDayMicroseconds() - shutdown_start)
          / NACL_MICROS_PER_MILLI);

  PLUGIN_PRINTF(("Plugin::~Plugin (this=%p, return)\n",
                 static_cast<void*>(this)));
}

bool Plugin::HandleDocumentLoad(const pp::URLLoader& url_loader) {
  PLUGIN_PRINTF(("Plugin::HandleDocumentLoad (this=%p)\n",
                 static_cast<void*>(this)));
  // We don't know if the plugin will handle the document load, but return
  // true in order to give it a chance to respond once the proxy is started.
  return true;
}

void Plugin::HistogramStartupTimeSmall(const std::string& name, float dt) {
  if (nexe_size_ > 0) {
    float size_in_MB = static_cast<float>(nexe_size_) / (1024.f * 1024.f);
    HistogramTimeSmall(name, static_cast<int64_t>(dt));
    HistogramTimeSmall(name + "PerMB", static_cast<int64_t>(dt / size_in_MB));
  }
}

void Plugin::HistogramStartupTimeMedium(const std::string& name, float dt) {
  if (nexe_size_ > 0) {
    float size_in_MB = static_cast<float>(nexe_size_) / (1024.f * 1024.f);
    HistogramTimeMedium(name, static_cast<int64_t>(dt));
    HistogramTimeMedium(name + "PerMB", static_cast<int64_t>(dt / size_in_MB));
  }
}

void Plugin::NexeFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NexeFileDidOpen (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  NaClFileInfo tmp_info(nexe_downloader_.GetFileInfo());
  NaClFileInfoAutoCloser info(&tmp_info);
  PLUGIN_PRINTF(("Plugin::NexeFileDidOpen (file_desc=%" NACL_PRId32 ")\n",
                 info.get_desc()));
  HistogramHTTPStatusCode(
      is_installed_ ?
          "NaCl.HttpStatusCodeClass.Nexe.InstalledApp" :
          "NaCl.HttpStatusCodeClass.Nexe.NotInstalledApp",
      nexe_downloader_.status_code());
  ErrorInfo error_info;
  if (pp_error != PP_OK || info.get_desc() == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else if (pp_error == PP_ERROR_NOACCESS) {
      error_info.SetReport(ERROR_NEXE_NOACCESS_URL,
                           "access to nexe url was denied.");
      ReportLoadError(error_info);
    } else {
      error_info.SetReport(ERROR_NEXE_LOAD_URL, "could not load nexe url.");
      ReportLoadError(error_info);
    }
    return;
  }
  int32_t file_desc_ok_to_close = DUP(info.get_desc());
  if (file_desc_ok_to_close == NACL_NO_FILE_DESC) {
    error_info.SetReport(ERROR_NEXE_FH_DUP,
                         "could not duplicate loaded file handle.");
    ReportLoadError(error_info);
    return;
  }
  struct stat stat_buf;
  if (0 != fstat(file_desc_ok_to_close, &stat_buf)) {
    CLOSE(file_desc_ok_to_close);
    error_info.SetReport(ERROR_NEXE_STAT, "could not stat nexe file.");
    ReportLoadError(error_info);
    return;
  }
  size_t nexe_bytes_read = static_cast<size_t>(stat_buf.st_size);

  nexe_size_ = nexe_bytes_read;
  HistogramSizeKB("NaCl.Perf.Size.Nexe",
                  static_cast<int32_t>(nexe_size_ / 1024));
  HistogramStartupTimeMedium(
      "NaCl.Perf.StartupTime.NexeDownload",
      static_cast<float>(nexe_downloader_.TimeSinceOpenMilliseconds()));

  // Inform JavaScript that we successfully downloaded the nacl module.
  EnqueueProgressEvent(PP_NACL_EVENT_PROGRESS,
                       nexe_downloader_.url_to_open(),
                       LENGTH_IS_COMPUTABLE,
                       nexe_bytes_read,
                       nexe_bytes_read);

  load_start_ = NaClGetTimeOfDayMicroseconds();
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(wrapper_factory()->MakeFileDesc(file_desc_ok_to_close, O_RDONLY));
  NaClLog(4, "NexeFileDidOpen: invoking LoadNaClModule\n");
  LoadNaClModule(
      wrapper.release(),
      true, /* enable_dyncode_syscalls */
      true, /* enable_exception_handling */
      false, /* enable_crash_throttling */
      callback_factory_.NewCallback(&Plugin::NexeFileDidOpenContinuation),
      callback_factory_.NewCallback(&Plugin::NexeDidCrash));
}

void Plugin::NexeFileDidOpenContinuation(int32_t pp_error) {
  ErrorInfo error_info;
  bool was_successful;

  UNREFERENCED_PARAMETER(pp_error);
  NaClLog(4, "Entered NexeFileDidOpenContinuation\n");
  NaClLog(4, "NexeFileDidOpenContinuation: invoking"
          " LoadNaClModuleContinuationIntern\n");
  was_successful = LoadNaClModuleContinuationIntern(&error_info);
  if (was_successful) {
    NaClLog(4, "NexeFileDidOpenContinuation: success;"
            " setting histograms\n");
    ready_time_ = NaClGetTimeOfDayMicroseconds();
    HistogramStartupTimeSmall(
        "NaCl.Perf.StartupTime.LoadModule",
        static_cast<float>(ready_time_ - load_start_) / NACL_MICROS_PER_MILLI);
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.Total",
        static_cast<float>(ready_time_ - init_time_) / NACL_MICROS_PER_MILLI);

    ReportLoadSuccess(LENGTH_IS_COMPUTABLE, nexe_size_, nexe_size_);
  } else {
    NaClLog(4, "NexeFileDidOpenContinuation: failed.");
    ReportLoadError(error_info);
  }
  NaClLog(4, "Leaving NexeFileDidOpenContinuation\n");
}

static void LogLineToConsole(Plugin* plugin, const nacl::string& one_line) {
  PLUGIN_PRINTF(("LogLineToConsole: %s\n",
                 one_line.c_str()));
  plugin->AddToConsole(one_line);
}

void Plugin::CopyCrashLogToJsConsole() {
  nacl::string fatal_msg(main_service_runtime()->GetCrashLogOutput());
  size_t ix_start = 0;
  size_t ix_end;

  PLUGIN_PRINTF(("Plugin::CopyCrashLogToJsConsole: got %" NACL_PRIuS " bytes\n",
                 fatal_msg.size()));
  while (nacl::string::npos != (ix_end = fatal_msg.find('\n', ix_start))) {
    LogLineToConsole(this, fatal_msg.substr(ix_start, ix_end - ix_start));
    ix_start = ix_end + 1;
  }
  if (ix_start != fatal_msg.size()) {
    LogLineToConsole(this, fatal_msg.substr(ix_start));
  }
}

void Plugin::NexeDidCrash(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NexeDidCrash (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    PLUGIN_PRINTF(("Plugin::NexeDidCrash: CallOnMainThread callback with"
                   " non-PP_OK arg -- SHOULD NOT HAPPEN\n"));
  }
  PLUGIN_PRINTF(("Plugin::NexeDidCrash: crash event!\n"));
  if (-1 != exit_status()) {
    // The NaCl module voluntarily exited.  However, this is still a
    // crash from the point of view of Pepper, since PPAPI plugins are
    // event handlers and should never exit.
    PLUGIN_PRINTF((("Plugin::NexeDidCrash: nexe exited with status %d"
                    " so this is a \"controlled crash\".\n"),
                   exit_status()));
  }
  // If the crash occurs during load, we just want to report an error
  // that fits into our load progress event grammar.  If the crash
  // occurs after loaded/loadend, then we use ReportDeadNexe to send a
  // "crash" event.
  if (nexe_error_reported()) {
    PLUGIN_PRINTF(("Plugin::NexeDidCrash: error already reported;"
                   " suppressing\n"));
  } else {
    if (nacl_ready_state_ == DONE) {
      ReportDeadNexe();
    } else {
      ErrorInfo error_info;
      // The error is not quite right.  In particular, the crash
      // reported by this path could be due to NaCl application
      // crashes that occur after the PPAPI proxy has started.
      error_info.SetReport(ERROR_START_PROXY_CRASH,
                           "Nexe crashed during startup");
      ReportLoadError(error_info);
    }
  }

  // In all cases, try to grab the crash log.  The first error
  // reported may have come from the start_module RPC reply indicating
  // a validation error or something similar, which wouldn't grab the
  // crash log.  In the event that this is called twice, the second
  // invocation will just be a no-op, since all the crash log will
  // have been received and we'll just get an EOF indication.
  CopyCrashLogToJsConsole();
}

void Plugin::BitcodeDidTranslate(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::BitcodeDidTranslate (pp_error=%" NACL_PRId32 ")\n",
                 pp_error));
  if (pp_error != PP_OK) {
    // Error should have been reported by pnacl. Just return.
    PLUGIN_PRINTF(("Plugin::BitcodeDidTranslate error in Pnacl\n"));
    return;
  }

  // Inform JavaScript that we successfully translated the bitcode to a nexe.
  nacl::scoped_ptr<nacl::DescWrapper>
      wrapper(pnacl_coordinator_.get()->ReleaseTranslatedFD());
  LoadNaClModule(
      wrapper.release(),
      false, /* enable_dyncode_syscalls */
      false, /* enable_exception_handling */
      true, /* enable_crash_throttling */
      callback_factory_.NewCallback(&Plugin::BitcodeDidTranslateContinuation),
      callback_factory_.NewCallback(&Plugin::NexeDidCrash));
}

void Plugin::BitcodeDidTranslateContinuation(int32_t pp_error) {
  ErrorInfo error_info;
  bool was_successful = LoadNaClModuleContinuationIntern(&error_info);

  NaClLog(4, "Entered BitcodeDidTranslateContinuation\n");
  UNREFERENCED_PARAMETER(pp_error);
  if (was_successful) {
    int64_t loaded;
    int64_t total;
    pnacl_coordinator_->GetCurrentProgress(&loaded, &total);
    ReportLoadSuccess(LENGTH_IS_COMPUTABLE, loaded, total);
  } else {
    ReportLoadError(error_info);
  }
}

void Plugin::ReportDeadNexe() {
  PLUGIN_PRINTF(("Plugin::ReportDeadNexe\n"));

  if (nacl_ready_state_ == DONE && !nexe_error_reported()) {  // After loadEnd.
    int64_t crash_time = NaClGetTimeOfDayMicroseconds();
    // Crashes will be more likely near startup, so use a medium histogram
    // instead of a large one.
    HistogramTimeMedium(
        "NaCl.ModuleUptime.Crash",
        (crash_time - ready_time_) / NACL_MICROS_PER_MILLI);

    nacl::string message = nacl::string("NaCl module crashed");
    set_last_error_string(message);
    AddToConsole(message);

    EnqueueProgressEvent(PP_NACL_EVENT_CRASH);
    set_nexe_error_reported(true);
  }
  // else ReportLoadError() and ReportAbortError() will be used by loading code
  // to provide error handling.
  //
  // NOTE: not all crashes during load will make it here.
  // Those in BrowserPpp::InitializeModule and creation of PPP interfaces
  // will just get reported back as PP_ERROR_FAILED.
}

void Plugin::NaClManifestBufferReady(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NaClManifestBufferReady (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  ErrorInfo error_info;
  if (pp_error != PP_OK) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else {
      error_info.SetReport(ERROR_MANIFEST_LOAD_URL,
                           "could not load manifest url.");
      ReportLoadError(error_info);
    }
    return;
  }

  const std::deque<char>& buffer = nexe_downloader_.buffer();
  size_t buffer_size = buffer.size();
  if (buffer_size > kNaClManifestMaxFileBytes) {
    error_info.SetReport(ERROR_MANIFEST_TOO_LARGE,
                         "manifest file too large.");
    ReportLoadError(error_info);
    return;
  }
  nacl::scoped_array<char> json_buffer(new char[buffer_size + 1]);
  if (json_buffer == NULL) {
    error_info.SetReport(ERROR_MANIFEST_MEMORY_ALLOC,
                         "could not allocate manifest memory.");
    ReportLoadError(error_info);
    return;
  }
  std::copy(buffer.begin(), buffer.begin() + buffer_size, &json_buffer[0]);
  json_buffer[buffer_size] = '\0';

  ProcessNaClManifest(json_buffer.get());
}

void Plugin::NaClManifestFileDidOpen(int32_t pp_error) {
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen (pp_error=%"
                 NACL_PRId32 ")\n", pp_error));
  HistogramTimeSmall("NaCl.Perf.StartupTime.ManifestDownload",
                     nexe_downloader_.TimeSinceOpenMilliseconds());
  HistogramHTTPStatusCode(
      is_installed_ ?
          "NaCl.HttpStatusCodeClass.Manifest.InstalledApp" :
          "NaCl.HttpStatusCodeClass.Manifest.NotInstalledApp",
      nexe_downloader_.status_code());
  ErrorInfo error_info;
  NaClFileInfo tmp_info(nexe_downloader_.GetFileInfo());
  NaClFileInfoAutoCloser info(&tmp_info);
  PLUGIN_PRINTF(("Plugin::NaClManifestFileDidOpen (file_desc=%"
                 NACL_PRId32 ")\n", info.get_desc()));
  if (pp_error != PP_OK || info.get_desc() == NACL_NO_FILE_DESC) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else if (pp_error == PP_ERROR_NOACCESS) {
      error_info.SetReport(ERROR_MANIFEST_NOACCESS_URL,
                           "access to manifest url was denied.");
      ReportLoadError(error_info);
    } else {
      error_info.SetReport(ERROR_MANIFEST_LOAD_URL,
                           "could not load manifest url.");
      ReportLoadError(error_info);
    }
    return;
  }
  // SlurpFile closes the file descriptor after reading (or on error).
  // Duplicate our file descriptor since it will be handled by the browser.
  int dup_file_desc = DUP(info.get_desc());
  nacl::string json_buffer;
  file_utils::StatusCode status = file_utils::SlurpFile(
      dup_file_desc, json_buffer, kNaClManifestMaxFileBytes);

  if (status != file_utils::PLUGIN_FILE_SUCCESS) {
    switch (status) {
      case file_utils::PLUGIN_FILE_SUCCESS:
        CHECK(0);
        break;
      case file_utils::PLUGIN_FILE_ERROR_MEM_ALLOC:
        error_info.SetReport(ERROR_MANIFEST_MEMORY_ALLOC,
                             "could not allocate manifest memory.");
        break;
      case file_utils::PLUGIN_FILE_ERROR_OPEN:
        error_info.SetReport(ERROR_MANIFEST_OPEN,
                             "could not open manifest file.");
        break;
      case file_utils::PLUGIN_FILE_ERROR_FILE_TOO_LARGE:
        error_info.SetReport(ERROR_MANIFEST_TOO_LARGE,
                             "manifest file too large.");
        break;
      case file_utils::PLUGIN_FILE_ERROR_STAT:
        error_info.SetReport(ERROR_MANIFEST_STAT,
                             "could not stat manifest file.");
        break;
      case file_utils::PLUGIN_FILE_ERROR_READ:
        error_info.SetReport(ERROR_MANIFEST_READ,
                             "could not read manifest file.");
        break;
    }
    ReportLoadError(error_info);
    return;
  }

  ProcessNaClManifest(json_buffer);
}

void Plugin::ProcessNaClManifest(const nacl::string& manifest_json) {
  HistogramSizeKB("NaCl.Perf.Size.Manifest",
                  static_cast<int32_t>(manifest_json.length() / 1024));
  nacl::string program_url;
  PnaclOptions pnacl_options;
  ErrorInfo error_info;
  if (!SetManifestObject(manifest_json, &error_info)) {
    ReportLoadError(error_info);
    return;
  }

  if (manifest_->GetProgramURL(&program_url, &pnacl_options, &error_info)) {
    is_installed_ = GetUrlScheme(program_url) == SCHEME_CHROME_EXTENSION;
    set_nacl_ready_state(LOADING);
    // Inform JavaScript that we found a nexe URL to load.
    EnqueueProgressEvent(PP_NACL_EVENT_PROGRESS);
    if (pnacl_options.translate()) {
      pp::CompletionCallback translate_callback =
          callback_factory_.NewCallback(&Plugin::BitcodeDidTranslate);
      // Will always call the callback on success or failure.
      pnacl_coordinator_.reset(
          PnaclCoordinator::BitcodeToNative(this,
                                            program_url,
                                            pnacl_options,
                                            translate_callback));
      return;
    } else {
      // Try the fast path first. This will only block if the file is installed.
      if (OpenURLFast(program_url, &nexe_downloader_)) {
        NexeFileDidOpen(PP_OK);
      } else {
        pp::CompletionCallback open_callback =
            callback_factory_.NewCallback(&Plugin::NexeFileDidOpen);
        // Will always call the callback on success or failure.
        CHECK(
            nexe_downloader_.Open(program_url,
                                  DOWNLOAD_TO_FILE,
                                  open_callback,
                                  true,
                                  &UpdateDownloadProgress));
      }
      return;
    }
  }
  // Failed to select the program and/or the translator.
  ReportLoadError(error_info);
}

void Plugin::RequestNaClManifest(const nacl::string& url) {
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (url='%s')\n", url.c_str()));
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (plugin base url='%s')\n",
                 plugin_base_url().c_str()));
  // The full URL of the manifest file is relative to the base url.
  CHECK(url_util_ != NULL);
  pp::Var nmf_resolved_url =
      url_util_->ResolveRelativeToURL(plugin_base_url(), pp::Var(url));
  if (!nmf_resolved_url.is_string()) {
    ErrorInfo error_info;
    error_info.SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        nacl::string("could not resolve URL \"") + url.c_str() +
        "\" relative to \"" + plugin_base_url().c_str() + "\".");
    ReportLoadError(error_info);
    return;
  }
  PLUGIN_PRINTF(("Plugin::RequestNaClManifest (resolved url='%s')\n",
                 nmf_resolved_url.AsString().c_str()));
  is_installed_ = GetUrlScheme(nmf_resolved_url.AsString()) ==
      SCHEME_CHROME_EXTENSION;
  set_manifest_base_url(nmf_resolved_url.AsString());
  // Inform JavaScript that a load is starting.
  set_nacl_ready_state(OPENED);
  EnqueueProgressEvent(PP_NACL_EVENT_LOADSTART);
  bool is_data_uri = GetUrlScheme(nmf_resolved_url.AsString()) == SCHEME_DATA;
  HistogramEnumerateManifestIsDataURI(static_cast<int>(is_data_uri));
  if (is_data_uri) {
    pp::CompletionCallback open_callback =
        callback_factory_.NewCallback(&Plugin::NaClManifestBufferReady);
    // Will always call the callback on success or failure.
    CHECK(nexe_downloader_.Open(nmf_resolved_url.AsString(),
                                DOWNLOAD_TO_BUFFER,
                                open_callback,
                                false,
                                NULL));
  } else {
    pp::CompletionCallback open_callback =
        callback_factory_.NewCallback(&Plugin::NaClManifestFileDidOpen);
    // Will always call the callback on success or failure.
    CHECK(nexe_downloader_.Open(nmf_resolved_url.AsString(),
                                DOWNLOAD_TO_FILE,
                                open_callback,
                                false,
                                NULL));
  }
}


bool Plugin::SetManifestObject(const nacl::string& manifest_json,
                               ErrorInfo* error_info) {
  PLUGIN_PRINTF(("Plugin::SetManifestObject(): manifest_json='%s'.\n",
       manifest_json.c_str()));
  if (error_info == NULL)
    return false;
  // Determine whether lookups should use portable (i.e., pnacl versions)
  // rather than platform-specific files.
  bool is_pnacl = (mime_type() == kPnaclMIMEType);
  nacl::scoped_ptr<JsonManifest> json_manifest(
      new JsonManifest(url_util_,
                       manifest_base_url(),
                       (is_pnacl ? kPortableISA : GetSandboxISA())));
  if (!json_manifest->Init(manifest_json, error_info)) {
    return false;
  }
  manifest_.reset(json_manifest.release());
  return true;
}

void Plugin::UrlDidOpenForStreamAsFile(int32_t pp_error,
                                       FileDownloader*& url_downloader,
                                       PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("Plugin::UrlDidOpen (pp_error=%" NACL_PRId32
                 ", url_downloader=%p)\n", pp_error,
                 static_cast<void*>(url_downloader)));
  url_downloaders_.erase(url_downloader);
  nacl::scoped_ptr<FileDownloader> scoped_url_downloader(url_downloader);
  NaClFileInfo tmp_info(scoped_url_downloader->GetFileInfo());
  NaClFileInfoAutoCloser *info = new NaClFileInfoAutoCloser(&tmp_info);

  if (pp_error != PP_OK) {
    PP_RunCompletionCallback(&callback, pp_error);
    delete info;
  } else if (info->get_desc() > NACL_NO_FILE_DESC) {
    std::map<nacl::string, NaClFileInfoAutoCloser*>::iterator it =
        url_file_info_map_.find(url_downloader->url_to_open());
    if (it != url_file_info_map_.end()) {
      delete it->second;
    }
    url_file_info_map_[url_downloader->url_to_open()] = info;
    PP_RunCompletionCallback(&callback, PP_OK);
  } else {
    PP_RunCompletionCallback(&callback, PP_ERROR_FAILED);
    delete info;
  }
}

struct NaClFileInfo Plugin::GetFileInfo(const nacl::string& url) {
  struct NaClFileInfo info;
  memset(&info, 0, sizeof(info));
  std::map<nacl::string, NaClFileInfoAutoCloser*>::iterator it =
      url_file_info_map_.find(url);
  if (it != url_file_info_map_.end()) {
    info = it->second->get();
    info.desc = DUP(info.desc);
  } else {
    info.desc = -1;
  }
  return info;
}

bool Plugin::StreamAsFile(const nacl::string& url,
                          PP_CompletionCallback callback) {
  PLUGIN_PRINTF(("Plugin::StreamAsFile (url='%s')\n", url.c_str()));
  FileDownloader* downloader = new FileDownloader();
  downloader->Initialize(this);
  url_downloaders_.insert(downloader);
  // Untrusted loads are always relative to the page's origin.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(plugin_base_url()), url);
  if (!resolved_url.is_string()) {
    PLUGIN_PRINTF(("Plugin::StreamAsFile: "
                   "could not resolve url \"%s\" relative to plugin \"%s\".",
                   url.c_str(),
                   plugin_base_url().c_str()));
    return false;
  }

  // Try the fast path first. This will only block if the file is installed.
  if (OpenURLFast(url, downloader)) {
    UrlDidOpenForStreamAsFile(PP_OK, downloader, callback);
    return true;
  }

  pp::CompletionCallback open_callback = callback_factory_.NewCallback(
      &Plugin::UrlDidOpenForStreamAsFile, downloader, callback);
  // If true, will always call the callback on success or failure.
  return downloader->Open(url,
                          DOWNLOAD_TO_FILE,
                          open_callback,
                          true,
                          &UpdateDownloadProgress);
}


void Plugin::ReportLoadSuccess(LengthComputable length_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes) {
  // Set the readyState attribute to indicate loaded.
  set_nacl_ready_state(DONE);
  // Inform JavaScript that loading was successful and is complete.
  const nacl::string& url = nexe_downloader_.url_to_open();
  EnqueueProgressEvent(
      PP_NACL_EVENT_LOAD, url, length_computable, loaded_bytes, total_bytes);
  EnqueueProgressEvent(
      PP_NACL_EVENT_LOADEND, url, length_computable, loaded_bytes, total_bytes);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_SUCCESS, is_installed_);
}


void Plugin::ReportLoadError(const ErrorInfo& error_info) {
  PLUGIN_PRINTF(("Plugin::ReportLoadError (error='%s')\n",
                 error_info.message().c_str()));
  // For errors the user (and not just the developer) should know about,
  // report them to the renderer so the browser can display a message.
  if (error_info.error_code() == ERROR_MANIFEST_PROGRAM_MISSING_ARCH) {
    // A special case: the manifest may otherwise be valid but is missing
    // a program/file compatible with the user's sandbox.
    nacl_interface()->ReportNaClError(pp_instance(),
                                      PP_NACL_MANIFEST_MISSING_ARCH);
  }

  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  set_nexe_error_reported(true);
  // Report an error in lastError and on the JavaScript console.
  nacl::string message = nacl::string("NaCl module load failed: ") +
      error_info.message();
  set_last_error_string(message);
  AddToConsole(nacl::string("NaCl module load failed: ") +
               error_info.console_message());
  // Inform JavaScript that loading encountered an error and is complete.
  EnqueueProgressEvent(PP_NACL_EVENT_ERROR);
  EnqueueProgressEvent(PP_NACL_EVENT_LOADEND);

  // UMA
  HistogramEnumerateLoadStatus(error_info.error_code(), is_installed_);
}


void Plugin::ReportLoadAbort() {
  PLUGIN_PRINTF(("Plugin::ReportLoadAbort\n"));
  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(DONE);
  set_nexe_error_reported(true);
  // Report an error in lastError and on the JavaScript console.
  nacl::string error_string("NaCl module load failed: user aborted");
  set_last_error_string(error_string);
  AddToConsole(error_string);
  // Inform JavaScript that loading was aborted and is complete.
  EnqueueProgressEvent(PP_NACL_EVENT_ABORT);
  EnqueueProgressEvent(PP_NACL_EVENT_LOADEND);

  // UMA
  HistogramEnumerateLoadStatus(ERROR_LOAD_ABORTED, is_installed_);
}

void Plugin::UpdateDownloadProgress(
    PP_Instance pp_instance,
    PP_Resource pp_resource,
    int64_t /*bytes_sent*/,
    int64_t /*total_bytes_to_be_sent*/,
    int64_t bytes_received,
    int64_t total_bytes_to_be_received) {
  Instance* instance = pp::Module::Get()->InstanceForPPInstance(pp_instance);
  if (instance != NULL) {
    Plugin* plugin = static_cast<Plugin*>(instance);
    // Rate limit progress events to a maximum of 100 per second.
    int64_t time = NaClGetTimeOfDayMicroseconds();
    int64_t elapsed = time - plugin->time_of_last_progress_event_;
    const int64_t kTenMilliseconds = 10000;
    if (elapsed > kTenMilliseconds) {
      plugin->time_of_last_progress_event_ = time;

      // Find the URL loader that sent this notification.
      const FileDownloader* file_downloader =
          plugin->FindFileDownloader(pp_resource);
      // If not a streamed file, it must be the .nexe loader.
      if (file_downloader == NULL)
        file_downloader = &plugin->nexe_downloader_;
      nacl::string url = file_downloader->url_to_open();
      LengthComputable length_computable = (total_bytes_to_be_received >= 0) ?
          LENGTH_IS_COMPUTABLE : LENGTH_IS_NOT_COMPUTABLE;

      plugin->EnqueueProgressEvent(PP_NACL_EVENT_PROGRESS,
                                   url,
                                   length_computable,
                                   bytes_received,
                                   total_bytes_to_be_received);
    }
  }
}

const FileDownloader* Plugin::FindFileDownloader(
    PP_Resource url_loader) const {
  const FileDownloader* file_downloader = NULL;
  if (url_loader == nexe_downloader_.url_loader()) {
    file_downloader = &nexe_downloader_;
  } else {
    std::set<FileDownloader*>::const_iterator it = url_downloaders_.begin();
    while (it != url_downloaders_.end()) {
      if (url_loader == (*it)->url_loader()) {
        file_downloader = (*it);
        break;
      }
      ++it;
    }
  }
  return file_downloader;
}

void Plugin::ReportSelLdrLoadStatus(int status) {
  HistogramEnumerateSelLdrLoadStatus(static_cast<NaClErrorCode>(status),
                                     is_installed_);
}

void Plugin::EnqueueProgressEvent(PP_NaClEventType event_type) {
  EnqueueProgressEvent(event_type,
                       NACL_NO_URL,
                       Plugin::LENGTH_IS_NOT_COMPUTABLE,
                       Plugin::kUnknownBytes,
                       Plugin::kUnknownBytes);
}

void Plugin::EnqueueProgressEvent(PP_NaClEventType event_type,
                                  const nacl::string& url,
                                  LengthComputable length_computable,
                                  uint64_t loaded_bytes,
                                  uint64_t total_bytes) {
  PLUGIN_PRINTF(("Plugin::EnqueueProgressEvent ("
                 "event_type='%d', url='%s', length_computable=%d, "
                 "loaded=%" NACL_PRIu64 ", total=%" NACL_PRIu64 ")\n",
                 static_cast<int>(event_type),
                 url.c_str(),
                 static_cast<int>(length_computable),
                 loaded_bytes,
                 total_bytes));

  nacl_interface_->DispatchEvent(
      pp_instance(),
      event_type,
      url.c_str(),
      length_computable == LENGTH_IS_COMPUTABLE ? PP_TRUE : PP_FALSE,
      loaded_bytes,
      total_bytes);
}

bool Plugin::OpenURLFast(const nacl::string& url,
                         FileDownloader* downloader) {
  // Fast path only works for installed file URLs.
  if (GetUrlScheme(url) != SCHEME_CHROME_EXTENSION)
    return false;
  // IMPORTANT: Make sure the document can request the given URL. If we don't
  // check, a malicious app could probe the extension system. This enforces a
  // same-origin policy which prevents the app from requesting resources from
  // another app.
  if (!DocumentCanRequest(url))
    return false;

  uint64_t file_token_lo = 0;
  uint64_t file_token_hi = 0;
  PP_FileHandle file_handle =
      nacl_interface()->OpenNaClExecutable(pp_instance(),
                                           url.c_str(),
                                           &file_token_lo, &file_token_hi);
  // We shouldn't hit this if the file URL is in an installed app.
  if (file_handle == PP_kInvalidFileHandle)
    return false;

  // FileDownloader takes ownership of the file handle.
  downloader->OpenFast(url, file_handle, file_token_lo, file_token_hi);
  return true;
}

UrlSchemeType Plugin::GetUrlScheme(const std::string& url) {
  CHECK(url_util_ != NULL);
  PP_URLComponents_Dev comps;
  pp::Var canonicalized =
      url_util_->Canonicalize(pp::Var(url), &comps);

  if (canonicalized.is_null() ||
      (comps.scheme.begin == 0 && comps.scheme.len == -1)) {
    // |url| was an invalid URL or has no scheme.
    return SCHEME_OTHER;
  }

  CHECK(comps.scheme.begin <
            static_cast<int>(canonicalized.AsString().size()));
  CHECK(comps.scheme.begin + comps.scheme.len <
            static_cast<int>(canonicalized.AsString().size()));

  std::string scheme = canonicalized.AsString().substr(comps.scheme.begin,
                                                       comps.scheme.len);
  if (scheme == kChromeExtensionUriScheme)
    return SCHEME_CHROME_EXTENSION;
  if (scheme == kDataUriScheme)
    return SCHEME_DATA;
  return SCHEME_OTHER;
}

bool Plugin::DocumentCanRequest(const std::string& url) {
  CHECK(url_util_ != NULL);
  return url_util_->DocumentCanRequest(this, pp::Var(url));
}

void Plugin::AddToConsole(const nacl::string& text) {
  pp::Module* module = pp::Module::Get();
  const PPB_Var* var_interface =
      static_cast<const PPB_Var*>(
          module->GetBrowserInterface(PPB_VAR_INTERFACE));
  nacl::string prefix_string("NativeClient");
  PP_Var prefix =
      var_interface->VarFromUtf8(prefix_string.c_str(),
                                 static_cast<uint32_t>(prefix_string.size()));
  PP_Var str = var_interface->VarFromUtf8(text.c_str(),
                                          static_cast<uint32_t>(text.size()));
  const PPB_Console* console_interface =
      static_cast<const PPB_Console*>(
          module->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
  console_interface->LogWithSource(pp_instance(), PP_LOGLEVEL_LOG, prefix, str);
  var_interface->Release(prefix);
  var_interface->Release(str);
}

void Plugin::set_last_error_string(const nacl::string& error) {
  DCHECK(nacl_interface_);
  nacl_interface_->SetReadOnlyProperty(pp_instance(),
                                       pp::Var("lastError").pp_var(),
                                       pp::Var(error).pp_var());
}

void Plugin::set_nacl_ready_state(ReadyState state) {
  nacl_ready_state_ = state;
  DCHECK(nacl_interface_);
  nacl_interface_->SetReadOnlyProperty(pp_instance(),
                                       pp::Var("readyState").pp_var(),
                                       pp::Var(state).pp_var());
}

void Plugin::set_exit_status(int exit_status) {
  pp::Core* core = pp::Module::Get()->core();
  if (core->IsMainThread()) {
    SetExitStatusOnMainThread(PP_OK, exit_status);
  } else {
    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&Plugin::SetExitStatusOnMainThread,
                                      exit_status);
    core->CallOnMainThread(0, callback, 0);
  }
}

void Plugin::SetExitStatusOnMainThread(int32_t pp_error,
                                       int exit_status) {
  DCHECK(pp::Module::Get()->core()->IsMainThread());
  DCHECK(nacl_interface_);
  exit_status_ = exit_status;
  nacl_interface_->SetReadOnlyProperty(pp_instance(),
                                       pp::Var("exitStatus").pp_var(),
                                       pp::Var(exit_status_).pp_var());
}


}  // namespace plugin
