/*
 * libjingle
 * Copyright 2013, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Hints for future visitors:
// This entire file is an implementation detail of the org.webrtc Java package,
// the most interesting bits of which are org.webrtc.PeerConnection{,Factory}.
// The layout of this file is roughly:
// - various helper C++ functions & classes that wrap Java counterparts and
//   expose a C++ interface that can be passed to the C++ PeerConnection APIs
// - implementations of methods declared "static" in the Java package (named
//   things like Java_org_webrtc_OMG_Can_This_Name_Be_Any_Longer, prescribed by
//   the JNI spec).
//
// Lifecycle notes: objects are owned where they will be called; in other words
// FooObservers are owned by C++-land, and user-callable objects (e.g.
// PeerConnection and VideoTrack) are owned by Java-land.
// When this file allocates C++ RefCountInterfaces it AddRef()s an artificial
// ref simulating the jlong held in Java-land, and then Release()s the ref in
// the respective free call.  Sometimes this AddRef is implicit in the
// construction of a scoped_refptr<> which is then .release()d.
// Any persistent (non-local) references from C++ to Java must be global or weak
// (in which case they must be checked before use)!
//
// Exception notes: pretty much all JNI calls can throw Java exceptions, so each
// call through a JNIEnv* pointer needs to be followed by an ExceptionCheck()
// call.  In this file this is done in CHECK_EXCEPTION, making for much easier
// debugging in case of failure (the alternative is to wait for control to
// return to the Java frame that called code in this file, at which point it's
// impossible to tell which JNI call broke).

#include <jni.h>
#undef JNIEXPORT
#define JNIEXPORT __attribute__((visibility("default")))

#include <asm/unistd.h>
#include <limits>
#include <map>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/base/bind.h"
#include "talk/base/logging.h"
#include "talk/base/messagequeue.h"
#include "talk/base/ssladapter.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "talk/media/devices/videorendererfactory.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/voice_engine/include/voe_base.h"

#ifdef ANDROID
#include "webrtc/system_wrappers/interface/logcat_trace_context.h"
using webrtc::LogcatTraceContext;
#endif

using icu::UnicodeString;
using talk_base::Bind;
using talk_base::Thread;
using talk_base::ThreadManager;
using talk_base::scoped_ptr;
using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::CreateSessionDescriptionObserver;
using webrtc::DataBuffer;
using webrtc::DataChannelInit;
using webrtc::DataChannelInterface;
using webrtc::DataChannelObserver;
using webrtc::IceCandidateInterface;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::PeerConnectionFactoryInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::SessionDescriptionInterface;
using webrtc::SetSessionDescriptionObserver;
using webrtc::StatsObserver;
using webrtc::StatsReport;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;
using webrtc::kVideoCodecVP8;

// Abort the process if |x| is false, emitting |msg|.
#define CHECK(x, msg)                                                          \
  if (x) {} else {                                                             \
    LOG(LS_ERROR) << __FILE__ << ":" << __LINE__ << ": " << msg;               \
    abort();                                                                   \
  }
// Abort the process if |jni| has a Java exception pending, emitting |msg|.
#define CHECK_EXCEPTION(jni, msg)                                              \
  if (0) {} else {                                                             \
    if (jni->ExceptionCheck()) {                                               \
      jni->ExceptionDescribe();                                                \
      jni->ExceptionClear();                                                   \
      CHECK(0, msg);                                                           \
    }                                                                          \
  }

// Helper that calls ptr->Release() and logs a useful message if that didn't
// actually delete *ptr because of extra refcounts.
#define CHECK_RELEASE(ptr)                                        \
  do {                                                            \
    int count = (ptr)->Release();                                 \
    if (count != 0) {                                             \
      LOG(LS_ERROR) << "Refcount unexpectedly not 0: " << (ptr)   \
                    << ": " << count;                             \
    }                                                             \
    CHECK(!count, "Unexpected refcount");                         \
  } while (0)

namespace {

static JavaVM* g_jvm = NULL;  // Set in JNI_OnLoad().

static pthread_once_t g_jni_ptr_once = PTHREAD_ONCE_INIT;
static pthread_key_t g_jni_ptr;  // Key for per-thread JNIEnv* data.

// Return thread ID as a string.
static std::string GetThreadId() {
  char buf[21];  // Big enough to hold a kuint64max plus terminating NULL.
  CHECK(snprintf(buf, sizeof(buf), "%llu", syscall(__NR_gettid)) <= sizeof(buf),
        "Thread id is bigger than uint64??");
  return std::string(buf);
}

// Return the current thread's name.
static std::string GetThreadName() {
  char name[17];
  CHECK(prctl(PR_GET_NAME, name) == 0, "prctl(PR_GET_NAME) failed");
  name[16] = '\0';
  return std::string(name);
}

static void ThreadDestructor(void* unused) {
  jint status = g_jvm->DetachCurrentThread();
  CHECK(status == JNI_OK, "Failed to detach thread: " << status);
}

static void CreateJNIPtrKey() {
  CHECK(!pthread_key_create(&g_jni_ptr, &ThreadDestructor),
        "pthread_key_create");
}

// Deal with difference in signatures between Oracle's jni.h and Android's.
static JNIEnv* AttachCurrentThreadIfNeeded() {
  CHECK(!pthread_once(&g_jni_ptr_once, &CreateJNIPtrKey),
        "pthread_once");
  JNIEnv* jni = reinterpret_cast<JNIEnv*>(pthread_getspecific(g_jni_ptr));
  if (jni == NULL) {
#ifdef _JAVASOFT_JNI_H_  // Oracle's jni.h violates the JNI spec!
    void* env;
#else
    JNIEnv* env;
#endif
    char* name = strdup((GetThreadName() + " - " + GetThreadId()).c_str());
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = name;
    args.group = NULL;
    CHECK(!g_jvm->AttachCurrentThread(&env, &args), "Failed to attach thread");
    free(name);
    CHECK(env, "AttachCurrentThread handed back NULL!");
    jni = reinterpret_cast<JNIEnv*>(env);
    CHECK(!pthread_setspecific(g_jni_ptr, jni), "pthread_setspecific");
  }
  return jni;
}

// Return a |jlong| that will correctly convert back to |ptr|.  This is needed
// because the alternative (of silently passing a 32-bit pointer to a vararg
// function expecting a 64-bit param) picks up garbage in the high 32 bits.
static jlong jlongFromPointer(void* ptr) {
  COMPILE_ASSERT(sizeof(intptr_t) <= sizeof(jlong),
                 Time_to_rethink_the_use_of_jlongs);
  // Going through intptr_t to be obvious about the definedness of the
  // conversion from pointer to integral type.  intptr_t to jlong is a standard
  // widening by the COMPILE_ASSERT above.
  jlong ret = reinterpret_cast<intptr_t>(ptr);
  assert(reinterpret_cast<void*>(ret) == ptr);
  return ret;
}

// Android's FindClass() is trickier than usual because the app-specific
// ClassLoader is not consulted when there is no app-specific frame on the
// stack.  Consequently, we only look up classes once in JNI_OnLoad.
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
class ClassReferenceHolder {
 public:
  explicit ClassReferenceHolder(JNIEnv* jni) {
    LoadClass(jni, "java/nio/ByteBuffer");
    LoadClass(jni, "org/webrtc/AudioTrack");
    LoadClass(jni, "org/webrtc/DataChannel");
    LoadClass(jni, "org/webrtc/DataChannel$Buffer");
    LoadClass(jni, "org/webrtc/DataChannel$Init");
    LoadClass(jni, "org/webrtc/DataChannel$State");
    LoadClass(jni, "org/webrtc/IceCandidate");
#ifdef ANDROID
    LoadClass(jni, "org/webrtc/MediaCodecVideoEncoder");
    LoadClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
#endif
    LoadClass(jni, "org/webrtc/MediaSource$State");
    LoadClass(jni, "org/webrtc/MediaStream");
    LoadClass(jni, "org/webrtc/MediaStreamTrack$State");
    LoadClass(jni, "org/webrtc/PeerConnection$IceConnectionState");
    LoadClass(jni, "org/webrtc/PeerConnection$IceGatheringState");
    LoadClass(jni, "org/webrtc/PeerConnection$SignalingState");
    LoadClass(jni, "org/webrtc/SessionDescription");
    LoadClass(jni, "org/webrtc/SessionDescription$Type");
    LoadClass(jni, "org/webrtc/StatsReport");
    LoadClass(jni, "org/webrtc/StatsReport$Value");
    LoadClass(jni, "org/webrtc/VideoRenderer$I420Frame");
    LoadClass(jni, "org/webrtc/VideoTrack");
  }

  ~ClassReferenceHolder() {
    CHECK(classes_.empty(), "Must call FreeReferences() before dtor!");
  }

  void FreeReferences(JNIEnv* jni) {
    for (std::map<std::string, jclass>::const_iterator it = classes_.begin();
         it != classes_.end(); ++it) {
      jni->DeleteGlobalRef(it->second);
    }
    classes_.clear();
  }

  jclass GetClass(const std::string& name) {
    std::map<std::string, jclass>::iterator it = classes_.find(name);
    CHECK(it != classes_.end(), "Unexpected GetClass() call for: " << name);
    return it->second;
  }

 private:
  void LoadClass(JNIEnv* jni, const std::string& name) {
    jclass localRef = jni->FindClass(name.c_str());
    CHECK_EXCEPTION(jni, "error during FindClass: " << name);
    CHECK(localRef, name);
    jclass globalRef = reinterpret_cast<jclass>(jni->NewGlobalRef(localRef));
    CHECK_EXCEPTION(jni, "error during NewGlobalRef: " << name);
    CHECK(globalRef, name);
    bool inserted = classes_.insert(std::make_pair(name, globalRef)).second;
    CHECK(inserted, "Duplicate class name: " << name);
  }

  std::map<std::string, jclass> classes_;
};

// Allocated in JNI_OnLoad(), freed in JNI_OnUnLoad().
static ClassReferenceHolder* g_class_reference_holder = NULL;

// JNIEnv-helper methods that CHECK success: no Java exception thrown and found
// object/class/method/field is non-null.
jmethodID GetMethodID(
    JNIEnv* jni, jclass c, const std::string& name, const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_EXCEPTION(jni,
                  "error during GetMethodID: " << name << ", " << signature);
  CHECK(m, name << ", " << signature);
  return m;
}

jmethodID GetStaticMethodID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jmethodID m = jni->GetStaticMethodID(c, name, signature);
  CHECK_EXCEPTION(jni,
                  "error during GetStaticMethodID: "
                  << name << ", " << signature);
  CHECK(m, name << ", " << signature);
  return m;
}

jfieldID GetFieldID(
    JNIEnv* jni, jclass c, const char* name, const char* signature) {
  jfieldID f = jni->GetFieldID(c, name, signature);
  CHECK_EXCEPTION(jni, "error during GetFieldID");
  CHECK(f, name << ", " << signature);
  return f;
}

// Returns a global reference guaranteed to be valid for the lifetime of the
// process.
jclass FindClass(JNIEnv* jni, const char* name) {
  return g_class_reference_holder->GetClass(name);
}

jclass GetObjectClass(JNIEnv* jni, jobject object) {
  jclass c = jni->GetObjectClass(object);
  CHECK_EXCEPTION(jni, "error during GetObjectClass");
  CHECK(c, "");
  return c;
}

jobject GetObjectField(JNIEnv* jni, jobject object, jfieldID id) {
  jobject o = jni->GetObjectField(object, id);
  CHECK_EXCEPTION(jni, "error during GetObjectField");
  CHECK(o, "");
  return o;
}

jstring GetStringField(JNIEnv* jni, jobject object, jfieldID id) {
  return static_cast<jstring>(GetObjectField(jni, object, id));
}

jlong GetLongField(JNIEnv* jni, jobject object, jfieldID id) {
  jlong l = jni->GetLongField(object, id);
  CHECK_EXCEPTION(jni, "error during GetLongField");
  return l;
}

jint GetIntField(JNIEnv* jni, jobject object, jfieldID id) {
  jint i = jni->GetIntField(object, id);
  CHECK_EXCEPTION(jni, "error during GetIntField");
  return i;
}

bool GetBooleanField(JNIEnv* jni, jobject object, jfieldID id) {
  jboolean b = jni->GetBooleanField(object, id);
  CHECK_EXCEPTION(jni, "error during GetBooleanField");
  return b;
}

jobject NewGlobalRef(JNIEnv* jni, jobject o) {
  jobject ret = jni->NewGlobalRef(o);
  CHECK_EXCEPTION(jni, "error during NewGlobalRef");
  CHECK(ret, "");
  return ret;
}

void DeleteGlobalRef(JNIEnv* jni, jobject o) {
  jni->DeleteGlobalRef(o);
  CHECK_EXCEPTION(jni, "error during DeleteGlobalRef");
}

// Given a jweak reference, allocate a (strong) local reference scoped to the
// lifetime of this object if the weak reference is still valid, or NULL
// otherwise.
class WeakRef {
 public:
  WeakRef(JNIEnv* jni, jweak ref)
      : jni_(jni), obj_(jni_->NewLocalRef(ref)) {
    CHECK_EXCEPTION(jni, "error during NewLocalRef");
  }
  ~WeakRef() {
    if (obj_) {
      jni_->DeleteLocalRef(obj_);
      CHECK_EXCEPTION(jni_, "error during DeleteLocalRef");
    }
  }
  jobject obj() { return obj_; }

 private:
  JNIEnv* const jni_;
  jobject const obj_;
};

// Scope Java local references to the lifetime of this object.  Use in all C++
// callbacks (i.e. entry points that don't originate in a Java callstack
// through a "native" method call).
class ScopedLocalRefFrame {
 public:
  explicit ScopedLocalRefFrame(JNIEnv* jni) : jni_(jni) {
    CHECK(!jni_->PushLocalFrame(0), "Failed to PushLocalFrame");
  }
  ~ScopedLocalRefFrame() {
    jni_->PopLocalFrame(NULL);
  }

 private:
  JNIEnv* jni_;
};

// Scoped holder for global Java refs.
template<class T>  // T is jclass, jobject, jintArray, etc.
class ScopedGlobalRef {
 public:
  ScopedGlobalRef(JNIEnv* jni, T obj)
      : obj_(static_cast<T>(jni->NewGlobalRef(obj))) {}
  ~ScopedGlobalRef() {
    DeleteGlobalRef(AttachCurrentThreadIfNeeded(), obj_);
  }
  T operator*() const {
    return obj_;
  }
 private:
  T obj_;
};

// Java references to "null" can only be distinguished as such in C++ by
// creating a local reference, so this helper wraps that logic.
static bool IsNull(JNIEnv* jni, jobject obj) {
  ScopedLocalRefFrame local_ref_frame(jni);
  return jni->NewLocalRef(obj) == NULL;
}

// Return the (singleton) Java Enum object corresponding to |index|;
// |state_class_fragment| is something like "MediaSource$State".
jobject JavaEnumFromIndex(
    JNIEnv* jni, const std::string& state_class_fragment, int index) {
  std::string state_class_name = "org/webrtc/" + state_class_fragment;
  jclass state_class = FindClass(jni, state_class_name.c_str());
  jmethodID state_values_id = GetStaticMethodID(
      jni, state_class, "values", ("()[L" + state_class_name  + ";").c_str());
  jobjectArray state_values = static_cast<jobjectArray>(
      jni->CallStaticObjectMethod(state_class, state_values_id));
  CHECK_EXCEPTION(jni, "error during CallStaticObjectMethod");
  jobject ret = jni->GetObjectArrayElement(state_values, index);
  CHECK_EXCEPTION(jni, "error during GetObjectArrayElement");
  return ret;
}

// Given a UTF-8 encoded |native| string return a new (UTF-16) jstring.
static jstring JavaStringFromStdString(JNIEnv* jni, const std::string& native) {
  UnicodeString ustr(UnicodeString::fromUTF8(native));
  jstring jstr = jni->NewString(ustr.getBuffer(), ustr.length());
  CHECK_EXCEPTION(jni, "error during NewString");
  return jstr;
}

// Given a (UTF-16) jstring return a new UTF-8 native string.
static std::string JavaToStdString(JNIEnv* jni, const jstring& j_string) {
  const jchar* jchars = jni->GetStringChars(j_string, NULL);
  CHECK_EXCEPTION(jni, "Error during GetStringChars");
  UnicodeString ustr(jchars, jni->GetStringLength(j_string));
  CHECK_EXCEPTION(jni, "Error during GetStringLength");
  jni->ReleaseStringChars(j_string, jchars);
  CHECK_EXCEPTION(jni, "Error during ReleaseStringChars");
  std::string ret;
  return ustr.toUTF8String(ret);
}

static DataChannelInit JavaDataChannelInitToNative(
    JNIEnv* jni, jobject j_init) {
  DataChannelInit init;

  jclass j_init_class = FindClass(jni, "org/webrtc/DataChannel$Init");
  jfieldID ordered_id = GetFieldID(jni, j_init_class, "ordered", "Z");
  jfieldID max_retransmit_time_id =
      GetFieldID(jni, j_init_class, "maxRetransmitTimeMs", "I");
  jfieldID max_retransmits_id =
      GetFieldID(jni, j_init_class, "maxRetransmits", "I");
  jfieldID protocol_id =
      GetFieldID(jni, j_init_class, "protocol", "Ljava/lang/String;");
  jfieldID negotiated_id = GetFieldID(jni, j_init_class, "negotiated", "Z");
  jfieldID id_id = GetFieldID(jni, j_init_class, "id", "I");

  init.ordered = GetBooleanField(jni, j_init, ordered_id);
  init.maxRetransmitTime = GetIntField(jni, j_init, max_retransmit_time_id);
  init.maxRetransmits = GetIntField(jni, j_init, max_retransmits_id);
  init.protocol = JavaToStdString(
      jni, GetStringField(jni, j_init, protocol_id));
  init.negotiated = GetBooleanField(jni, j_init, negotiated_id);
  init.id = GetIntField(jni, j_init, id_id);

  return init;
}

class ConstraintsWrapper;

// Adapter between the C++ PeerConnectionObserver interface and the Java
// PeerConnection.Observer interface.  Wraps an instance of the Java interface
// and dispatches C++ callbacks to Java.
class PCOJava : public PeerConnectionObserver {
 public:
  PCOJava(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, *j_observer_global_)),
        j_media_stream_class_(jni, FindClass(jni, "org/webrtc/MediaStream")),
        j_media_stream_ctor_(GetMethodID(
            jni, *j_media_stream_class_, "<init>", "(J)V")),
        j_audio_track_class_(jni, FindClass(jni, "org/webrtc/AudioTrack")),
        j_audio_track_ctor_(GetMethodID(
            jni, *j_audio_track_class_, "<init>", "(J)V")),
        j_video_track_class_(jni, FindClass(jni, "org/webrtc/VideoTrack")),
        j_video_track_ctor_(GetMethodID(
            jni, *j_video_track_class_, "<init>", "(J)V")),
        j_data_channel_class_(jni, FindClass(jni, "org/webrtc/DataChannel")),
        j_data_channel_ctor_(GetMethodID(
            jni, *j_data_channel_class_, "<init>", "(J)V")) {
  }

  virtual ~PCOJava() {}

  virtual void OnIceCandidate(const IceCandidateInterface* candidate) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    std::string sdp;
    CHECK(candidate->ToString(&sdp), "got so far: " << sdp);
    jclass candidate_class = FindClass(jni(), "org/webrtc/IceCandidate");
    jmethodID ctor = GetMethodID(jni(), candidate_class,
        "<init>", "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring j_mid = JavaStringFromStdString(jni(), candidate->sdp_mid());
    jstring j_sdp = JavaStringFromStdString(jni(), sdp);
    jobject j_candidate = jni()->NewObject(
        candidate_class, ctor, j_mid, candidate->sdp_mline_index(), j_sdp);
    CHECK_EXCEPTION(jni(), "error during NewObject");
    jmethodID m = GetMethodID(jni(), *j_observer_class_,
                              "onIceCandidate", "(Lorg/webrtc/IceCandidate;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_candidate);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnError() OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onError", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onSignalingChange",
        "(Lorg/webrtc/PeerConnection$SignalingState;)V");
    jobject new_state_enum =
        JavaEnumFromIndex(jni(), "PeerConnection$SignalingState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceConnectionChange",
        "(Lorg/webrtc/PeerConnection$IceConnectionState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceConnectionState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onIceGatheringChange",
        "(Lorg/webrtc/PeerConnection$IceGatheringState;)V");
    jobject new_state_enum = JavaEnumFromIndex(
        jni(), "PeerConnection$IceGatheringState", new_state);
    jni()->CallVoidMethod(*j_observer_global_, m, new_state_enum);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnAddStream(MediaStreamInterface* stream) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_stream = jni()->NewObject(
        *j_media_stream_class_, j_media_stream_ctor_, (jlong)stream);
    CHECK_EXCEPTION(jni(), "error during NewObject");

    AudioTrackVector audio_tracks = stream->GetAudioTracks();
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      AudioTrackInterface* track = audio_tracks[i];
      jstring id = JavaStringFromStdString(jni(), track->id());
      jobject j_track = jni()->NewObject(
          *j_audio_track_class_, j_audio_track_ctor_, (jlong)track, id);
      CHECK_EXCEPTION(jni(), "error during NewObject");
      jfieldID audio_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "audioTracks",
                                            "Ljava/util/LinkedList;");
      jobject audio_tracks = GetObjectField(jni(), j_stream, audio_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), audio_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(audio_tracks, add, j_track);
      CHECK_EXCEPTION(jni(), "error during CallBooleanMethod");
      CHECK(added, "");
    }

    VideoTrackVector video_tracks = stream->GetVideoTracks();
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      VideoTrackInterface* track = video_tracks[i];
      jstring id = JavaStringFromStdString(jni(), track->id());
      jobject j_track = jni()->NewObject(
          *j_video_track_class_, j_video_track_ctor_, (jlong)track, id);
      CHECK_EXCEPTION(jni(), "error during NewObject");
      jfieldID video_tracks_id = GetFieldID(jni(),
                                            *j_media_stream_class_,
                                            "videoTracks",
                                            "Ljava/util/LinkedList;");
      jobject video_tracks = GetObjectField(jni(), j_stream, video_tracks_id);
      jmethodID add = GetMethodID(jni(),
                                  GetObjectClass(jni(), video_tracks),
                                  "add",
                                  "(Ljava/lang/Object;)Z");
      jboolean added = jni()->CallBooleanMethod(video_tracks, add, j_track);
      CHECK_EXCEPTION(jni(), "error during CallBooleanMethod");
      CHECK(added, "");
    }
    streams_[stream] = jni()->NewWeakGlobalRef(j_stream);
    CHECK_EXCEPTION(jni(), "error during NewWeakGlobalRef");

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onAddStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_stream);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnRemoveStream(MediaStreamInterface* stream) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    NativeToJavaStreamsMap::iterator it = streams_.find(stream);
    CHECK(it != streams_.end(), "unexpected stream: " << std::hex << stream);

    WeakRef s(jni(), it->second);
    streams_.erase(it);
    if (!s.obj())
      return;

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onRemoveStream",
                              "(Lorg/webrtc/MediaStream;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, s.obj());
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnDataChannel(DataChannelInterface* channel) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_channel = jni()->NewObject(
        *j_data_channel_class_, j_data_channel_ctor_, (jlong)channel);
    CHECK_EXCEPTION(jni(), "error during NewObject");

    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onDataChannel",
                              "(Lorg/webrtc/DataChannel;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_channel);

    // Channel is now owned by Java object, and will be freed from
    // DataChannel.dispose().  Important that this be done _after_ the
    // CallVoidMethod above as Java code might call back into native code and be
    // surprised to see a refcount of 2.
    int bumped_count = channel->AddRef();
    CHECK(bumped_count == 2, "Unexpected refcount OnDataChannel");

    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnRenegotiationNeeded() OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m =
        GetMethodID(jni(), *j_observer_class_, "onRenegotiationNeeded", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  void SetConstraints(ConstraintsWrapper* constraints) {
    CHECK(!constraints_.get(), "constraints already set!");
    constraints_.reset(constraints);
  }

  const ConstraintsWrapper* constraints() { return constraints_.get(); }

 private:
  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_media_stream_class_;
  const jmethodID j_media_stream_ctor_;
  const ScopedGlobalRef<jclass> j_audio_track_class_;
  const jmethodID j_audio_track_ctor_;
  const ScopedGlobalRef<jclass> j_video_track_class_;
  const jmethodID j_video_track_ctor_;
  const ScopedGlobalRef<jclass> j_data_channel_class_;
  const jmethodID j_data_channel_ctor_;
  typedef std::map<void*, jweak> NativeToJavaStreamsMap;
  NativeToJavaStreamsMap streams_;  // C++ -> Java streams.
  scoped_ptr<ConstraintsWrapper> constraints_;
};

// Wrapper for a Java MediaConstraints object.  Copies all needed data so when
// the constructor returns the Java object is no longer needed.
class ConstraintsWrapper : public MediaConstraintsInterface {
 public:
  ConstraintsWrapper(JNIEnv* jni, jobject j_constraints) {
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "mandatory", &mandatory_);
    PopulateConstraintsFromJavaPairList(
        jni, j_constraints, "optional", &optional_);
  }

  virtual ~ConstraintsWrapper() {}

  // MediaConstraintsInterface.
  virtual const Constraints& GetMandatory() const OVERRIDE {
    return mandatory_;
  }

  virtual const Constraints& GetOptional() const OVERRIDE {
    return optional_;
  }

 private:
  // Helper for translating a List<Pair<String, String>> to a Constraints.
  static void PopulateConstraintsFromJavaPairList(
      JNIEnv* jni, jobject j_constraints,
      const char* field_name, Constraints* field) {
    jfieldID j_id = GetFieldID(jni,
        GetObjectClass(jni, j_constraints), field_name, "Ljava/util/List;");
    jobject j_list = GetObjectField(jni, j_constraints, j_id);
    jmethodID j_iterator_id = GetMethodID(jni,
        GetObjectClass(jni, j_list), "iterator", "()Ljava/util/Iterator;");
    jobject j_iterator = jni->CallObjectMethod(j_list, j_iterator_id);
    CHECK_EXCEPTION(jni, "error during CallObjectMethod");
    jmethodID j_has_next = GetMethodID(jni,
        GetObjectClass(jni, j_iterator), "hasNext", "()Z");
    jmethodID j_next = GetMethodID(jni,
        GetObjectClass(jni, j_iterator), "next", "()Ljava/lang/Object;");
    while (jni->CallBooleanMethod(j_iterator, j_has_next)) {
      CHECK_EXCEPTION(jni, "error during CallBooleanMethod");
      jobject entry = jni->CallObjectMethod(j_iterator, j_next);
      CHECK_EXCEPTION(jni, "error during CallObjectMethod");
      jmethodID get_key = GetMethodID(jni,
          GetObjectClass(jni, entry), "getKey", "()Ljava/lang/String;");
      jstring j_key = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_key));
      CHECK_EXCEPTION(jni, "error during CallObjectMethod");
      jmethodID get_value = GetMethodID(jni,
          GetObjectClass(jni, entry), "getValue", "()Ljava/lang/String;");
      jstring j_value = reinterpret_cast<jstring>(
          jni->CallObjectMethod(entry, get_value));
      CHECK_EXCEPTION(jni, "error during CallObjectMethod");
      field->push_back(Constraint(JavaToStdString(jni, j_key),
                                  JavaToStdString(jni, j_value)));
    }
    CHECK_EXCEPTION(jni, "error during CallBooleanMethod");
  }

  Constraints mandatory_;
  Constraints optional_;
};

static jobject JavaSdpFromNativeSdp(
    JNIEnv* jni, const SessionDescriptionInterface* desc) {
  std::string sdp;
  CHECK(desc->ToString(&sdp), "got so far: " << sdp);
  jstring j_description = JavaStringFromStdString(jni, sdp);

  jclass j_type_class = FindClass(
      jni, "org/webrtc/SessionDescription$Type");
  jmethodID j_type_from_canonical = GetStaticMethodID(
      jni, j_type_class, "fromCanonicalForm",
      "(Ljava/lang/String;)Lorg/webrtc/SessionDescription$Type;");
  jstring j_type_string = JavaStringFromStdString(jni, desc->type());
  jobject j_type = jni->CallStaticObjectMethod(
      j_type_class, j_type_from_canonical, j_type_string);
  CHECK_EXCEPTION(jni, "error during CallObjectMethod");

  jclass j_sdp_class = FindClass(jni, "org/webrtc/SessionDescription");
  jmethodID j_sdp_ctor = GetMethodID(
      jni, j_sdp_class, "<init>",
      "(Lorg/webrtc/SessionDescription$Type;Ljava/lang/String;)V");
  jobject j_sdp = jni->NewObject(
      j_sdp_class, j_sdp_ctor, j_type, j_description);
  CHECK_EXCEPTION(jni, "error during NewObject");
  return j_sdp;
}

template <class T>  // T is one of {Create,Set}SessionDescriptionObserver.
class SdpObserverWrapper : public T {
 public:
  SdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                     ConstraintsWrapper* constraints)
      : constraints_(constraints),
        j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)) {
  }

  virtual ~SdpObserverWrapper() {}

  // Can't mark OVERRIDE because of templating.
  virtual void OnSuccess() {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onSetSuccess", "()V");
    jni()->CallVoidMethod(*j_observer_global_, m);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  // Can't mark OVERRIDE because of templating.
  virtual void OnSuccess(SessionDescriptionInterface* desc) {
    ScopedLocalRefFrame local_ref_frame(jni());
    jmethodID m = GetMethodID(
        jni(), *j_observer_class_, "onCreateSuccess",
        "(Lorg/webrtc/SessionDescription;)V");
    jobject j_sdp = JavaSdpFromNativeSdp(jni(), desc);
    jni()->CallVoidMethod(*j_observer_global_, m, j_sdp);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

 protected:
  // Common implementation for failure of Set & Create types, distinguished by
  // |op| being "Set" or "Create".
  void OnFailure(const std::string& op, const std::string& error) {
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "on" + op + "Failure",
                              "(Ljava/lang/String;)V");
    jstring j_error_string = JavaStringFromStdString(jni(), error);
    jni()->CallVoidMethod(*j_observer_global_, m, j_error_string);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

 private:
  scoped_ptr<ConstraintsWrapper> constraints_;
  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
};

class CreateSdpObserverWrapper
    : public SdpObserverWrapper<CreateSessionDescriptionObserver> {
 public:
  CreateSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                           ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  virtual void OnFailure(const std::string& error) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::OnFailure(std::string("Create"), error);
  }
};

class SetSdpObserverWrapper
    : public SdpObserverWrapper<SetSessionDescriptionObserver> {
 public:
  SetSdpObserverWrapper(JNIEnv* jni, jobject j_observer,
                        ConstraintsWrapper* constraints)
      : SdpObserverWrapper(jni, j_observer, constraints) {}

  virtual void OnFailure(const std::string& error) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    SdpObserverWrapper::OnFailure(std::string("Set"), error);
  }
};

// Adapter for a Java DataChannel$Observer presenting a C++ DataChannelObserver
// and dispatching the callback from C++ back to Java.
class DataChannelObserverWrapper : public DataChannelObserver {
 public:
  DataChannelObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_on_state_change_mid_(GetMethodID(jni, *j_observer_class_,
                                           "onStateChange", "()V")),
        j_on_message_mid_(GetMethodID(jni, *j_observer_class_, "onMessage",
                                      "(Lorg/webrtc/DataChannel$Buffer;)V")),
        j_buffer_class_(jni, FindClass(jni, "org/webrtc/DataChannel$Buffer")),
        j_buffer_ctor_(GetMethodID(jni, *j_buffer_class_,
                                   "<init>", "(Ljava/nio/ByteBuffer;Z)V")) {
  }

  virtual ~DataChannelObserverWrapper() {}

  virtual void OnStateChange() OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_observer_global_, j_on_state_change_mid_);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

  virtual void OnMessage(const DataBuffer& buffer) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject byte_buffer =
        jni()->NewDirectByteBuffer(const_cast<char*>(buffer.data.data()),
                                   buffer.data.length());
    jobject j_buffer = jni()->NewObject(*j_buffer_class_, j_buffer_ctor_,
                                        byte_buffer, buffer.binary);
    jni()->CallVoidMethod(*j_observer_global_, j_on_message_mid_, j_buffer);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

 private:
  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_buffer_class_;
  const jmethodID j_on_state_change_mid_;
  const jmethodID j_on_message_mid_;
  const jmethodID j_buffer_ctor_;
};

// Adapter for a Java StatsObserver presenting a C++ StatsObserver and
// dispatching the callback from C++ back to Java.
class StatsObserverWrapper : public StatsObserver {
 public:
  StatsObserverWrapper(JNIEnv* jni, jobject j_observer)
      : j_observer_global_(jni, j_observer),
        j_observer_class_(jni, GetObjectClass(jni, j_observer)),
        j_stats_report_class_(jni, FindClass(jni, "org/webrtc/StatsReport")),
        j_stats_report_ctor_(GetMethodID(
            jni, *j_stats_report_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;D"
            "[Lorg/webrtc/StatsReport$Value;)V")),
        j_value_class_(jni, FindClass(
            jni, "org/webrtc/StatsReport$Value")),
        j_value_ctor_(GetMethodID(
            jni, *j_value_class_, "<init>",
            "(Ljava/lang/String;Ljava/lang/String;)V")) {
  }

  virtual ~StatsObserverWrapper() {}

  virtual void OnComplete(const std::vector<StatsReport>& reports) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobjectArray j_reports = ReportsToJava(jni(), reports);
    jmethodID m = GetMethodID(jni(), *j_observer_class_, "onComplete",
                              "([Lorg/webrtc/StatsReport;)V");
    jni()->CallVoidMethod(*j_observer_global_, m, j_reports);
    CHECK_EXCEPTION(jni(), "error during CallVoidMethod");
  }

 private:
  jobjectArray ReportsToJava(
      JNIEnv* jni, const std::vector<StatsReport>& reports) {
    jobjectArray reports_array = jni->NewObjectArray(
        reports.size(), *j_stats_report_class_, NULL);
    for (int i = 0; i < reports.size(); ++i) {
      ScopedLocalRefFrame local_ref_frame(jni);
      const StatsReport& report = reports[i];
      jstring j_id = JavaStringFromStdString(jni, report.id);
      jstring j_type = JavaStringFromStdString(jni, report.type);
      jobjectArray j_values = ValuesToJava(jni, report.values);
      jobject j_report = jni->NewObject(*j_stats_report_class_,
                                        j_stats_report_ctor_,
                                        j_id,
                                        j_type,
                                        report.timestamp,
                                        j_values);
      jni->SetObjectArrayElement(reports_array, i, j_report);
    }
    return reports_array;
  }

  jobjectArray ValuesToJava(JNIEnv* jni, const StatsReport::Values& values) {
    jobjectArray j_values = jni->NewObjectArray(
        values.size(), *j_value_class_, NULL);
    for (int i = 0; i < values.size(); ++i) {
      ScopedLocalRefFrame local_ref_frame(jni);
      const StatsReport::Value& value = values[i];
      jstring j_name = JavaStringFromStdString(jni, value.name);
      jstring j_value = JavaStringFromStdString(jni, value.value);
      jobject j_element_value =
          jni->NewObject(*j_value_class_, j_value_ctor_, j_name, j_value);
      jni->SetObjectArrayElement(j_values, i, j_element_value);
    }
    return j_values;
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  const ScopedGlobalRef<jobject> j_observer_global_;
  const ScopedGlobalRef<jclass> j_observer_class_;
  const ScopedGlobalRef<jclass> j_stats_report_class_;
  const jmethodID j_stats_report_ctor_;
  const ScopedGlobalRef<jclass> j_value_class_;
  const jmethodID j_value_ctor_;
};

// Adapter presenting a cricket::VideoRenderer as a
// webrtc::VideoRendererInterface.
class VideoRendererWrapper : public VideoRendererInterface {
 public:
  static VideoRendererWrapper* Create(cricket::VideoRenderer* renderer) {
    if (renderer)
      return new VideoRendererWrapper(renderer);
    return NULL;
  }

  virtual ~VideoRendererWrapper() {}

  virtual void SetSize(int width, int height) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(AttachCurrentThreadIfNeeded());
    const bool kNotReserved = false;  // What does this param mean??
    renderer_->SetSize(width, height, kNotReserved);
  }

  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(AttachCurrentThreadIfNeeded());
    renderer_->RenderFrame(frame);
  }

 private:
  explicit VideoRendererWrapper(cricket::VideoRenderer* renderer)
      : renderer_(renderer) {}

  scoped_ptr<cricket::VideoRenderer> renderer_;
};

// Wrapper dispatching webrtc::VideoRendererInterface to a Java VideoRenderer
// instance.
class JavaVideoRendererWrapper : public VideoRendererInterface {
 public:
  JavaVideoRendererWrapper(JNIEnv* jni, jobject j_callbacks)
      : j_callbacks_(jni, j_callbacks),
        j_set_size_id_(GetMethodID(
            jni, GetObjectClass(jni, j_callbacks), "setSize", "(II)V")),
        j_render_frame_id_(GetMethodID(
            jni, GetObjectClass(jni, j_callbacks), "renderFrame",
            "(Lorg/webrtc/VideoRenderer$I420Frame;)V")),
        j_frame_class_(jni,
                       FindClass(jni, "org/webrtc/VideoRenderer$I420Frame")),
        j_frame_ctor_id_(GetMethodID(
            jni, *j_frame_class_, "<init>", "(II[I[Ljava/nio/ByteBuffer;)V")),
        j_byte_buffer_class_(jni, FindClass(jni, "java/nio/ByteBuffer")) {
    CHECK_EXCEPTION(jni, "");
  }

  virtual ~JavaVideoRendererWrapper() {}

  virtual void SetSize(int width, int height) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jni()->CallVoidMethod(*j_callbacks_, j_set_size_id_, width, height);
    CHECK_EXCEPTION(jni(), "");
  }

  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE {
    ScopedLocalRefFrame local_ref_frame(jni());
    jobject j_frame = CricketToJavaFrame(frame);
    jni()->CallVoidMethod(*j_callbacks_, j_render_frame_id_, j_frame);
    CHECK_EXCEPTION(jni(), "");
  }

 private:
  // Return a VideoRenderer.I420Frame referring to the data in |frame|.
  jobject CricketToJavaFrame(const cricket::VideoFrame* frame) {
    jintArray strides = jni()->NewIntArray(3);
    jint* strides_array = jni()->GetIntArrayElements(strides, NULL);
    strides_array[0] = frame->GetYPitch();
    strides_array[1] = frame->GetUPitch();
    strides_array[2] = frame->GetVPitch();
    jni()->ReleaseIntArrayElements(strides, strides_array, 0);
    jobjectArray planes = jni()->NewObjectArray(3, *j_byte_buffer_class_, NULL);
    jobject y_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetYPlane()),
        frame->GetYPitch() * frame->GetHeight());
    jobject u_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetUPlane()), frame->GetChromaSize());
    jobject v_buffer = jni()->NewDirectByteBuffer(
        const_cast<uint8*>(frame->GetVPlane()), frame->GetChromaSize());
    jni()->SetObjectArrayElement(planes, 0, y_buffer);
    jni()->SetObjectArrayElement(planes, 1, u_buffer);
    jni()->SetObjectArrayElement(planes, 2, v_buffer);
    return jni()->NewObject(
        *j_frame_class_, j_frame_ctor_id_,
        frame->GetWidth(), frame->GetHeight(), strides, planes);
  }

  JNIEnv* jni() {
    return AttachCurrentThreadIfNeeded();
  }

  ScopedGlobalRef<jobject> j_callbacks_;
  jmethodID j_set_size_id_;
  jmethodID j_render_frame_id_;
  ScopedGlobalRef<jclass> j_frame_class_;
  jmethodID j_frame_ctor_id_;
  ScopedGlobalRef<jclass> j_byte_buffer_class_;
};

#ifdef ANDROID
// TODO(fischman): consider pulling MediaCodecVideoEncoder out of this file and
// into its own .h/.cc pair, if/when the JNI helper stuff above is extracted
// from this file.

// Arbitrary interval to poll the codec for new outputs.
enum { kMediaCodecPollMs = 10 };

// MediaCodecVideoEncoder is a webrtc::VideoEncoder implementation that uses
// Android's MediaCodec SDK API behind the scenes to implement (hopefully)
// HW-backed video encode.  This C++ class is implemented as a very thin shim,
// delegating all of the interesting work to org.webrtc.MediaCodecVideoEncoder.
// MediaCodecVideoEncoder is created, operated, and destroyed on a single
// thread, currently the libjingle Worker thread.
class MediaCodecVideoEncoder : public webrtc::VideoEncoder,
                               public talk_base::MessageHandler {
 public:
  virtual ~MediaCodecVideoEncoder();
  explicit MediaCodecVideoEncoder(JNIEnv* jni);

  // webrtc::VideoEncoder implementation.  Everything trampolines to
  // |codec_thread_| for execution.
  virtual int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                             int32_t /* number_of_cores */,
                             uint32_t /* max_payload_size */) OVERRIDE;
  virtual int32_t Encode(
      const webrtc::I420VideoFrame& input_image,
      const webrtc::CodecSpecificInfo* /* codec_specific_info */,
      const std::vector<webrtc::VideoFrameType>* frame_types) OVERRIDE;
  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) OVERRIDE;
  virtual int32_t Release() OVERRIDE;
  virtual int32_t SetChannelParameters(uint32_t /* packet_loss */,
                                       int /* rtt */) OVERRIDE;
  virtual int32_t SetRates(uint32_t new_bit_rate, uint32_t frame_rate) OVERRIDE;

  // talk_base::MessageHandler implementation.
  virtual void OnMessage(talk_base::Message* msg) OVERRIDE;

 private:
  // CHECK-fail if not running on |codec_thread_|.
  void CheckOnCodecThread();

  // Release() and InitEncode() in an attempt to restore the codec to an
  // operable state.  Necessary after all manner of OMX-layer errors.
  void ResetCodec();

  // Implementation of webrtc::VideoEncoder methods above, all running on the
  // codec thread exclusively.
  //
  // If width==0 then this is assumed to be a re-initialization and the
  // previously-current values are reused instead of the passed parameters
  // (makes it easier to reason about thread-safety).
  int32_t InitEncodeOnCodecThread(int width, int height, int kbps);
  int32_t EncodeOnCodecThread(
      const webrtc::I420VideoFrame& input_image,
      const std::vector<webrtc::VideoFrameType>* frame_types);
  int32_t RegisterEncodeCompleteCallbackOnCodecThread(
      webrtc::EncodedImageCallback* callback);
  int32_t ReleaseOnCodecThread();
  int32_t SetRatesOnCodecThread(uint32_t new_bit_rate, uint32_t frame_rate);

  // Reset parameters valid between InitEncode() & Release() (see below).
  void ResetParameters(JNIEnv* jni);

  // Helper accessors for MediaCodecVideoEncoder$OutputBufferInfo members.
  int GetOutputBufferInfoIndex(JNIEnv* jni, jobject j_output_buffer_info);
  jobject GetOutputBufferInfoBuffer(JNIEnv* jni, jobject j_output_buffer_info);
  bool GetOutputBufferInfoIsKeyFrame(JNIEnv* jni, jobject j_output_buffer_info);
  jlong GetOutputBufferInfoPresentationTimestampUs(
      JNIEnv* jni,
      jobject j_output_buffer_info);

  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni);

  // Valid all the time since RegisterEncodeCompleteCallback() Invoke()s to
  // |codec_thread_| synchronously.
  webrtc::EncodedImageCallback* callback_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  scoped_ptr<Thread> codec_thread_;  // Thread on which to operate MediaCodec.
  ScopedGlobalRef<jclass> j_media_codec_video_encoder_class_;
  ScopedGlobalRef<jobject> j_media_codec_video_encoder_;
  jmethodID j_init_encode_method_;
  jmethodID j_dequeue_input_buffer_method_;
  jmethodID j_encode_method_;
  jmethodID j_release_method_;
  jmethodID j_set_rates_method_;
  jmethodID j_dequeue_output_buffer_method_;
  jmethodID j_release_output_buffer_method_;
  jfieldID j_info_index_field_;
  jfieldID j_info_buffer_field_;
  jfieldID j_info_is_key_frame_field_;
  jfieldID j_info_presentation_timestamp_us_field_;

  // State that is valid only between InitEncode() and the next Release().
  // Touched only on codec_thread_ so no explicit synchronization necessary.
  int width_;   // Frame width in pixels.
  int height_;  // Frame height in pixels.
  int last_set_bitrate_kbps_;  // Last-requested bitrate in kbps.
  // Frame size in bytes fed to MediaCodec (stride==width, sliceHeight==height).
  int nv12_size_;
  // True only when between a callback_->Encoded() call return a positive value
  // and the next Encode() call being ignored.
  bool drop_next_input_frame_;
  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
};

