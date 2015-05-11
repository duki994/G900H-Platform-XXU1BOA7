

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_MEDIA_SBR_MEDIA_PLAYER_MOTION_LISTENER_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_MEDIA_SBR_MEDIA_PLAYER_MOTION_LISTENER_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class MessageLoopProxy;
}


namespace media {
class MediaPlayerBridge;
class MediaSourcePlayer;

class SbrMediaPlayerMotionListener {
public:
  SbrMediaPlayerMotionListener(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      base::WeakPtr<media::MediaPlayerBridge> media_player);
  SbrMediaPlayerMotionListener(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      base::WeakPtr<media::MediaSourcePlayer> media_source_player);
  virtual ~SbrMediaPlayerMotionListener();

  // Create a Java MediaPlayerListener object.
  void CreateMediaPlayerMotionListener(jobject context);

  void PauseMedia(JNIEnv* , jobject );

  void RegisterReceiver();
  void UnRegisterReceiver();

  static bool RegisterSbrMediaPlayerMotionListener(JNIEnv* env);

private:
  // The message loop where |media_player_| lives.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // The MediaPlayerBridge object all the callbacks should be send to.
  base::WeakPtr<media::MediaPlayerBridge> media_player_;

  // The MediaSourcePlayer object all the callbacks should be send to.
  base::WeakPtr<media::MediaSourcePlayer> media_source_player_;

  base::android::ScopedJavaGlobalRef<jobject> j_media_player_motion_listener_;

  DISALLOW_COPY_AND_ASSIGN(SbrMediaPlayerMotionListener);
};

} // namespace media

#endif  // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_MEDIA_SBR_MEDIA_PLAYER_MOTION_LISTENER_H_
