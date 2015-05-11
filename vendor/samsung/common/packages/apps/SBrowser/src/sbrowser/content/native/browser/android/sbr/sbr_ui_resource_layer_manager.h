
#if defined(SBROWSER_HIDE_URLBAR_UI_COMPOSITOR)

#ifndef SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_UI_RESOURCE_LAYER_MANAGER_H_
#define SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_UI_RESOURCE_LAYER_MANAGER_H_

#include "base/containers/hash_tables.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "cc/layers/ui_resource_layer.h"
#include "ui/gfx/size_f.h"

namespace content {

class SbrUIResourceLayerManagerClient {
 
  public:
  virtual scoped_refptr<cc::Layer> root_layer() = 0;
  virtual void didEnableUIResourceLayer(int layer_type, bool composited, bool visible) = 0;
  virtual void onScrollEnd(bool scroll_ignored) = 0;
  virtual gfx::SizeF GetViewPortSizePix() = 0;
  virtual float GetDeviceScaleFactor() = 0;

};

class SbrUIResourceLayerManager {
  public:
  enum SbrUIResourceLayerType {
    SBROWSER_TOPBAR_LAYER = 1,
    SBROWSER_BOTTOMBAR_LAYER,
    LAYER_NONE,
  };
  enum SbrUIResourceLayerState {
    LayerAdded = 1,           
    LayerEnablePending,       
    LayerEnabled,             
    LayerDisablePending,      
    LayerDisabled             
  };
  class SbrUIResource {
    public:
    static SbrUIResource* Create(SbrUIResourceLayerType layer_type, SbrUIResourceLayerState layer_state, scoped_refptr<cc::UIResourceLayer> layer){
         SbrUIResource* resource = new SbrUIResource();
         if(resource){
           resource->layer_type_ = layer_type;
           resource->layer_state_ = layer_state;
           resource->layer_ = layer;
         }
         return resource;
    }
    SbrUIResourceLayerType layer_type_;
    SbrUIResourceLayerState layer_state_;
    scoped_refptr<cc::UIResourceLayer> layer_;    
  };
  SbrUIResourceLayerManager(SbrUIResourceLayerManagerClient* client);
  ~SbrUIResourceLayerManager();
  void SetUIResourceBitmap(int layer_type, SkBitmap* bitmap);
  void EnableUIResourceLayer(int layer_type, bool enable);
  void MoveUIResourceLayer(int layer_type, float offsetX, float offsetY);
  int  HandleUIResourceLayerEvent(float offsetX, float offsetY);
  void UpdateUIResourceLayers();
  void UpdateViewPortSize(gfx::SizeF size);
  void UpdateUIResourceWidgets();
  void SetTopControlsHeight(float top_controls_height);
  void SetTopControlsOffset(float top_controls_offset);
  void DidViewPortSizeChanged(gfx::SizeF size);
  void SetPageScaleFactor(float page_scale_factor);
  void SetDeviceScaleFactor(float device_scale_factor);
  void onScrollEnd(bool scroll_ignored);
  void UpdateLocalBitmap(int layer_type, SkBitmap* bitmap);
  void Attach();
  void Detach();
  void CleanUp();
  bool isAttached() {return attached_;}

  private:
  SbrUIResourceLayerManagerClient* client_;
  SbrUIResource* ui_resources_[LAYER_NONE+1];
  gfx::SizeF viewport_size_;
  bool attached_;
  bool update_resource_widgets_;
  float top_controls_height_;
  float top_controls_offset_;
  float page_scale_factor_;
  float device_scale_factor_;
  SkBitmap *top_layer_bitmap_;
  SkBitmap *bottom_layer_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(SbrUIResourceLayerManager);
};

}
#endif // SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_UI_RESOURCE_LAYER_MANAGER_H_
#endif // SBROWSER_HIDE_URLBAR_UI_COMPOSITOR
