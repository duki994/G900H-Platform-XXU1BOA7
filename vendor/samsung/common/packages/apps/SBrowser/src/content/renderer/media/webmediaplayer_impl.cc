// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_impl.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "cc/layers/video_layer.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/buffered_data_source.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/render_media_log.h"
#include "content/renderer/media/texttrack_impl.h"
#include "content/renderer/media/webaudiosourceprovider_impl.h"
#include "content/renderer/media/webcontentdecryptionmodule_impl.h"
#include "content/renderer/media/webinbandtexttrack_impl.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/media/webmediaplayer_params.h"
#include "content/renderer/media/webmediaplayer_util.h"
#include "content/renderer/media/webmediasource_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/text_renderer.h"
#include "media/base/video_frame.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/filters/opus_audio_decoder.h"
#include "media/filters/video_renderer_impl.h"
#include "media/filters/vpx_video_decoder.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

using blink::WebCanvas;
using blink::WebMediaPlayer;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using media::PipelineStatus;

namespace {

// Amount of extra memory used by each player instance reported to V8.
// It is not exact number -- first, it differs on different platforms,
// and second, it is very hard to calculate. Instead, use some arbitrary
// value that will cause garbage collection from time to time. We don't want
// it to happen on every allocation, but don't want 5k players to sit in memory
// either. Looks that chosen constant achieves both goals, at least for audio
// objects. (Do not worry about video objects yet, JS programs do not create
// thousands of them...)
const int kPlayerExtraMemory = 1024 * 1024;

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const double kMinRate = 0.0625;
const double kMaxRate = 16.0;

// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";

}  // namespace

namespace content {

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::CORSMode ## name) == \
                 static_cast<int>(BufferedResourceLoader::k ## name), \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(Unspecified);
COMPILE_ASSERT_MATCHING_ENUM(Anonymous);
COMPILE_ASSERT_MATCHING_ENUM(UseCredentials);
#undef COMPILE_ASSERT_MATCHING_ENUM

#define BIND_TO_RENDER_LOOP(function) \
  (DCHECK(main_loop_->BelongsToCurrentThread()), \
  media::BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

static void LogMediaSourceError(const scoped_refptr<media::MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

WebMediaPlayerImpl::WebMediaPlayerImpl(
    blink::WebFrame* frame,
    blink::WebMediaPlayerClient* client,
    base::WeakPtr<WebMediaPlayerDelegate> delegate,
    const WebMediaPlayerParams& params)
    : RenderFrameObserver(RenderFrame::FromWebFrame(frame)),
      frame_(frame),
      network_state_(WebMediaPlayer::NetworkStateEmpty),
      ready_state_(WebMediaPlayer::ReadyStateHaveNothing),
      main_loop_(base::MessageLoopProxy::current()),
      media_loop_(
          RenderThreadImpl::current()->GetMediaThreadMessageLoopProxy()),
      paused_(true),
      seeking_(false),
      playback_rate_(0.0f),
      pending_seek_(false),
      pending_seek_seconds_(0.0f),
      client_(client),
      delegate_(delegate),
      defer_load_cb_(params.defer_load_cb()),
      media_log_(new RenderMediaLog()),
      accelerated_compositing_reported_(false),
      incremented_externally_allocated_memory_(false),
      gpu_factories_(RenderThreadImpl::current()->GetGpuFactories()),
      is_local_source_(false),
      supports_save_(true),
      starting_(false),
      chunk_demuxer_(NULL),
      painter_(
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::InvalidateOnMainThread),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnNaturalSizeChange)),
      video_frame_provider_client_(NULL),
      text_track_index_(0),
      web_cdm_(NULL),
      destroy_reason_(0u) {
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  pipeline_.reset(new media::Pipeline(media_loop_, media_log_.get()));

  // |gpu_factories_| requires that its entry points be called on its
  // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
  // factories, require that their message loops are identical.
  DCHECK(!gpu_factories_ || (gpu_factories_->GetTaskRunner() == media_loop_));

  // Let V8 know we started new thread if we did not do it yet.
  // Made separate task to avoid deletion of player currently being created.
  // Also, delaying GC until after player starts gets rid of starting lag --
  // collection happens in parallel with playing.
  //
  // TODO(enal): remove when we get rid of per-audio-stream thread.
  main_loop_->PostTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerImpl::IncrementExternallyAllocatedMemory,
                 AsWeakPtr()));

