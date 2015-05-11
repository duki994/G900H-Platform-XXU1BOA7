// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#if defined(USE_X11)
#include <X11/Xlib.h>

// Xlib.h defines RootWindow.
#undef RootWindow
#endif

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/display/root_window_transformers.h"
#include "ash/host/window_tree_host_factory.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/layout.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif

namespace ash {
namespace internal {
namespace {

#if defined(USE_X11)
// Mirror window shouldn't handle input events.
void DisableInput(XID window) {
  long event_mask = ExposureMask | VisibilityChangeMask |
      StructureNotifyMask | PropertyChangeMask;
  XSelectInput(gfx::GetXDisplay(), window, event_mask);
}
#endif

class NoneCaptureClient : public aura::client::CaptureClient {
 public:
  NoneCaptureClient() {}
  virtual ~NoneCaptureClient() {}

 private:
  // Does a capture on the |window|.
  virtual void SetCapture(aura::Window* window) OVERRIDE {}

  // Releases a capture from the |window|.
  virtual void ReleaseCapture(aura::Window* window) OVERRIDE {}

  // Returns the current capture window.
  virtual aura::Window* GetCaptureWindow() OVERRIDE {
    return NULL;
  }
  virtual aura::Window* GetGlobalCaptureWindow() OVERRIDE {
    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(NoneCaptureClient);
};

}  // namespace

MirrorWindowController::MirrorWindowController() {}

MirrorWindowController::~MirrorWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close();
}

void MirrorWindowController::UpdateWindow(const DisplayInfo& display_info) {
  static int mirror_root_window_count = 0;

  if (!root_window_.get()) {
    const gfx::Rect& bounds_in_native = display_info.bounds_in_native();
    aura::RootWindow::CreateParams params(bounds_in_native);
    params.host = Shell::GetInstance()->window_tree_host_factory()->
        CreateWindowTreeHost(bounds_in_native);
    root_window_.reset(new aura::RootWindow(params));
    root_window_->window()->SetName(
        base::StringPrintf("MirrorRootWindow-%d", mirror_root_window_count++));
    root_window_->host()->compositor()->SetBackgroundColor(SK_ColorBLACK);
    // No need to remove RootWindowObserver because
    // the DisplayController object outlives RootWindow objects.
    root_window_->AddRootWindowObserver(
        Shell::GetInstance()->display_controller());
    root_window_->AddRootWindowObserver(this);
    // TODO(oshima): TouchHUD is using idkey.
    InitRootWindowSettings(root_window_->window())->display_id =
        display_info.id();
    root_window_->host()->InitHost();
#if defined(USE_X11)
    DisableInput(root_window_->host()->GetAcceleratedWidget());
#endif

    aura::client::SetCaptureClient(root_window_->window(),
                                   new NoneCaptureClient());
    root_window_->host()->Show();

    // TODO(oshima): Start mirroring.
    aura::Window* mirror_window = new aura::Window(NULL);
    mirror_window->Init(aura::WINDOW_LAYER_TEXTURED);
    root_window_->window()->AddChild(mirror_window);
    mirror_window->SetBounds(root_window_->window()->bounds());
    mirror_window->Show();
    reflector_ = ui::ContextFactory::GetInstance()->CreateReflector(
        Shell::GetPrimaryRootWindow()->GetDispatcher()->host()->compositor(),
        mirror_window->layer());
  } else {
    GetRootWindowSettings(root_window_->window())->display_id =
        display_info.id();
    root_window_->host()->SetBounds(display_info.bounds_in_native());
  }

  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  scoped_ptr<aura::RootWindowTransformer> transformer(
      internal::CreateRootWindowTransformerForMirroredDisplay(
          source_display_info,
          display_info));
  root_window_->host()->SetRootWindowTransformer(transformer.Pass());
}

void MirrorWindowController::UpdateWindow() {
  if (root_window_.get()) {
    DisplayManager* display_manager = Shell::GetInstance()->display_manager();
    const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
        display_manager->mirrored_display_id());
    UpdateWindow(mirror_display_info);
  }
}

void MirrorWindowController::Close() {
  if (root_window_.get()) {
    ui::ContextFactory::GetInstance()->RemoveReflector(reflector_);
    reflector_ = NULL;
    NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
        aura::client::GetCaptureClient(root_window_->window()));
    aura::client::SetCaptureClient(root_window_->window(), NULL);
    delete capture_client;

    root_window_->RemoveRootWindowObserver(
        Shell::GetInstance()->display_controller());
    root_window_->RemoveRootWindowObserver(this);
    root_window_.reset();
  }
}

void MirrorWindowController::OnWindowTreeHostResized(
    const aura::RootWindow* root) {
  // Do not use |old_size| as it contains RootWindow's (but not host's) size,
  // and this parameter wil be removed soon.
  if (mirror_window_host_size_ == root->host()->GetBounds().size())
    return;
  mirror_window_host_size_ = root->host()->GetBounds().size();
  reflector_->OnMirroringCompositorResized();
  root_window_->host()->SetRootWindowTransformer(
      CreateRootWindowTransformer().Pass());
  Shell::GetInstance()->display_controller()->cursor_window_controller()->
      UpdateLocation();
}


scoped_ptr<aura::RootWindowTransformer>
MirrorWindowController::CreateRootWindowTransformer() const {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  const DisplayInfo& mirror_display_info = display_manager->GetDisplayInfo(
      display_manager->mirrored_display_id());
  const DisplayInfo& source_display_info = display_manager->GetDisplayInfo(
      Shell::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(display_manager->IsMirrored());
  return scoped_ptr<aura::RootWindowTransformer>(
      internal::CreateRootWindowTransformerForMirroredDisplay(
          source_display_info,
          mirror_display_info));
}

}  // namespace internal
}  // namespace ash