enum { MSG_SET_RATES, MSG_POLL_FOR_READY_OUTPUTS, };

MediaCodecVideoEncoder::~MediaCodecVideoEncoder() {
  // We depend on ResetParameters() to ensure no more callbacks to us after we
  // are deleted, so assert it here.
  CHECK(width_ == 0, "Release() should have been called");
}

MediaCodecVideoEncoder::MediaCodecVideoEncoder(JNIEnv* jni)
    : callback_(NULL),
      codec_thread_(new Thread()),
      j_media_codec_video_encoder_class_(
          jni,
          FindClass(jni, "org/webrtc/MediaCodecVideoEncoder")),
      j_media_codec_video_encoder_(
          jni,
          jni->NewObject(*j_media_codec_video_encoder_class_,
                         GetMethodID(jni,
                                     *j_media_codec_video_encoder_class_,
                                     "<init>",
                                     "()V"))) {
  ScopedLocalRefFrame local_ref_frame(jni);
  // It would be nice to avoid spinning up a new thread per MediaCodec, and
  // instead re-use e.g. the PeerConnectionFactory's |worker_thread_|, but bug
  // 2732 means that deadlocks abound.  This class synchronously trampolines
  // to |codec_thread_|, so if anything else can be coming to _us_ from
  // |codec_thread_|, or from any thread holding the |_sendCritSect| described
  // in the bug, we have a problem.  For now work around that with a dedicated
  // thread.
  codec_thread_->SetName("MediaCodecVideoEncoder", NULL);
  CHECK(codec_thread_->Start(), "Failed to start MediaCodecVideoEncoder");

  ResetParameters(jni);

  jclass j_output_buffer_info_class =
      FindClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
  j_init_encode_method_ = GetMethodID(jni,
                                      *j_media_codec_video_encoder_class_,
                                      "initEncode",
                                      "(III)[Ljava/nio/ByteBuffer;");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "dequeueInputBuffer", "()I");
  j_encode_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "encode", "(ZIIJ)Z");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_encoder_class_, "release", "()V");
  j_set_rates_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "setRates", "(II)Z");
  j_dequeue_output_buffer_method_ =
      GetMethodID(jni,
                  *j_media_codec_video_encoder_class_,
                  "dequeueOutputBuffer",
                  "()Lorg/webrtc/MediaCodecVideoEncoder$OutputBufferInfo;");
  j_release_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "releaseOutputBuffer", "(I)Z");

  j_info_index_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "index", "I");
  j_info_buffer_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "buffer", "Ljava/nio/ByteBuffer;");
  j_info_is_key_frame_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "isKeyFrame", "Z");
  j_info_presentation_timestamp_us_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "presentationTimestampUs", "J");
  CHECK_EXCEPTION(jni, "MediaCodecVideoEncoder ctor failed");
}