  // Use the null sink if no sink was provided.
  audio_source_provider_ = new WebAudioSourceProviderImpl(
      params.audio_renderer_sink().get()
          ? params.audio_renderer_sink()
          : new media::NullAudioSink(media_loop_));
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  SetVideoFrameProviderClient(NULL);
  client_->setWebLayer(NULL);

  DCHECK(main_loop_->BelongsToCurrentThread());
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  if (delegate_.get())
    delegate_->PlayerGone(this);

  Destroy(WEBMEDIAPLAYER_DESTROYED);
}

namespace {

// Helper enum for reporting scheme histograms.
enum URLSchemeForHistogram {
  kUnknownURLScheme,
  kMissingURLScheme,
  kHttpURLScheme,
  kHttpsURLScheme,
  kFtpURLScheme,
  kChromeExtensionURLScheme,
  kJavascriptURLScheme,
  kFileURLScheme,
  kBlobURLScheme,
  kDataURLScheme,
  kFileSystemScheme,
  kMaxURLScheme = kFileSystemScheme  // Must be equal to highest enum value.
};

URLSchemeForHistogram URLScheme(const GURL& url) {
  if (!url.has_scheme()) return kMissingURLScheme;
  if (url.SchemeIs("http")) return kHttpURLScheme;
  if (url.SchemeIs("https")) return kHttpsURLScheme;
  if (url.SchemeIs("ftp")) return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension")) return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript")) return kJavascriptURLScheme;
  if (url.SchemeIs("file")) return kFileURLScheme;
  if (url.SchemeIs("blob")) return kBlobURLScheme;
  if (url.SchemeIs("data")) return kDataURLScheme;
  if (url.SchemeIs("filesystem")) return kFileSystemScheme;
  return kUnknownURLScheme;
}

}  // namespace

void WebMediaPlayerImpl::load(LoadType load_type, const blink::WebURL& url,
                              CORSMode cors_mode) {
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(
        &WebMediaPlayerImpl::DoLoad, AsWeakPtr(), load_type, url, cors_mode));
    return;
  }
  DoLoad(load_type, url, cors_mode);
}

void WebMediaPlayerImpl::DoLoad(LoadType load_type,
                                const blink::WebURL& url,
                                CORSMode cors_mode) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  GURL gurl(url);
  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(gurl), kMaxURLScheme);

  // Set subresource URL for crash reporting.
  base::debug::SetCrashKeyValue("subresource_url", gurl.spec());

  load_type_ = load_type;

  // Handle any volume/preload changes that occurred before load().
  setVolume(client_->volume());
  setPreload(client_->preload());

  SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  SetReadyState(WebMediaPlayer::ReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  // Media source pipelines can start immediately.
  if (load_type == LoadTypeMediaSource) {
    supports_save_ = false;
    StartPipeline();
    return;
  }

  // Otherwise it's a regular request which requires resolving the URL first.
  data_source_.reset(new BufferedDataSource(
      main_loop_,
      frame_,
      media_log_.get(),
      base::Bind(&WebMediaPlayerImpl::NotifyDownloading, AsWeakPtr())));
  data_source_->Initialize(
      url, static_cast<BufferedResourceLoader::CORSMode>(cors_mode),
      base::Bind(
          &WebMediaPlayerImpl::DataSourceInitialized,
          AsWeakPtr(), gurl));

  is_local_source_ = !gurl.SchemeIsHTTPOrHTTPS();
}

void WebMediaPlayerImpl::play() {
  DCHECK(main_loop_->BelongsToCurrentThread());

  paused_ = false;
  pipeline_->SetPlaybackRate(playback_rate_);
  if (data_source_)
    data_source_->MediaIsPlaying();

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PLAY));

  if (delegate_.get())
    delegate_->DidPlay(this);
}

void WebMediaPlayerImpl::pause() {
  DCHECK(main_loop_->BelongsToCurrentThread());

  paused_ = true;
  pipeline_->SetPlaybackRate(0.0f);
  if (data_source_)
    data_source_->MediaIsPaused();
  paused_time_ = pipeline_->GetMediaTime();

  media_log_->AddEvent(media_log_->CreateEvent(media::MediaLogEvent::PAUSE));

  if (delegate_.get())
    delegate_->DidPause(this);
}

bool WebMediaPlayerImpl::supportsSave() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return supports_save_;
}

