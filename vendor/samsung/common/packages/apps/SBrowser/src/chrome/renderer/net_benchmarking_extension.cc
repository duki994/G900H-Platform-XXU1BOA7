// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net_benchmarking_extension.h"

#include "chrome/common/benchmarking_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include "content/common/view_messages.h"
#endif

using blink::WebCache;

const char kNetBenchmarkingExtensionName[] = "v8/NetBenchmarking";

namespace extensions_v8 {

class NetBenchmarkingWrapper : public v8::Extension {
 public:
  NetBenchmarkingWrapper() :
      v8::Extension(kNetBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.benchmarking) == 'undefined') {"
        "  chrome.benchmarking = {};"
        "};"
        "chrome.benchmarking.clearCache = function() {"
        "  native function ClearCache();"
        "  ClearCache();"
        "};"
        "chrome.benchmarking.clearHostResolverCache = function() {"
        "  native function ClearHostResolverCache();"
        "  ClearHostResolverCache();"
        "};"
        "chrome.benchmarking.clearPredictorCache = function() {"
        "  native function ClearPredictorCache();"
        "  ClearPredictorCache();"
        "};"
        "chrome.benchmarking.closeConnections = function() {"
        "  native function CloseConnections();"
        "  CloseConnections();"
        "};"
        "chrome.benchmarking.enableSpdy = function(name) {"
        "  native function EnableSpdy();"
        "  EnableSpdy(name);"
        "};"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "ClearCache"))) {
      return v8::FunctionTemplate::New(isolate, ClearCache);
    } else if (name->Equals(v8::String::NewFromUtf8(
                   isolate, "ClearHostResolverCache"))) {
      return v8::FunctionTemplate::New(isolate, ClearHostResolverCache);
    } else if (name->Equals(
                   v8::String::NewFromUtf8(isolate, "ClearPredictorCache"))) {
      return v8::FunctionTemplate::New(isolate, ClearPredictorCache);
    } else if (name->Equals(v8::String::NewFromUtf8(isolate, "EnableSpdy"))) {
      return v8::FunctionTemplate::New(isolate, EnableSpdy);
    } else if (name->Equals(
                   v8::String::NewFromUtf8(isolate, "CloseConnections"))) {
      return v8::FunctionTemplate::New(isolate, CloseConnections);
    }

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void ClearCache(const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(new ChromeViewHostMsg_ClearCache(&rv));
    WebCache::clear();
  }

  static void ClearHostResolverCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_ClearHostResolverCache(&rv));
  }

  static void ClearPredictorCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_ClearPredictorCache(&rv));
  }

  static void CloseConnections(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_CloseCurrentConnections());
  }

  static void EnableSpdy(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!args.Length() || !args[0]->IsBoolean())
      return;

    content::RenderThread::Get()->Send(new ChromeViewHostMsg_EnableSpdy(
        args[0]->BooleanValue()));
  }

#if defined(OS_ANDROID)
  static void SSRMMode(int count) {
    content::RenderThread* thread = content::RenderThread::Get();
    if (thread) {
      int routing_id = thread->GetLastViewId();
      thread->Send(new ViewHostMsg_OnSSRMModeCallback(routing_id, 0, count));  // 0 is v8
    }
  }
#endif
};

v8::Extension* NetBenchmarkingExtension::Get() {
  return new NetBenchmarkingWrapper();
}

#if defined(OS_ANDROID)
static void SSRMModeCallback(v8::WebKitAdapter::ssrmMode mode, int flags) {
  extensions_v8::NetBenchmarkingWrapper::SSRMMode(flags);
}
static v8::WebKitAdapter s_adapter(&SSRMModeCallback);
#endif

}  // namespace extensions_v8