int32_t MediaCodecVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int32_t /* number_of_cores */,
    uint32_t /* max_payload_size */) {
  // Factory should guard against other codecs being used with us.
  CHECK(codec_settings->codecType == kVideoCodecVP8, "Unsupported codec");

  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::InitEncodeOnCodecThread,
           this,
           codec_settings->width,
           codec_settings->height,
           codec_settings->startBitrate));
}

int32_t MediaCodecVideoEncoder::Encode(
    const webrtc::I420VideoFrame& frame,
    const webrtc::CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  return codec_thread_->Invoke<int32_t>(Bind(
      &MediaCodecVideoEncoder::EncodeOnCodecThread, this, frame, frame_types));
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread,
           this,
           callback));
}

int32_t MediaCodecVideoEncoder::Release() {
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoEncoder::SetChannelParameters(uint32_t /* packet_loss */,
                                                     int /* rtt */) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRates(uint32_t new_bit_rate,
                                         uint32_t frame_rate) {
  return codec_thread_->Invoke<int32_t>(
      Bind(&MediaCodecVideoEncoder::SetRatesOnCodecThread,
           this,
           new_bit_rate,
           frame_rate));
}

void MediaCodecVideoEncoder::OnMessage(talk_base::Message* msg) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // We only ever send one message to |this| directly (not through a Bind()'d
  // functor), so expect no ID/data.
  CHECK(!msg->message_id, "Unexpected message!");
  CHECK(!msg->pdata, "Unexpected message!");
  CheckOnCodecThread();

  // It would be nice to recover from a failure here if one happened, but it's
  // unclear how to signal such a failure to the app, so instead we stay silent
  // about it and let the next app-called API method reveal the borkedness.
  DeliverPendingOutputs(jni);
  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
}