void WebMediaPlayerImpl::seek(double seconds) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ > WebMediaPlayer::ReadyStateHaveMetadata)
    SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  if (starting_ || seeking_) {
    pending_seek_ = true;
    pending_seek_seconds_ = seconds;
    if (chunk_demuxer_)
      chunk_demuxer_->CancelPendingSeek(seek_time);
    return;
  }

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

  // Update our paused time.
  if (paused_)
    paused_time_ = seek_time;

  seeking_ = true;

  if (chunk_demuxer_)
    chunk_demuxer_->StartWaitingForSeek(seek_time);

  // Kick off the asynchronous seek!
  pipeline_->Seek(
      seek_time,
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek));
}

void WebMediaPlayerImpl::setRate(double rate) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0)
    return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  playback_rate_ = rate;
  if (!paused_) {
    pipeline_->SetPlaybackRate(rate);
    if (data_source_)
      data_source_->MediaPlaybackRateChanged(rate);
  }
}

void WebMediaPlayerImpl::setVolume(double volume) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  pipeline_->SetVolume(volume);
}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, chromium_name) \
    COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::webkit_name) == \
                   static_cast<int>(content::chromium_name), \
                   mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(PreloadNone, NONE);
COMPILE_ASSERT_MATCHING_ENUM(PreloadMetaData, METADATA);
COMPILE_ASSERT_MATCHING_ENUM(PreloadAuto, AUTO);
#undef COMPILE_ASSERT_MATCHING_ENUM

void WebMediaPlayerImpl::setPreload(WebMediaPlayer::Preload preload) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (data_source_)
    data_source_->SetPreload(static_cast<content::Preload>(preload));
}

bool WebMediaPlayerImpl::hasVideo() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_->HasVideo();
}

bool WebMediaPlayerImpl::hasAudio() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_->HasAudio();
}

blink::WebSize WebMediaPlayerImpl::naturalSize() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return blink::WebSize(natural_size_);
}

bool WebMediaPlayerImpl::paused() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  return pipeline_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::seeking() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return false;

  return seeking_;
}

double WebMediaPlayerImpl::duration() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  return GetPipelineDuration();
}

double WebMediaPlayerImpl::currentTime() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return (paused_ ? paused_time_ : pipeline_->GetMediaTime()).InSecondsF();
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::networkState() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::readyState() const {
  DCHECK(main_loop_->BelongsToCurrentThread());
  return ready_state_;
}

const blink::WebTimeRanges& WebMediaPlayerImpl::buffered() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  blink::WebTimeRanges web_ranges(
      ConvertToWebTimeRanges(pipeline_->GetBufferedTimeRanges()));
  buffered_.swap(web_ranges);
  return buffered_;
}

double WebMediaPlayerImpl::maxTimeSeekable() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // If we haven't even gotten to ReadyStateHaveMetadata yet then just
  // return 0 so that the seekable range is empty.
  if (ready_state_ < WebMediaPlayer::ReadyStateHaveMetadata)
    return 0.0;

  // We don't support seeking in streaming media.
  if (data_source_ && data_source_->IsStreaming())
    return 0.0;
  return duration();
}

bool WebMediaPlayerImpl::didLoadingProgress() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // TODO(scherkus): Remove after tracking down cause for crashes
  // http://crbug.com/341184 http://crbug.com/341186
  uint32 reason = this->destroy_reason_;
  base::debug::Alias(&reason);
  CHECK(pipeline_);

  return pipeline_->DidLoadingProgress();
}

void WebMediaPlayerImpl::paint(WebCanvas* canvas,
                               const WebRect& rect,
                               unsigned char alpha) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (!accelerated_compositing_reported_) {
    accelerated_compositing_reported_ = true;
    // Normally paint() is only called in non-accelerated rendering, but there
    // are exceptions such as webgl where compositing is used in the WebView but
    // video frames are still rendered to a canvas.
    UMA_HISTOGRAM_BOOLEAN(
        "Media.AcceleratedCompositingActive",
        frame_->view()->isAcceleratedCompositingActive());
  }


  TRACE_EVENT0("media", "WebMediaPlayerImpl:paint");
  scoped_refptr<media::VideoFrame> video_frame = painter_.GetCurrentFrame(true);
  gfx::Rect gfx_rect(rect);
  skcanvas_video_renderer_.Paint(video_frame.get(), canvas, gfx_rect, alpha);
}

bool WebMediaPlayerImpl::hasSingleSecurityOrigin() const {
  if (data_source_)
    return data_source_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::didPassCORSAccessCheck() const {
  if (data_source_)
    return data_source_->DidPassCORSAccessCheck();
  return false;
}

double WebMediaPlayerImpl::mediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::decodedFrameCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::droppedFrameCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_->GetStatistics();

  unsigned frames_dropped = stats.video_frames_dropped +
      const_cast<media::VideoFramePainter*>(&painter_)
          ->GetFramesDroppedBeforePaint();
  DCHECK_LE(frames_dropped, stats.video_frames_decoded);
  return frames_dropped;
}

