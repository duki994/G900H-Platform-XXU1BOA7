

#ifndef SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_VIEW_CORE_IMPL_H_
#define SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_VIEW_CORE_IMPL_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "url/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
#include "sbrowser/content/native/browser/android/sbr/sbr_ui_resource_layer_manager.h"
#endif

namespace gfx {
class JavaBitmap;
}

namespace ui {
class ViewAndroid;
class WindowAndroid;
}

namespace content {
//class RenderWidgetHostViewAndroid;
class ContentViewCoreImpl;
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
class SbrUIResourceLayerManagerClient;
#endif
struct MenuItem;

// TODO(jrg): this is a shell.  Upstream the rest.
class SbrContentViewCoreImpl : public ContentViewCoreImpl
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
                             , public SbrUIResourceLayerManagerClient 
#endif
{
 public:
  SbrContentViewCoreImpl(JNIEnv* env,
                         jobject obj,
                         WebContents* web_contents,
                         ui::ViewAndroid* view_android,
                         ui::WindowAndroid* window_android);

  // ContentViewCore implementation.

#if defined(S_NOTIFY_ROTATE_STATUS)
  void NotifyRotateStatus();
#endif

#if defined(S_MEDIAPLAYER_SBRCONTENTVIEWCOREIMPL_CREATEMEDIAPLAYERNOTIFICATION)
  void CreateMediaPlayerNotification();
#endif

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  jboolean IsIncognito(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jstring>
      GetContentMimeType(JNIEnv* env, jobject obj);

  void GetSelectionVisibilityStatus(JNIEnv* env, jobject obj);

  void CheckBelongToSelection(JNIEnv* env, jobject obj, jint TouchX, jint TouchY);

  void GetSelectionBitmap(JNIEnv* env , jobject obj);

  void selectClosestWord(JNIEnv* env , jobject obj,int x , int y);

  void clearTextSelection(JNIEnv* env , jobject obj);

  // SBROWSER_HANDLE_MOUSECLICK_CTRL ++
  void HandleMouseClickWithCtrlkey(JNIEnv* env , jobject obj, int x, int y);

  void OnOpenUrlInNewTab(const base::string16& mouse_click_url);
  // SBROWSER_HANDLE_MOUSECLICK_CTRL --

  //Sent to browser for setting last touch point for long press enter key : start
  void SetLongPressSelectionPoint(int x, int y);
  //Sent to browser for setting last touch point for long press enter key : end

  void PerformLongPress(JNIEnv* env, jobject obj, jlong time_ms,
                        jfloat x, jfloat y,
                        jboolean disambiguation_popup_tap);

  void GetSelectionMarkup(JNIEnv* env , jobject obj);

  void SavePageAs(JNIEnv* env, jobject obj);

  void GetBitmapFromCachedResource(JNIEnv* env, jobject obj, jstring image_url);

  void SetPasswordEcho(JNIEnv* env, jobject obj, jboolean passwordEchoEnabled);

  void RecognizeArticle(JNIEnv* env, jobject obj, int mode);

  void SbrScrollBy(JNIEnv* env, jobject obj, jlong time_ms,
                   jint x, jint y, jfloat dx, jfloat dy);

  //-------------------------------------------------------------------------- 
  //Tab Crash APIs 
  //--------------------------------------------------------------------------
  jboolean NeedsReload(JNIEnv* env, jobject obj);
  void ResetTabState(JNIEnv* env, jobject obj);
  jboolean Crashed(JNIEnv* env, jobject obj) const { return tab_crashed_; }
  // Tab Crash APIs

  // --------------------------------------------------------------------------
  // Public methods that call to Java via JNI
  // --------------------------------------------------------------------------

  // Tab Crash APIs
  void OnTabCrashed();
  // Tab Crash APIs
   
  void SelectedBitmap(const SkBitmap& Bitmap);

  void PointOnRegion(bool isOnRegion);

  void OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params);

  void SelectBetweenCoordinates(JNIEnv* env, jobject obj,
                                jfloat x1, jfloat y1,
                                jfloat x2, jfloat y2, jboolean isLastTouchPoint,jboolean isFirstTouchPoint);
    
  void SetSelectionVisibilty(bool isVisible);

  void UpdateCurrentSelectionRect(const gfx::Rect& selectionRect);
#if defined(S_SCROLL_EVENT)
  void OnTextFieldBoundsChanged(const gfx::Rect&  input_edit_rect);
#endif
  void SelectedMarkup(const base::string16& markup);

  void FocusedNodeChanged(bool is_editable_node, bool is_select_node,long node_id);

  void ShowSelectPopupMenu(const std::vector<MenuItem>& items,
                           int selected_item,
                           bool multiple);

  //SBROWSER_FORM_NAVIGATION
  void ShowSelectPopupMenu(const std::vector<MenuItem>& items,
                           int selected_item,
                           bool multiple,
                           int advanced_ime_options);

  void selectPopupCloseZero();

  void UpdateImeAdapter(int native_ime_adapter,
                        int text_input_type,
                        const std::string& text,
                        int selection_start,
                        int selection_end,
                        int composition_start,
                        int composition_end,
                        bool show_ime_if_needed,
                        bool require_ack,
                        int advanced_ime_options);

  void OnRecognizeArticleResult(std::string reader_result_str);

  void moveFocusToNext(JNIEnv* env , jobject obj);

  #if defined (SBROWSER_MULTIINSTANCE_TAB_DRAG_AND_DROP)
  bool GetTabDragAndDropIsInProgress();
  #endif

  void moveFocusToPrevious(JNIEnv* env , jobject obj);
  //SBROWSER_FORM_NAVIGATION
  void GetTouchedFixedElementHeight(JNIEnv* env, jobject obj, jint x, jint y);
  void UpdateTouchedFixedElementHeight(int height);

  void OnSSRMModeCallback(int SSRMCaller, int count); //0 is v8, 1 is css, 2 is canvas, 3 is etc

  // All sizes and offsets are in CSS pixels as cached by the renderer.
  void UpdateFrameInfo(const gfx::Vector2dF& scroll_offset,
                       float page_scale_factor,
                       const gfx::Vector2dF& page_scale_factor_limits,
                       const gfx::SizeF& content_size,
                       const gfx::SizeF& viewport_size,
                       const gfx::Vector2dF& controls_offset,
                       const gfx::Vector2dF& content_offset,
                       float overdraw_bottom_height);

  void UpdateImeAdapter(int native_ime_adapter, int text_input_type,
                        const std::string& text,
                        int selection_start, int selection_end,
                        int composition_start, int composition_end,
                        bool show_ime_if_needed, bool require_ack);
  
  void ConfirmTouchEvent(InputEventAckState ack_result);
//PIPETTE >>
  void OnUpdateFocusedInputInfo(const gfx::Rect& bounds, bool is_multi_line_input, bool is_content_richly_editable); 
//PIPETTE <<
  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  void SavePageFileName(const base::FilePath::StringType& pure_file_name);

#if defined(S_FP_AUTOLOGIN_FAILURE_ALERT)
  void ShowAutoLoginFailureMsg();
#endif

  void OnReceiveBitmapFromCache(const SkBitmap& bitmap);

  void Destroy(JNIEnv* env, jobject obj);

  // AsyncReadBack  API to be used by all other features except magnifier.
  void PopulateHardwareBitmap(JNIEnv* env,
                              jobject obj,
                              jint x, jint y, jint width, jint height,
                              jboolean configRGB565);
  void PopulateHardwareBitmapFinished(bool result,
                                      const SkBitmap& sk_bitmap);
  void LoadDataWithBaseUrl(JNIEnv* env, jobject obj, jstring data,
                           jstring base_url, jstring mime_type, jstring encoding,
                           jstring history_url);

  jboolean IsWMLPage(JNIEnv* env, jobject obj);
  void HandleSelectionDrop(JNIEnv* env , jobject obj, int x, int y,
                           jstring text);
//PIPETTE >>
  void HandleSelectionDropOnFocusedInput(JNIEnv* env, jobject obj, jstring text, int dropAction);
  void GetFocusedInputInfo(JNIEnv* env, jobject obj);
//PIPETTE <<

  // Sync readback api for Magnifier support.
  // FIXME:This API will be deprecated from next version of S-Browser.
  // Opensource uses only async way of getting the bitmap.
  jboolean PopulateBitmapFromDelegatedLayerSync(JNIEnv* env, jobject obj,
                                                jint x, jint y,
                                                jint width, jint height,
                                                jobject jbitmap);

  void performLongClickOnFocussedNode(JNIEnv* env, jobject obj, jlong time_ms);

  // Show highlight around the the object under the point.
  void ShowHoverFocus(JNIEnv* env, jobject obj,
                      jfloat x, jfloat y,
                      jlong time_ms,
                      jboolean high_light);
  void PopulateBitmapFromCompositorAsync(JNIEnv* env,
                                         jobject obj,
                                         jint x,
                                         jint y,
                                         jint width,
                                         jint height,
                                         jfloat scale,
                                         SkBitmap::Config bitmap_config);

  jboolean PopulateBitmapFromCompositor(JNIEnv* env,
                                        jobject obj,
                                        jobject jbitmap);

  // Callback declaration for Softbitmap
  void PopulateSoftwareBitmapFinished(bool result, const SkBitmap& sk_bitmap);

  // When the Renderer is in background gets the snapshot of the renderer
  // using a callback.
  void PopulateSoftwareBitmap(JNIEnv* env, jobject obj, jint x,	jint y,
                              jint width, jint height, jfloat page_scale_factor);

#if defined(S_INTUITIVE_HOVER)
  void OnHoverHitTestResult(int contentType);
#endif
//SBROWSER_HIDE_URLBAR_HYBRID >>
  void OnRendererInitializeComplete();
  void SetTopControlsHeight(JNIEnv* env, jobject obj, jint top_controls_height);
  void SetScrollType(JNIEnv* env, jobject obj, jint type);
//SBROWSER_HIDE_URLBAR_HYBRID <<
#if defined(SBROWSER_HIDE_URLBAR_EOP)
// This function will notify contentView about the webpage end condition
  void OnUpdateEndOfPageState( bool eop_state );
#endif
  void SetUIResourceBitmap(JNIEnv* env, jobject obj, jint layer_type, jobject bitmap);
  void EnableUIResourceLayer(JNIEnv* env, jobject obj, jint layer_type, jboolean enable);
  void MoveUIResourceLayer(JNIEnv* env, jobject obj, jint layer_type, jfloat offsetX, jfloat offsetY);
  int  HandleUIResourceLayerEvent(JNIEnv* env, jobject obj, jfloat offsetX, jfloat offsetY);
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
  SbrUIResourceLayerManager* GetUIResourceLayerManager();
//SbrUIResourceLayerManagerClient Interfaces
  virtual scoped_refptr<cc::Layer> root_layer() OVERRIDE;
  virtual void didEnableUIResourceLayer(int layer_type, bool composited, bool visible) OVERRIDE;
  virtual void onScrollEnd(bool scroll_ignored) OVERRIDE;
  virtual gfx::SizeF GetViewPortSizePix() OVERRIDE;
  virtual float GetDeviceScaleFactor() OVERRIDE;
#endif
//SBROWSER_HIDE_URLBAR_HYBRID <<
// MULTI-SELECTION >>
  void GetSelectionMarkupWithBounds(JNIEnv* env, jobject obj);
  void OnSelectedMarkupWithStartContentRect(const base::string16& markup, const gfx::Rect& selection_start_content_rect);
// MULTI-SELECTION <<
#if defined(SBROWSER_GRAPHICS_GETBITMAP)
  jboolean GetBitmapFromCompositor(JNIEnv* env,
                                   jobject obj,  jint x, jint y, jint width, jint height,
                                   jobject jbitmap,jint imageFormat);
#endif //SBROWSER_GRAPHICS_GETBITMAP

// video should be paused once activity onPause() got called
#if defined(S_MEDIAPLAYER_SBRCONTENTVIEWCOREIMPL_PAUSEVIDEO)
  bool IsPlayerEmpty(JNIEnv* env, jobject obj);
  void OnPauseVideo(JNIEnv* env, jobject obj);
#endif
#if defined(SBROWSER_UI_COMPOSITOR_SET_BACKGROUND_COLOR)
  void SetBackgroundColor(JNIEnv* env, jobject obj);
#endif

  bool IsAnyVideoPlaying(JNIEnv* env, jobject obj);
#if defined(S_MEDIAPLAYER_AUDIOFOCUS_MESSAGE_FIX)
  void ShowAudioFocusFailMessage();
#endif

 private:
  virtual ~SbrContentViewCoreImpl();

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  // The Android view that can be used to add and remove decoration layers
  // like AutofillPopup.
  ui::ViewAndroid* view_android_;

  // The owning window that has a hold of main application activity.
  ui::WindowAndroid* window_android_;

  // The cache of device's current orientation set from Java side, this value
  // will be sent to Renderer once it is ready.
  int device_orientation_;

  bool geolocation_needs_pause_;

  base::android::ScopedJavaGlobalRef<jobject> jbitmap_;
  scoped_ptr<gfx::JavaBitmap> java_bitmap_;

  // Added for Tab Crash APIs
  // Whether the renderer backing this ContentViewCore has crashed.  
  bool tab_crashed_;
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)
  SbrUIResourceLayerManager* ui_resource_layer_manager_;  
#endif
  
  DISALLOW_COPY_AND_ASSIGN(SbrContentViewCoreImpl);
};

bool RegisterSbrContentViewCore(JNIEnv* env);

}  // namespace content

#endif  // SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_VIEW_CORE_IMPL_H_