void MediaCodecVideoEncoder::CheckOnCodecThread() {
  CHECK(codec_thread_ == ThreadManager::Instance()->CurrentThread(),
        "Running on wrong thread!");
}

void MediaCodecVideoEncoder::ResetCodec() {
  if (Release() != WEBRTC_VIDEO_CODEC_OK ||
      codec_thread_->Invoke<int32_t>(Bind(
          &MediaCodecVideoEncoder::InitEncodeOnCodecThread, this, 0, 0, 0)) !=
          WEBRTC_VIDEO_CODEC_OK) {
    // TODO(fischman): wouldn't it be nice if there was a way to gracefully
    // degrade to a SW encoder at this point?  There isn't one AFAICT :(
    // https://code.google.com/p/webrtc/issues/detail?id=2920
  }
}

int32_t MediaCodecVideoEncoder::InitEncodeOnCodecThread(
    int width, int height, int kbps) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  if (width == 0) {
    width = width_;
    height = height_;
    kbps = last_set_bitrate_kbps_;
  }

  width_ = width;
  height_ = height;
  last_set_bitrate_kbps_ = kbps;
  nv12_size_ = width_ * height_ * 3 / 2;
  // We enforce no extra stride/padding in the format creation step.
  jobjectArray input_buffers = reinterpret_cast<jobjectArray>(
      jni->CallObjectMethod(*j_media_codec_video_encoder_,
                            j_init_encode_method_,
                            width_,
                            height_,
                            kbps));
  CHECK_EXCEPTION(jni, "");
  if (IsNull(jni, input_buffers))
    return WEBRTC_VIDEO_CODEC_ERROR;

  size_t num_input_buffers = jni->GetArrayLength(input_buffers);
  CHECK(input_buffers_.empty(), "Unexpected double InitEncode without Release");
  input_buffers_.resize(num_input_buffers);
  for (size_t i = 0; i < num_input_buffers; ++i) {
    input_buffers_[i] =
        jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
    int64 nv12_buffer_capacity =
        jni->GetDirectBufferCapacity(input_buffers_[i]);
    CHECK_EXCEPTION(jni, "");
    CHECK(nv12_buffer_capacity >= nv12_size_, "Insufficient capacity");
  }
  CHECK_EXCEPTION(jni, "");

  codec_thread_->PostDelayed(kMediaCodecPollMs, this);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::EncodeOnCodecThread(
    const webrtc::I420VideoFrame& frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  if (!DeliverPendingOutputs(jni)) {
    ResetCodec();
    // Continue as if everything's fine.
  }

  if (drop_next_input_frame_) {
    drop_next_input_frame_ = false;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  CHECK(frame_types->size() == 1, "Unexpected stream count");
  bool key_frame = frame_types->front() != webrtc::kDeltaFrame;

  CHECK(frame.width() == width_, "Unexpected resolution change");
  CHECK(frame.height() == height_, "Unexpected resolution change");

  int j_input_buffer_index = jni->CallIntMethod(*j_media_codec_video_encoder_,
                                                j_dequeue_input_buffer_method_);
  CHECK_EXCEPTION(jni, "");
  if (j_input_buffer_index == -1)
    return WEBRTC_VIDEO_CODEC_OK;  // TODO(fischman): see webrtc bug 2887.
  if (j_input_buffer_index == -2) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  jobject j_input_buffer = input_buffers_[j_input_buffer_index];
  uint8* nv12_buffer =
      reinterpret_cast<uint8*>(jni->GetDirectBufferAddress(j_input_buffer));
  CHECK_EXCEPTION(jni, "");
  CHECK(nv12_buffer, "Indirect buffer??");
  CHECK(!libyuv::I420ToNV12(
            frame.buffer(webrtc::kYPlane),
            frame.stride(webrtc::kYPlane),
            frame.buffer(webrtc::kUPlane),
            frame.stride(webrtc::kUPlane),
            frame.buffer(webrtc::kVPlane),
            frame.stride(webrtc::kVPlane),
            nv12_buffer,
            frame.width(),
            nv12_buffer + frame.stride(webrtc::kYPlane) * frame.height(),
            frame.width(),
            frame.width(),
            frame.height()),
        "I420ToNV12 failed");
  jlong timestamp_us = frame.render_time_ms() * 1000;
  int64_t start = talk_base::Time();
  bool encode_status = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                              j_encode_method_,
                                              key_frame,
                                              j_input_buffer_index,
                                              nv12_size_,
                                              timestamp_us);
  CHECK_EXCEPTION(jni, "");
  if (!encode_status || !DeliverPendingOutputs(jni)) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread(
    webrtc::EncodedImageCallback* callback) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::ReleaseOnCodecThread() {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    jni->DeleteGlobalRef(input_buffers_[i]);
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_encoder_, j_release_method_);
  ResetParameters(jni);
  CHECK_EXCEPTION(jni, "");
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRatesOnCodecThread(uint32_t new_bit_rate,
                                                      uint32_t frame_rate) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  last_set_bitrate_kbps_ = new_bit_rate;
  bool ret = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                       j_set_rates_method_,
                                       new_bit_rate,
                                       frame_rate);
  CHECK_EXCEPTION(jni, "");
  if (!ret) {
    ResetCodec();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

void MediaCodecVideoEncoder::ResetParameters(JNIEnv* jni) {
  talk_base::MessageQueueManager::Clear(this);
  width_ = 0;
  height_ = 0;
  nv12_size_ = 0;
  drop_next_input_frame_ = false;
  CHECK(input_buffers_.empty(),
        "ResetParameters called while holding input_buffers_!");
}

int MediaCodecVideoEncoder::GetOutputBufferInfoIndex(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetIntField(jni, j_output_buffer_info, j_info_index_field_);
}

jobject MediaCodecVideoEncoder::GetOutputBufferInfoBuffer(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetObjectField(jni, j_output_buffer_info, j_info_buffer_field_);
}

bool MediaCodecVideoEncoder::GetOutputBufferInfoIsKeyFrame(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetBooleanField(jni, j_output_buffer_info, j_info_is_key_frame_field_);
}

jlong MediaCodecVideoEncoder::GetOutputBufferInfoPresentationTimestampUs(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetLongField(
      jni, j_output_buffer_info, j_info_presentation_timestamp_us_field_);
}

bool MediaCodecVideoEncoder::DeliverPendingOutputs(JNIEnv* jni) {
  while (true) {
    jobject j_output_buffer_info = jni->CallObjectMethod(
        *j_media_codec_video_encoder_, j_dequeue_output_buffer_method_);
    CHECK_EXCEPTION(jni, "");
    if (IsNull(jni, j_output_buffer_info))
      break;

    int output_buffer_index =
        GetOutputBufferInfoIndex(jni, j_output_buffer_info);
    if (output_buffer_index == -1) {
      ResetCodec();
      return false;
    }

    jlong capture_time_ms =
        GetOutputBufferInfoPresentationTimestampUs(jni, j_output_buffer_info) /
        1000;

    int32_t callback_status = 0;
    if (callback_) {
      jobject j_output_buffer =
          GetOutputBufferInfoBuffer(jni, j_output_buffer_info);
      bool key_frame = GetOutputBufferInfoIsKeyFrame(jni, j_output_buffer_info);
      size_t payload_size = jni->GetDirectBufferCapacity(j_output_buffer);
      uint8* payload = reinterpret_cast<uint8_t*>(
          jni->GetDirectBufferAddress(j_output_buffer));
      CHECK_EXCEPTION(jni, "");
      scoped_ptr<webrtc::EncodedImage> image(
          new webrtc::EncodedImage(payload, payload_size, payload_size));
      image->_encodedWidth = width_;
      image->_encodedHeight = height_;
      // Convert capture time to 90 kHz RTP timestamp.
      image->_timeStamp = static_cast<uint32_t>(90 * capture_time_ms);
      image->capture_time_ms_ = capture_time_ms;
      image->_frameType = (key_frame ? webrtc::kKeyFrame : webrtc::kDeltaFrame);
      image->_completeFrame = true;

      webrtc::CodecSpecificInfo info;
      memset(&info, 0, sizeof(info));
      info.codecType = kVideoCodecVP8;
      info.codecSpecific.VP8.pictureId = webrtc::kNoPictureId;
      info.codecSpecific.VP8.tl0PicIdx = webrtc::kNoTl0PicIdx;
      info.codecSpecific.VP8.keyIdx = webrtc::kNoKeyIdx;

      // Generate a header describing a single fragment.
      webrtc::RTPFragmentationHeader header;
      memset(&header, 0, sizeof(header));
      header.VerifyAndAllocateFragmentationHeader(1);
      header.fragmentationOffset[0] = 0;
      header.fragmentationLength[0] = image->_length;
      header.fragmentationPlType[0] = 0;
      header.fragmentationTimeDiff[0] = 0;

      callback_status = callback_->Encoded(*image, &info, &header);
    }

    bool success = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                          j_release_output_buffer_method_,
                                          output_buffer_index);
    CHECK_EXCEPTION(jni, "");
    if (!success) {
      ResetCodec();
      return false;
    }

    if (callback_status > 0)
      drop_next_input_frame_ = true;
    // Theoretically could handle callback_status<0 here, but unclear what that
    // would mean for us.
  }

  return true;
}