unsigned WebMediaPlayerImpl::audioDecodedByteCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.audio_bytes_decoded;
}

unsigned WebMediaPlayerImpl::videoDecodedByteCount() const {
  DCHECK(main_loop_->BelongsToCurrentThread());

  media::PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_bytes_decoded;
}

void WebMediaPlayerImpl::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  // This is called from both the main renderer thread and the compositor
  // thread (when the main thread is blocked).
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = client;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerImpl::GetCurrentFrame() {
  scoped_refptr<media::VideoFrame> current_frame =
      painter_.GetCurrentFrame(true);
  TRACE_EVENT_ASYNC_BEGIN0(
      "media", "WebMediaPlayerImpl:compositing", this);
  return current_frame;
}

void WebMediaPlayerImpl::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (!accelerated_compositing_reported_) {
    accelerated_compositing_reported_ = true;
    DCHECK(frame_->view()->isAcceleratedCompositingActive());
    UMA_HISTOGRAM_BOOLEAN("Media.AcceleratedCompositingActive", true);
  }
  TRACE_EVENT_ASYNC_END0("media", "WebMediaPlayerImpl:compositing", this);
}

bool WebMediaPlayerImpl::copyVideoTextureToPlatformTexture(
    blink::WebGraphicsContext3D* web_graphics_context,
    unsigned int texture,
    unsigned int level,
    unsigned int internal_format,
    unsigned int type,
    bool premultiply_alpha,
    bool flip_y) {
  scoped_refptr<media::VideoFrame> video_frame =
      painter_.GetCurrentFrame(false);

  TRACE_EVENT0("media", "WebMediaPlayerImpl:copyVideoTextureToPlatformTexture");

  if (!video_frame)
    return false;
  if (video_frame->format() != media::VideoFrame::NATIVE_TEXTURE)
    return false;

  gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  if (mailbox_holder->texture_target != GL_TEXTURE_2D)
    return false;

  // Since this method changes which texture is bound to the TEXTURE_2D target,
  // ideally it would restore the currently-bound texture before returning.
  // The cost of getIntegerv is sufficiently high, however, that we want to
  // avoid it in user builds. As a result assume (below) that |texture| is
  // bound when this method is called, and only verify this fact when
  // DCHECK_IS_ON.
  if (DCHECK_IS_ON()) {
    GLint bound_texture = 0;
    web_graphics_context->getIntegerv(GL_TEXTURE_BINDING_2D, &bound_texture);
    DCHECK_EQ(static_cast<GLuint>(bound_texture), texture);
  }

  uint32 source_texture = web_graphics_context->createTexture();

  web_graphics_context->waitSyncPoint(mailbox_holder->sync_point);
  web_graphics_context->bindTexture(GL_TEXTURE_2D, source_texture);
  web_graphics_context->consumeTextureCHROMIUM(GL_TEXTURE_2D,
                                               mailbox_holder->mailbox.name);

  // The video is stored in a unmultiplied format, so premultiply
  // if necessary.
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    premultiply_alpha);
  // Application itself needs to take care of setting the right flip_y
  // value down to get the expected result.
  // flip_y==true means to reverse the video orientation while
  // flip_y==false means to keep the intrinsic orientation.
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, flip_y);
  web_graphics_context->copyTextureCHROMIUM(GL_TEXTURE_2D,
                                            source_texture,
                                            texture,
                                            level,
                                            internal_format,
                                            type);
  web_graphics_context->pixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, false);
  web_graphics_context->pixelStorei(GL_UNPACK_PREMULTIPLY_ALPHA_CHROMIUM,
                                    false);

  // Restore the state for TEXTURE_2D binding point as mentioned above.
  web_graphics_context->bindTexture(GL_TEXTURE_2D, texture);

  web_graphics_context->deleteTexture(source_texture);

  // The flush() operation is not necessary here. It is kept since the
  // performance will be better when it is added than not.
  web_graphics_context->flush();
  return true;
}

// Helper functions to report media EME related stats to UMA. They follow the
// convention of more commonly used macros UMA_HISTOGRAM_ENUMERATION and
// UMA_HISTOGRAM_COUNTS. The reason that we cannot use those macros directly is
// that UMA_* macros require the names to be constant throughout the process'
// lifetime.
static void EmeUMAHistogramEnumeration(const blink::WebString& key_system,
                                       const std::string& method,
                                       int sample,
                                       int boundary_value) {
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, boundary_value, boundary_value + 1,
      base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

static void EmeUMAHistogramCounts(const blink::WebString& key_system,
                                  const std::string& method,
                                  int sample) {
  // Use the same parameters as UMA_HISTOGRAM_COUNTS.
  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method,
      1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)->Add(sample);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::MediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(
    const std::string& method,
    const WebString& key_system,
    WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  EmeUMAHistogramEnumeration(
      key_system, method, result_id, kMaxMediaKeyException);
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::generateKeyRequest(const WebString& key_system,
                                       const unsigned char* init_data,
                                       unsigned init_data_length) {
  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::GenerateKeyRequestInternal(
    const WebString& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  DVLOG(1) << "generateKeyRequest: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.isEmpty()) {
    if (!proxy_decryptor_) {
      proxy_decryptor_.reset(new ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
          client_,
          frame_,
#endif
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnKeyAdded),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnKeyError),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnKeyMessage)));
    }

    if (!proxy_decryptor_->InitializeCDM(key_system.utf8(),
                                         frame_->document().url()))
      return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

    if (proxy_decryptor_ && !decryptor_ready_cb_.is_null()) {
      base::ResetAndReturn(&decryptor_ready_cb_)
          .Run(proxy_decryptor_->GetDecryptor());
    }

    current_key_system_ = key_system;
  } else if (key_system != current_key_system_) {
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;
  }

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!proxy_decryptor_->GenerateKeyRequest(
           init_data_type_, init_data, init_data_length)) {
    current_key_system_.reset();
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::addKey(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e = AddKeyInternal(
      key_system, key, key_length, init_data, init_data_length, session_id);
  ReportMediaKeyExceptionToUMA("addKey", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::AddKeyInternal(
    const WebString& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const WebString& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);
  DVLOG(1) << "addKey: " << key_system.utf8().data() << ": "
           << std::string(reinterpret_cast<const char*>(key),
                          static_cast<size_t>(key_length)) << ", "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length))
           << " [" << session_id.utf8().data() << "]";


  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->AddKey(
      key, key_length, init_data, init_data_length, session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::cancelKeyRequest(
    const WebString& key_system,
    const WebString& session_id) {
  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(key_system, session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::CancelKeyRequestInternal(
    const WebString& key_system,
    const WebString& session_id) {
  if (!IsConcreteSupportedKeySystem(key_system))
    return WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.isEmpty() || key_system != current_key_system_)
    return WebMediaPlayer::MediaKeyExceptionInvalidPlayerState;

  proxy_decryptor_->CancelKeyRequest(session_id.utf8());
  return WebMediaPlayer::MediaKeyExceptionNoError;
}

void WebMediaPlayerImpl::setContentDecryptionModule(
    blink::WebContentDecryptionModule* cdm) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // TODO(xhwang): Support setMediaKeys(0) if necessary: http://crbug.com/330324
  if (!cdm)
    return;

  web_cdm_ = ToWebContentDecryptionModuleImpl(cdm);

  if (web_cdm_ && !decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_).Run(web_cdm_->GetDecryptor());
}

void WebMediaPlayerImpl::OnDestruct() {
  Destroy(RENDERFRAME_DESTROYED);
}

void WebMediaPlayerImpl::InvalidateOnMainThread() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  TRACE_EVENT0("media", "WebMediaPlayerImpl::InvalidateOnMainThread");

  painter_.DidFinishInvalidating();
  client_->repaint();
}

void WebMediaPlayerImpl::OnPipelineSeek(PipelineStatus status) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  starting_ = false;
  seeking_ = false;
  if (pending_seek_) {
    pending_seek_ = false;
    seek(pending_seek_seconds_);
    return;
  }

  if (status != media::PIPELINE_OK) {
    OnPipelineError(status);
    return;
  }

  // Update our paused time.
  if (paused_)
    paused_time_ = pipeline_->GetMediaTime();

  client_->timeChanged();
}