// Simplest-possible implementation of an encoder factory, churns out
// MediaCodecVideoEncoders on demand (or errors, if that's not possible).
class MediaCodecVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  MediaCodecVideoEncoderFactory();
  virtual ~MediaCodecVideoEncoderFactory();

  // WebRtcVideoEncoderFactory implementation.
  virtual webrtc::VideoEncoder* CreateVideoEncoder(webrtc::VideoCodecType type)
      OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual const std::vector<VideoCodec>& codecs() const OVERRIDE;
  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) OVERRIDE;

 private:
  // Empty if platform support is lacking, const after ctor returns.
  std::vector<VideoCodec> supported_codecs_;
};

MediaCodecVideoEncoderFactory::MediaCodecVideoEncoderFactory() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_encoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoEncoder");
  bool is_platform_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isPlatformSupported", "()Z"));
  CHECK_EXCEPTION(jni, "");
  if (!is_platform_supported)
    return;

  if (true) {
    // TODO(fischman): re-enable once
    // https://code.google.com/p/webrtc/issues/detail?id=2899 is fixed.  Until
    // then the Android MediaCodec experience is too abysmal to turn on.
    return;
  }

  // Wouldn't it be nice if MediaCodec exposed the maximum capabilities of the
  // encoder?  Sure would be.  Too bad it doesn't.  So we hard-code some
  // reasonable defaults.
  supported_codecs_.push_back(
      VideoCodec(kVideoCodecVP8, "VP8", 1920, 1088, 30));
}