void WebMediaPlayerImpl::OnPipelineEnded() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  client_->timeChanged();
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK_NE(error, media::PIPELINE_OK);

  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);

    // TODO(scherkus): This should be handled by HTMLMediaElement and controls
    // should know when to invalidate themselves http://crbug.com/337015
    InvalidateOnMainThread();
    return;
  }

  SetNetworkState(PipelineErrorToNetworkState(error));

  if (error == media::PIPELINE_ERROR_DECRYPT)
    EmeUMAHistogramCounts(current_key_system_, "DecryptError", 1);

  // TODO(scherkus): This should be handled by HTMLMediaElement and controls
  // should know when to invalidate themselves http://crbug.com/337015
  InvalidateOnMainThread();
}

void WebMediaPlayerImpl::OnPipelineBufferingState(
    media::Pipeline::BufferingState buffering_state) {
  DVLOG(1) << "OnPipelineBufferingState(" << buffering_state << ")";

  // TODO(scherkus): Remove after tracking down cause for crashes
  // http://crbug.com/341184 http://crbug.com/341186
  CHECK(pipeline_);

  switch (buffering_state) {
    case media::Pipeline::kHaveMetadata:
      // TODO(scherkus): Would be better to have a metadata changed callback
      // that contained the size information as well whether audio/video is
      // present. Doing so would let us remove more methods off Pipeline.
      natural_size_ = pipeline_->GetInitialNaturalSize();

      SetReadyState(WebMediaPlayer::ReadyStateHaveMetadata);

      if (hasVideo() && client_->needsWebLayerForVideo()) {
        DCHECK(!video_weblayer_);
        video_weblayer_.reset(
            new webkit::WebLayerImpl(cc::VideoLayer::Create(this)));
        client_->setWebLayer(video_weblayer_.get());
      }
      break;
    case media::Pipeline::kPrerollCompleted:
      // Only transition to ReadyStateHaveEnoughData if we don't have
      // any pending seeks because the transition can cause Blink to
      // report that the most recent seek has completed.
      if (!pending_seek_)
        SetReadyState(WebMediaPlayer::ReadyStateHaveEnoughData);
      break;
  }

  // TODO(scherkus): This should be handled by HTMLMediaElement and controls
  // should know when to invalidate themselves http://crbug.com/337015
  InvalidateOnMainThread();
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  client_->mediaSourceOpened(new WebMediaSourceImpl(
      chunk_demuxer_, base::Bind(&LogMediaSourceError, media_log_)));
}

void WebMediaPlayerImpl::OnKeyAdded(const std::string& session_id) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  EmeUMAHistogramCounts(current_key_system_, "KeyAdded", 1);
  client_->keyAdded(current_key_system_, WebString::fromUTF8(session_id));
}

void WebMediaPlayerImpl::OnNeedKey(const std::string& type,
                                   const std::vector<uint8>& init_data) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!blink::WebRuntimeFeatures::isPrefixedEncryptedMediaEnabled() &&
      !blink::WebRuntimeFeatures::isEncryptedMediaEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
  if (init_data_type_.empty())
    init_data_type_ = type;

  const uint8* init_data_ptr = init_data.empty() ? NULL : &init_data[0];
  client_->keyNeeded(
      WebString::fromUTF8(type), init_data_ptr, init_data.size());
}

void WebMediaPlayerImpl::OnAddTextTrack(
    const media::TextTrackConfig& config,
    const media::AddTextTrackDoneCB& done_cb) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  const WebInbandTextTrackImpl::Kind web_kind =
      static_cast<WebInbandTextTrackImpl::Kind>(config.kind());
  const blink::WebString web_label =
      blink::WebString::fromUTF8(config.label());
  const blink::WebString web_language =
      blink::WebString::fromUTF8(config.language());
  const blink::WebString web_id =
      blink::WebString::fromUTF8(config.id());

  scoped_ptr<WebInbandTextTrackImpl> web_inband_text_track(
      new WebInbandTextTrackImpl(web_kind, web_label, web_language, web_id,
                                 text_track_index_++));

  scoped_ptr<media::TextTrack> text_track(
      new TextTrackImpl(main_loop_, client_, web_inband_text_track.Pass()));

  done_cb.Run(text_track.Pass());
}

void WebMediaPlayerImpl::OnKeyError(const std::string& session_id,
                                    media::MediaKeys::KeyError error_code,
                                    int system_code) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  EmeUMAHistogramEnumeration(current_key_system_, "KeyError",
                             error_code, media::MediaKeys::kMaxKeyError);

  client_->keyError(
      current_key_system_,
      WebString::fromUTF8(session_id),
      static_cast<blink::WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      system_code);
}

void WebMediaPlayerImpl::OnKeyMessage(const std::string& session_id,
                                      const std::vector<uint8>& message,
                                      const std::string& default_url) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  const GURL default_url_gurl(default_url);
  DLOG_IF(WARNING, !default_url.empty() && !default_url_gurl.is_valid())
      << "Invalid URL in default_url: " << default_url;

  client_->keyMessage(current_key_system_,
                      WebString::fromUTF8(session_id),
                      message.empty() ? NULL : &message[0],
                      message.size(),
                      default_url_gurl);
}

void WebMediaPlayerImpl::SetOpaque(bool opaque) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  client_->setOpaque(opaque);
}

void WebMediaPlayerImpl::DataSourceInitialized(const GURL& gurl, bool success) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  if (!success) {
    SetNetworkState(WebMediaPlayer::NetworkStateFormatError);

    // TODO(scherkus): This should be handled by HTMLMediaElement and controls
    // should know when to invalidate themselves http://crbug.com/337015
    InvalidateOnMainThread();
    return;
  }

  StartPipeline();
}

void WebMediaPlayerImpl::NotifyDownloading(bool is_downloading) {
  if (!is_downloading && network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateIdle);
  else if (is_downloading && network_state_ == WebMediaPlayer::NetworkStateIdle)
    SetNetworkState(WebMediaPlayer::NetworkStateLoading);
  media_log_->AddEvent(
      media_log_->CreateBooleanEvent(
          media::MediaLogEvent::NETWORK_ACTIVITY_SET,
          "is_downloading_data", is_downloading));
}

void WebMediaPlayerImpl::StartPipeline() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  // Keep track if this is a MSE or non-MSE playback.
  UMA_HISTOGRAM_BOOLEAN("Media.MSE.Playback",
                        (load_type_ == LoadTypeMediaSource));

  // Figure out which demuxer to use.
  if (load_type_ != LoadTypeMediaSource) {
    DCHECK(!chunk_demuxer_);
    DCHECK(data_source_);

    demuxer_.reset(new media::FFmpegDemuxer(
        media_loop_, data_source_.get(),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnNeedKey),
        media_log_));
  } else {
    DCHECK(!chunk_demuxer_);
    DCHECK(!data_source_);

    chunk_demuxer_ = new media::ChunkDemuxer(
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
        BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnNeedKey),
        base::Bind(&LogMediaSourceError, media_log_));
    demuxer_.reset(chunk_demuxer_);
  }

  scoped_ptr<media::FilterCollection> filter_collection(
      new media::FilterCollection());
  filter_collection->SetDemuxer(demuxer_.get());

  media::SetDecryptorReadyCB set_decryptor_ready_cb =
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetDecryptorReadyCB);

  // Create our audio decoders and renderer.
  ScopedVector<media::AudioDecoder> audio_decoders;
  audio_decoders.push_back(new media::FFmpegAudioDecoder(media_loop_));
  if (!cmd_line->HasSwitch(switches::kDisableOpusPlayback)) {
    audio_decoders.push_back(new media::OpusAudioDecoder(media_loop_));
  }

  scoped_ptr<media::AudioRenderer> audio_renderer(
      new media::AudioRendererImpl(media_loop_,
                                   audio_source_provider_.get(),
                                   audio_decoders.Pass(),
                                   set_decryptor_ready_cb));
  filter_collection->SetAudioRenderer(audio_renderer.Pass());

  // Create our video decoders and renderer.
  ScopedVector<media::VideoDecoder> video_decoders;

  if (gpu_factories_.get()) {
    video_decoders.push_back(
        new media::GpuVideoDecoder(gpu_factories_, media_log_));
  }

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new media::VpxVideoDecoder(media_loop_));
#endif  // !defined(MEDIA_DISABLE_LIBVPX)

  video_decoders.push_back(new media::FFmpegVideoDecoder(media_loop_));

  scoped_ptr<media::VideoRenderer> video_renderer(
      new media::VideoRendererImpl(
          media_loop_,
          video_decoders.Pass(),
          set_decryptor_ready_cb,
          base::Bind(&WebMediaPlayerImpl::FrameReady, base::Unretained(this)),
          BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetOpaque),
          true));
  filter_collection->SetVideoRenderer(video_renderer.Pass());

  if (cmd_line->HasSwitch(switches::kEnableInbandTextTracks)) {
    scoped_ptr<media::TextRenderer> text_renderer(
        new media::TextRenderer(
            media_loop_,
            BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnAddTextTrack)));

    filter_collection->SetTextRenderer(text_renderer.Pass());
  }

  // ... and we're ready to go!
  starting_ = true;
  pipeline_->Start(
      filter_collection.Pass(),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChange));
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << "SetNetworkState: " << state;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->networkStateChanged();
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DVLOG(1) << "SetReadyState: " << state;

  if (state == WebMediaPlayer::ReadyStateHaveEnoughData &&
      is_local_source_ &&
      network_state_ == WebMediaPlayer::NetworkStateLoading)
    SetNetworkState(WebMediaPlayer::NetworkStateLoaded);

  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  client_->readyStateChanged();
}