MediaCodecVideoEncoderFactory::~MediaCodecVideoEncoderFactory() {}

webrtc::VideoEncoder* MediaCodecVideoEncoderFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  if (type != kVideoCodecVP8 || supported_codecs_.empty())
    return NULL;
  return new MediaCodecVideoEncoder(AttachCurrentThreadIfNeeded());
}

// Since the available codec list is never going to change, we ignore the
// Observer-related interface here.
void MediaCodecVideoEncoderFactory::AddObserver(Observer* observer) {}
void MediaCodecVideoEncoderFactory::RemoveObserver(Observer* observer) {}

const std::vector<MediaCodecVideoEncoderFactory::VideoCodec>&
MediaCodecVideoEncoderFactory::codecs() const {
  return supported_codecs_;
}

void MediaCodecVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

#endif  // ANDROID

}  // anonymous namespace

// Convenience macro defining JNI-accessible methods in the org.webrtc package.
// Eliminates unnecessary boilerplate and line-wraps, reducing visual clutter.
#define JOW(rettype, name) extern "C" rettype JNIEXPORT JNICALL \
  Java_org_webrtc_##name

extern "C" jint JNIEXPORT JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
  CHECK(!g_jvm, "JNI_OnLoad called more than once!");
  g_jvm = jvm;
  CHECK(g_jvm, "JNI_OnLoad handed NULL?");

  CHECK(talk_base::InitializeSSL(), "Failed to InitializeSSL()");

  JNIEnv* jni;
  if (jvm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6) != JNI_OK)
    return -1;
  g_class_reference_holder = new ClassReferenceHolder(jni);

  return JNI_VERSION_1_6;
}

extern "C" void JNIEXPORT JNICALL JNI_OnUnLoad(JavaVM *jvm, void *reserved) {
  g_class_reference_holder->FreeReferences(AttachCurrentThreadIfNeeded());
  delete g_class_reference_holder;
  g_class_reference_holder = NULL;
  CHECK(talk_base::CleanupSSL(), "Failed to CleanupSSL()");
}

static DataChannelInterface* ExtractNativeDC(JNIEnv* jni, jobject j_dc) {
  jfieldID native_dc_id = GetFieldID(jni,
      GetObjectClass(jni, j_dc), "nativeDataChannel", "J");
  jlong j_d = GetLongField(jni, j_dc, native_dc_id);
  return reinterpret_cast<DataChannelInterface*>(j_d);
}

JOW(jlong, DataChannel_registerObserverNative)(
    JNIEnv* jni, jobject j_dc, jobject j_observer) {
  scoped_ptr<DataChannelObserverWrapper> observer(
      new DataChannelObserverWrapper(jni, j_observer));
  ExtractNativeDC(jni, j_dc)->RegisterObserver(observer.get());
  return jlongFromPointer(observer.release());
}

JOW(void, DataChannel_unregisterObserverNative)(
    JNIEnv* jni, jobject j_dc, jlong native_observer) {
  ExtractNativeDC(jni, j_dc)->UnregisterObserver();
  delete reinterpret_cast<DataChannelObserverWrapper*>(native_observer);
}

JOW(jstring, DataChannel_label)(JNIEnv* jni, jobject j_dc) {
  return JavaStringFromStdString(jni, ExtractNativeDC(jni, j_dc)->label());
}

JOW(jobject, DataChannel_state)(JNIEnv* jni, jobject j_dc) {
  return JavaEnumFromIndex(
      jni, "DataChannel$State", ExtractNativeDC(jni, j_dc)->state());
}

JOW(jlong, DataChannel_bufferedAmount)(JNIEnv* jni, jobject j_dc) {
  uint64 buffered_amount = ExtractNativeDC(jni, j_dc)->buffered_amount();
  CHECK(buffered_amount <= std::numeric_limits<int64>::max(),
        "buffered_amount overflowed jlong!");
  return static_cast<jlong>(buffered_amount);
}

JOW(void, DataChannel_close)(JNIEnv* jni, jobject j_dc) {
  ExtractNativeDC(jni, j_dc)->Close();
}

JOW(jboolean, DataChannel_sendNative)(JNIEnv* jni, jobject j_dc,
                                      jbyteArray data, jboolean binary) {
  jbyte* bytes = jni->GetByteArrayElements(data, NULL);
  bool ret = ExtractNativeDC(jni, j_dc)->Send(DataBuffer(
      talk_base::Buffer(bytes, jni->GetArrayLength(data)),
      binary));
  jni->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
  return ret;
}

JOW(void, DataChannel_dispose)(JNIEnv* jni, jobject j_dc) {
  CHECK_RELEASE(ExtractNativeDC(jni, j_dc));
}

JOW(void, Logging_nativeEnableTracing)(
    JNIEnv* jni, jclass, jstring j_path, jint nativeLevels,
    jint nativeSeverity) {
  std::string path = JavaToStdString(jni, j_path);
  if (nativeLevels != webrtc::kTraceNone) {
    webrtc::Trace::set_level_filter(nativeLevels);
#ifdef ANDROID
    if (path != "logcat:") {
#endif
      CHECK(webrtc::Trace::SetTraceFile(path.c_str(), false) == 0,
            "SetTraceFile failed");
#ifdef ANDROID
    } else {
      // Intentionally leak this to avoid needing to reason about its lifecycle.
      // It keeps no state and functions only as a dispatch point.
      static LogcatTraceContext* g_trace_callback = new LogcatTraceContext();
    }
#endif
  }
  talk_base::LogMessage::LogToDebug(nativeSeverity);
}

JOW(void, PeerConnection_freePeerConnection)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(void, PeerConnection_freeObserver)(JNIEnv*, jclass, jlong j_p) {
  PCOJava* p = reinterpret_cast<PCOJava*>(j_p);
  delete p;
}

JOW(void, MediaSource_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaSourceInterface*>(j_p));
}

JOW(void, VideoCapturer_free)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<cricket::VideoCapturer*>(j_p);
}

JOW(void, VideoRenderer_free)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<VideoRendererWrapper*>(j_p);
}

JOW(void, MediaStreamTrack_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamTrackInterface*>(j_p));
}

JOW(jboolean, MediaStream_nativeAddAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->AddTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeAddVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)
      ->AddTrack(reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveAudioTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_audio_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<AudioTrackInterface*>(j_audio_track_pointer));
}

JOW(jboolean, MediaStream_nativeRemoveVideoTrack)(
    JNIEnv* jni, jclass, jlong pointer, jlong j_video_track_pointer) {
  return reinterpret_cast<MediaStreamInterface*>(pointer)->RemoveTrack(
      reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer));
}

JOW(jstring, MediaStream_nativeLabel)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamInterface*>(j_p)->label());
}

JOW(void, MediaStream_free)(JNIEnv*, jclass, jlong j_p) {
  CHECK_RELEASE(reinterpret_cast<MediaStreamInterface*>(j_p));
}

JOW(jlong, PeerConnectionFactory_nativeCreateObserver)(
    JNIEnv * jni, jclass, jobject j_observer) {
  return (jlong)new PCOJava(jni, j_observer);
}

#ifdef ANDROID
JOW(jboolean, PeerConnectionFactory_initializeAndroidGlobals)(
    JNIEnv* jni, jclass, jobject context) {
  CHECK(g_jvm, "JNI_OnLoad failed to run?");
  bool failure = false;
  failure |= webrtc::VideoEngine::SetAndroidObjects(g_jvm);
  failure |= webrtc::VoiceEngine::SetAndroidObjects(g_jvm, jni, context);
  return !failure;
}
#endif  // ANDROID

// Helper struct for working around the fact that CreatePeerConnectionFactory()
// comes in two flavors: either entirely automagical (constructing its own
// threads and deleting them on teardown, but no external codec factory support)
// or entirely manual (requires caller to delete threads after factory
// teardown).  This struct takes ownership of its ctor's arguments to present a
// single thing for Java to hold and eventually free.
class OwnedFactoryAndThreads {
 public:
  OwnedFactoryAndThreads(Thread* worker_thread,
                         Thread* signaling_thread,
                         PeerConnectionFactoryInterface* factory)
      : worker_thread_(worker_thread),
        signaling_thread_(signaling_thread),
        factory_(factory) {}

  ~OwnedFactoryAndThreads() { CHECK_RELEASE(factory_); }

  PeerConnectionFactoryInterface* factory() { return factory_; }

 private:
  const scoped_ptr<Thread> worker_thread_;
  const scoped_ptr<Thread> signaling_thread_;
  PeerConnectionFactoryInterface* factory_;  // Const after ctor except dtor.
};

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnectionFactory)(
    JNIEnv* jni, jclass) {
  webrtc::Trace::CreateTrace();
  Thread* worker_thread = new Thread();
  worker_thread->SetName("worker_thread", NULL);
  Thread* signaling_thread = new Thread();
  signaling_thread->SetName("signaling_thread", NULL);
  CHECK(worker_thread->Start() && signaling_thread->Start(),
        "Failed to start threads");
  scoped_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;
#ifdef ANDROID
  encoder_factory.reset(new MediaCodecVideoEncoderFactory());
#endif
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      webrtc::CreatePeerConnectionFactory(worker_thread,
                                          signaling_thread,
                                          NULL,
                                          encoder_factory.release(),
                                          NULL));
  OwnedFactoryAndThreads* owned_factory = new OwnedFactoryAndThreads(
      worker_thread, signaling_thread, factory.release());
  return jlongFromPointer(owned_factory);
}

JOW(void, PeerConnectionFactory_freeFactory)(JNIEnv*, jclass, jlong j_p) {
  delete reinterpret_cast<OwnedFactoryAndThreads*>(j_p);
  webrtc::Trace::ReturnTrace();
}

static PeerConnectionFactoryInterface* factoryFromJava(jlong j_p) {
  return reinterpret_cast<OwnedFactoryAndThreads*>(j_p)->factory();
}

JOW(jlong, PeerConnectionFactory_nativeCreateLocalMediaStream)(
    JNIEnv* jni, jclass, jlong native_factory, jstring label) {
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  talk_base::scoped_refptr<MediaStreamInterface> stream(
      factory->CreateLocalMediaStream(JavaToStdString(jni, label)));
  return (jlong)stream.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoSource)(
    JNIEnv* jni, jclass, jlong native_factory, jlong native_capturer,
    jobject j_constraints) {
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  talk_base::scoped_refptr<VideoSourceInterface> source(
      factory->CreateVideoSource(
          reinterpret_cast<cricket::VideoCapturer*>(native_capturer),
          constraints.get()));
  return (jlong)source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateVideoTrack)(
    JNIEnv* jni, jclass, jlong native_factory, jstring id,
    jlong native_source) {
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  talk_base::scoped_refptr<VideoTrackInterface> track(
      factory->CreateVideoTrack(
          JavaToStdString(jni, id),
          reinterpret_cast<VideoSourceInterface*>(native_source)));
  return (jlong)track.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioSource)(
    JNIEnv* jni, jclass, jlong native_factory, jobject j_constraints) {
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  talk_base::scoped_refptr<AudioSourceInterface> source(
      factory->CreateAudioSource(constraints.get()));
  return (jlong)source.release();
}

JOW(jlong, PeerConnectionFactory_nativeCreateAudioTrack)(
    JNIEnv* jni, jclass, jlong native_factory, jstring id,
    jlong native_source) {
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> factory(
      factoryFromJava(native_factory));
  talk_base::scoped_refptr<AudioTrackInterface> track(factory->CreateAudioTrack(
      JavaToStdString(jni, id),
      reinterpret_cast<AudioSourceInterface*>(native_source)));
  return (jlong)track.release();
}

static void JavaIceServersToJsepIceServers(
    JNIEnv* jni, jobject j_ice_servers,
    PeerConnectionInterface::IceServers* ice_servers) {
  jclass list_class = GetObjectClass(jni, j_ice_servers);
  jmethodID iterator_id = GetMethodID(
      jni, list_class, "iterator", "()Ljava/util/Iterator;");
  jobject iterator = jni->CallObjectMethod(j_ice_servers, iterator_id);
  CHECK_EXCEPTION(jni, "error during CallObjectMethod");
  jmethodID iterator_has_next = GetMethodID(
      jni, GetObjectClass(jni, iterator), "hasNext", "()Z");
  jmethodID iterator_next = GetMethodID(
      jni, GetObjectClass(jni, iterator), "next", "()Ljava/lang/Object;");
  while (jni->CallBooleanMethod(iterator, iterator_has_next)) {
    CHECK_EXCEPTION(jni, "error during CallBooleanMethod");
    jobject j_ice_server = jni->CallObjectMethod(iterator, iterator_next);
    CHECK_EXCEPTION(jni, "error during CallObjectMethod");
    jclass j_ice_server_class = GetObjectClass(jni, j_ice_server);
    jfieldID j_ice_server_uri_id =
        GetFieldID(jni, j_ice_server_class, "uri", "Ljava/lang/String;");
    jfieldID j_ice_server_username_id =
        GetFieldID(jni, j_ice_server_class, "username", "Ljava/lang/String;");
    jfieldID j_ice_server_password_id =
        GetFieldID(jni, j_ice_server_class, "password", "Ljava/lang/String;");
    jstring uri = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_uri_id));
    jstring username = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_username_id));
    jstring password = reinterpret_cast<jstring>(
        GetObjectField(jni, j_ice_server, j_ice_server_password_id));
    PeerConnectionInterface::IceServer server;
    server.uri = JavaToStdString(jni, uri);
    server.username = JavaToStdString(jni, username);
    server.password = JavaToStdString(jni, password);
    ice_servers->push_back(server);
  }
  CHECK_EXCEPTION(jni, "error during CallBooleanMethod");
}

JOW(jlong, PeerConnectionFactory_nativeCreatePeerConnection)(
    JNIEnv *jni, jclass, jlong factory, jobject j_ice_servers,
    jobject j_constraints, jlong observer_p) {
  talk_base::scoped_refptr<PeerConnectionFactoryInterface> f(
      reinterpret_cast<PeerConnectionFactoryInterface*>(
          factoryFromJava(factory)));
  PeerConnectionInterface::IceServers servers;
  JavaIceServersToJsepIceServers(jni, j_ice_servers, &servers);
  PCOJava* observer = reinterpret_cast<PCOJava*>(observer_p);
  observer->SetConstraints(new ConstraintsWrapper(jni, j_constraints));
  talk_base::scoped_refptr<PeerConnectionInterface> pc(f->CreatePeerConnection(
      servers, observer->constraints(), NULL, observer));
  return (jlong)pc.release();
}

static talk_base::scoped_refptr<PeerConnectionInterface> ExtractNativePC(
    JNIEnv* jni, jobject j_pc) {
  jfieldID native_pc_id = GetFieldID(jni,
      GetObjectClass(jni, j_pc), "nativePeerConnection", "J");
  jlong j_p = GetLongField(jni, j_pc, native_pc_id);
  return talk_base::scoped_refptr<PeerConnectionInterface>(
      reinterpret_cast<PeerConnectionInterface*>(j_p));
}

JOW(jobject, PeerConnection_getLocalDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->local_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_getRemoteDescription)(JNIEnv* jni, jobject j_pc) {
  const SessionDescriptionInterface* sdp =
      ExtractNativePC(jni, j_pc)->remote_description();
  return sdp ? JavaSdpFromNativeSdp(jni, sdp) : NULL;
}

JOW(jobject, PeerConnection_createDataChannel)(
    JNIEnv* jni, jobject j_pc, jstring j_label, jobject j_init) {
  DataChannelInit init = JavaDataChannelInitToNative(jni, j_init);
  talk_base::scoped_refptr<DataChannelInterface> channel(
      ExtractNativePC(jni, j_pc)->CreateDataChannel(
          JavaToStdString(jni, j_label), &init));
  // Mustn't pass channel.get() directly through NewObject to avoid reading its
  // vararg parameter as 64-bit and reading memory that doesn't belong to the
  // 32-bit parameter.
  jlong nativeChannelPtr = jlongFromPointer(channel.get());
  CHECK(nativeChannelPtr, "Failed to create DataChannel");
  jclass j_data_channel_class = FindClass(jni, "org/webrtc/DataChannel");
  jmethodID j_data_channel_ctor = GetMethodID(
      jni, j_data_channel_class, "<init>", "(J)V");
  jobject j_channel = jni->NewObject(
      j_data_channel_class, j_data_channel_ctor, nativeChannelPtr);
  CHECK_EXCEPTION(jni, "error during NewObject");
  // Channel is now owned by Java object, and will be freed from there.
  int bumped_count = channel->AddRef();
  CHECK(bumped_count == 2, "Unexpected refcount");
  return j_channel;
}