void WebMediaPlayerImpl::Destroy(DestroyReason reason) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // TODO(scherkus): Remove after tracking down cause for crashes
  // http://crbug.com/341184 http://crbug.com/341186
  CHECK((destroy_reason_ & reason) == 0);
  destroy_reason_ |= reason;

  // Abort any pending IO so stopping the pipeline doesn't get blocked.
  if (data_source_)
    data_source_->Abort();
  if (chunk_demuxer_) {
    chunk_demuxer_->Shutdown();
    chunk_demuxer_ = NULL;
  }

  gpu_factories_ = NULL;

  if (pipeline_) {
    // Make sure to kill the pipeline so there's no more media threads running.
    // Note: stopping the pipeline might block for a long time.
    base::WaitableEvent waiter(false, false);
    pipeline_->Stop(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
    waiter.Wait();

    // Let V8 know we are not using extra resources anymore.
    if (incremented_externally_allocated_memory_) {
      v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
          -kPlayerExtraMemory);
      incremented_externally_allocated_memory_ = false;
    }
  }

  // Release any final references now that everything has stopped.
  pipeline_.reset();
  demuxer_.reset();
  data_source_.reset();
}

blink::WebAudioSourceProvider* WebMediaPlayerImpl::audioSourceProvider() {
  return audio_source_provider_.get();
}

void WebMediaPlayerImpl::IncrementExternallyAllocatedMemory() {
  DCHECK(main_loop_->BelongsToCurrentThread());
  incremented_externally_allocated_memory_ = true;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      kPlayerExtraMemory);
}

double WebMediaPlayerImpl::GetPipelineDuration() const {
  base::TimeDelta duration = pipeline_->GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == media::kInfiniteDuration())
    return std::numeric_limits<double>::infinity();

  return duration.InSecondsF();
}

void WebMediaPlayerImpl::OnDurationChange() {
  if (ready_state_ == WebMediaPlayer::ReadyStateHaveNothing)
    return;

  client_->durationChanged();
}

void WebMediaPlayerImpl::OnNaturalSizeChange(gfx::Size size) {
  DCHECK(main_loop_->BelongsToCurrentThread());
  DCHECK_NE(ready_state_, WebMediaPlayer::ReadyStateHaveNothing);
  TRACE_EVENT0("media", "WebMediaPlayerImpl::OnNaturalSizeChanged");

  media_log_->AddEvent(
      media_log_->CreateVideoSizeSetEvent(size.width(), size.height()));
  natural_size_ = size;

  client_->sizeChanged();
}

void WebMediaPlayerImpl::FrameReady(
    const scoped_refptr<media::VideoFrame>& frame) {
  // TODO(scherkus): Today we always invalidate on the main thread even when
  // compositing is available, which is less efficient and involves more
  // thread hops. Refer to http://crbug.com/335345 for details.
  painter_.UpdateCurrentFrame(frame);
}

void WebMediaPlayerImpl::SetDecryptorReadyCB(
     const media::DecryptorReadyCB& decryptor_ready_cb) {
  DCHECK(main_loop_->BelongsToCurrentThread());

  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null())
      base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
    return;
  }

  // TODO(xhwang): Support multiple decryptor notification request (e.g. from
  // video and audio). The current implementation is okay for the current
  // media pipeline since we initialize audio and video decoders in sequence.
  // But WebMediaPlayerImpl should not depend on media pipeline's implementation
  // detail.
  DCHECK(decryptor_ready_cb_.is_null());

  // Mixed use of prefixed and unprefixed EME APIs is disallowed by Blink.
  DCHECK(!(proxy_decryptor_ && web_cdm_));

  if (proxy_decryptor_) {
    decryptor_ready_cb.Run(proxy_decryptor_->GetDecryptor());
    return;
  }

  if (web_cdm_) {
    decryptor_ready_cb.Run(web_cdm_->GetDecryptor());
    return;
  }

  decryptor_ready_cb_ = decryptor_ready_cb;
}

}  // namespace content