JOW(void, PeerConnection_createOffer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  talk_base::scoped_refptr<CreateSdpObserverWrapper> observer(
      new talk_base::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateOffer(observer, constraints);
}

JOW(void, PeerConnection_createAnswer)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jobject j_constraints) {
  ConstraintsWrapper* constraints =
      new ConstraintsWrapper(jni, j_constraints);
  talk_base::scoped_refptr<CreateSdpObserverWrapper> observer(
      new talk_base::RefCountedObject<CreateSdpObserverWrapper>(
          jni, j_observer, constraints));
  ExtractNativePC(jni, j_pc)->CreateAnswer(observer, constraints);
}

// Helper to create a SessionDescriptionInterface from a SessionDescription.
static SessionDescriptionInterface* JavaSdpToNativeSdp(
    JNIEnv* jni, jobject j_sdp) {
  jfieldID j_type_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "type",
      "Lorg/webrtc/SessionDescription$Type;");
  jobject j_type = GetObjectField(jni, j_sdp, j_type_id);
  jmethodID j_canonical_form_id = GetMethodID(
      jni, GetObjectClass(jni, j_type), "canonicalForm",
      "()Ljava/lang/String;");
  jstring j_type_string = (jstring)jni->CallObjectMethod(
      j_type, j_canonical_form_id);
  CHECK_EXCEPTION(jni, "error during CallObjectMethod");
  std::string std_type = JavaToStdString(jni, j_type_string);

  jfieldID j_description_id = GetFieldID(
      jni, GetObjectClass(jni, j_sdp), "description", "Ljava/lang/String;");
  jstring j_description = (jstring)GetObjectField(jni, j_sdp, j_description_id);
  std::string std_description = JavaToStdString(jni, j_description);

  return webrtc::CreateSessionDescription(
      std_type, std_description, NULL);
}

JOW(void, PeerConnection_setLocalDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  talk_base::scoped_refptr<SetSdpObserverWrapper> observer(
      new talk_base::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetLocalDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(void, PeerConnection_setRemoteDescription)(
    JNIEnv* jni, jobject j_pc,
    jobject j_observer, jobject j_sdp) {
  talk_base::scoped_refptr<SetSdpObserverWrapper> observer(
      new talk_base::RefCountedObject<SetSdpObserverWrapper>(
          jni, j_observer, reinterpret_cast<ConstraintsWrapper*>(NULL)));
  ExtractNativePC(jni, j_pc)->SetRemoteDescription(
      observer, JavaSdpToNativeSdp(jni, j_sdp));
}

JOW(jboolean, PeerConnection_updateIce)(
    JNIEnv* jni, jobject j_pc, jobject j_ice_servers, jobject j_constraints) {
  PeerConnectionInterface::IceServers ice_servers;
  JavaIceServersToJsepIceServers(jni, j_ice_servers, &ice_servers);
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  return ExtractNativePC(jni, j_pc)->UpdateIce(ice_servers, constraints.get());
}

JOW(jboolean, PeerConnection_nativeAddIceCandidate)(
    JNIEnv* jni, jobject j_pc, jstring j_sdp_mid,
    jint j_sdp_mline_index, jstring j_candidate_sdp) {
  std::string sdp_mid = JavaToStdString(jni, j_sdp_mid);
  std::string sdp = JavaToStdString(jni, j_candidate_sdp);
  scoped_ptr<IceCandidateInterface> candidate(
      webrtc::CreateIceCandidate(sdp_mid, j_sdp_mline_index, sdp, NULL));
  return ExtractNativePC(jni, j_pc)->AddIceCandidate(candidate.get());
}

JOW(jboolean, PeerConnection_nativeAddLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream, jobject j_constraints) {
  scoped_ptr<ConstraintsWrapper> constraints(
      new ConstraintsWrapper(jni, j_constraints));
  return ExtractNativePC(jni, j_pc)->AddStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream),
      constraints.get());
}

JOW(void, PeerConnection_nativeRemoveLocalStream)(
    JNIEnv* jni, jobject j_pc, jlong native_stream) {
  ExtractNativePC(jni, j_pc)->RemoveStream(
      reinterpret_cast<MediaStreamInterface*>(native_stream));
}

JOW(bool, PeerConnection_nativeGetStats)(
    JNIEnv* jni, jobject j_pc, jobject j_observer, jlong native_track) {
  talk_base::scoped_refptr<StatsObserverWrapper> observer(
      new talk_base::RefCountedObject<StatsObserverWrapper>(jni, j_observer));
  return ExtractNativePC(jni, j_pc)->GetStats(
      observer, reinterpret_cast<MediaStreamTrackInterface*>(native_track));
}

JOW(jobject, PeerConnection_signalingState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::SignalingState state =
      ExtractNativePC(jni, j_pc)->signaling_state();
  return JavaEnumFromIndex(jni, "PeerConnection$SignalingState", state);
}

JOW(jobject, PeerConnection_iceConnectionState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceConnectionState state =
      ExtractNativePC(jni, j_pc)->ice_connection_state();
  return JavaEnumFromIndex(jni, "PeerConnection$IceConnectionState", state);
}

JOW(jobject, PeerGathering_iceGatheringState)(JNIEnv* jni, jobject j_pc) {
  PeerConnectionInterface::IceGatheringState state =
      ExtractNativePC(jni, j_pc)->ice_gathering_state();
  return JavaEnumFromIndex(jni, "PeerGathering$IceGatheringState", state);
}

JOW(void, PeerConnection_close)(JNIEnv* jni, jobject j_pc) {
  ExtractNativePC(jni, j_pc)->Close();
  return;
}

JOW(jobject, MediaSource_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  talk_base::scoped_refptr<MediaSourceInterface> p(
      reinterpret_cast<MediaSourceInterface*>(j_p));
  return JavaEnumFromIndex(jni, "MediaSource$State", p->state());
}

JOW(jlong, VideoCapturer_nativeCreateVideoCapturer)(
    JNIEnv* jni, jclass, jstring j_device_name) {
  std::string device_name = JavaToStdString(jni, j_device_name);
  scoped_ptr<cricket::DeviceManagerInterface> device_manager(
      cricket::DeviceManagerFactory::Create());
  CHECK(device_manager->Init(), "DeviceManager::Init() failed");
  cricket::Device device;
  if (!device_manager->GetVideoCaptureDevice(device_name, &device)) {
    LOG(LS_ERROR) << "GetVideoCaptureDevice failed for " << device_name;
    return 0;
  }
  scoped_ptr<cricket::VideoCapturer> capturer(
      device_manager->CreateVideoCapturer(device));
  return (jlong)capturer.release();
}

JOW(jlong, VideoRenderer_nativeCreateGuiVideoRenderer)(
    JNIEnv* jni, jclass, int x, int y) {
  scoped_ptr<VideoRendererWrapper> renderer(VideoRendererWrapper::Create(
      cricket::VideoRendererFactory::CreateGuiVideoRenderer(x, y)));
  return (jlong)renderer.release();
}

JOW(jlong, VideoRenderer_nativeWrapVideoRenderer)(
    JNIEnv* jni, jclass, jobject j_callbacks) {
  scoped_ptr<JavaVideoRendererWrapper> renderer(
      new JavaVideoRendererWrapper(jni, j_callbacks));
  return (jlong)renderer.release();
}

JOW(jlong, VideoSource_stop)(JNIEnv* jni, jclass, jlong j_p) {
  cricket::VideoCapturer* capturer =
      reinterpret_cast<VideoSourceInterface*>(j_p)->GetVideoCapturer();
  scoped_ptr<cricket::VideoFormatPod> format(
      new cricket::VideoFormatPod(*capturer->GetCaptureFormat()));
  capturer->Stop();
  return jlongFromPointer(format.release());
}

JOW(void, VideoSource_restart)(
    JNIEnv* jni, jclass, jlong j_p_source, jlong j_p_format) {
  CHECK(j_p_source, "");
  CHECK(j_p_format, "");
  scoped_ptr<cricket::VideoFormatPod> format(
      reinterpret_cast<cricket::VideoFormatPod*>(j_p_format));
  reinterpret_cast<VideoSourceInterface*>(j_p_source)->GetVideoCapturer()->
      StartCapturing(cricket::VideoFormat(*format));
}

JOW(void, VideoSource_freeNativeVideoFormat)(
    JNIEnv* jni, jclass, jlong j_p) {
  delete reinterpret_cast<cricket::VideoFormatPod*>(j_p);
}

JOW(jstring, MediaStreamTrack_nativeId)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->id());
}

JOW(jstring, MediaStreamTrack_nativeKind)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaStringFromStdString(
      jni, reinterpret_cast<MediaStreamTrackInterface*>(j_p)->kind());
}

JOW(jboolean, MediaStreamTrack_nativeEnabled)(JNIEnv* jni, jclass, jlong j_p) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)->enabled();
}

JOW(jobject, MediaStreamTrack_nativeState)(JNIEnv* jni, jclass, jlong j_p) {
  return JavaEnumFromIndex(
      jni,
      "MediaStreamTrack$State",
      reinterpret_cast<MediaStreamTrackInterface*>(j_p)->state());
}

JOW(jboolean, MediaStreamTrack_nativeSetState)(
    JNIEnv* jni, jclass, jlong j_p, jint j_new_state) {
  MediaStreamTrackInterface::TrackState new_state =
      (MediaStreamTrackInterface::TrackState)j_new_state;
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)
      ->set_state(new_state);
}

JOW(jboolean, MediaStreamTrack_nativeSetEnabled)(
    JNIEnv* jni, jclass, jlong j_p, jboolean enabled) {
  return reinterpret_cast<MediaStreamTrackInterface*>(j_p)
      ->set_enabled(enabled);
}

JOW(void, VideoTrack_nativeAddRenderer)(
    JNIEnv* jni, jclass,
    jlong j_video_track_pointer, jlong j_renderer_pointer) {
  reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer)->AddRenderer(
      reinterpret_cast<VideoRendererInterface*>(j_renderer_pointer));
}

JOW(void, VideoTrack_nativeRemoveRenderer)(
    JNIEnv* jni, jclass,
    jlong j_video_track_pointer, jlong j_renderer_pointer) {
  reinterpret_cast<VideoTrackInterface*>(j_video_track_pointer)->RemoveRenderer(
      reinterpret_cast<VideoRendererInterface*>(j_renderer_pointer));
}
