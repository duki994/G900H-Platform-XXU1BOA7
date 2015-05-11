// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace gles2 {

namespace {

void EnableDisable(GLenum pname, bool enable) {
  if (enable) {
    glEnable(pname);
  } else {
    glDisable(pname);
  }
}

GLuint Get2dServiceId(const TextureUnit& unit) {
  return unit.bound_texture_2d.get()
      ? unit.bound_texture_2d->service_id() : 0;
}

GLuint GetCubeServiceId(const TextureUnit& unit) {
  return unit.bound_texture_cube_map.get()
      ? unit.bound_texture_cube_map->service_id() : 0;
}

GLuint GetOesServiceId(const TextureUnit& unit) {
  return unit.bound_texture_external_oes.get()
      ? unit.bound_texture_external_oes->service_id() : 0;
}

GLuint GetArbServiceId(const TextureUnit& unit) {
  return unit.bound_texture_rectangle_arb.get()
      ? unit.bound_texture_rectangle_arb->service_id() : 0;
}

}  // anonymous namespace.

TextureUnit::TextureUnit()
    : bind_target(GL_TEXTURE_2D) {
}

TextureUnit::~TextureUnit() {
}

ContextState::ContextState(FeatureInfo* feature_info,
                           #if defined(S_PLM_P140603_03145)
                           ErrorStateClient* error_state_client,
                           #endif
                           Logger* logger)
    : active_texture_unit(0),
      pack_reverse_row_order(false),
      fbo_binding_for_scissor_workaround_dirty_(false),
      feature_info_(feature_info),
      error_state_(ErrorState::Create(
        #if defined(S_PLM_P140603_03145)
        error_state_client,
        #endif 
      logger)) {
  Initialize();
}

ContextState::~ContextState() {
}

void ContextState::RestoreTextureUnitBindings(
    GLuint unit, const ContextState* prev_state) const {
  DCHECK_LT(unit, texture_units.size());
  const TextureUnit& texture_unit = texture_units[unit];
  GLuint service_id_2d = Get2dServiceId(texture_unit);
  GLuint service_id_cube = GetCubeServiceId(texture_unit);
  GLuint service_id_oes = GetOesServiceId(texture_unit);
  GLuint service_id_arb = GetArbServiceId(texture_unit);

  bool bind_texture_2d = true;
  bool bind_texture_cube = true;
  bool bind_texture_oes = feature_info_->feature_flags().oes_egl_image_external;
  bool bind_texture_arb = feature_info_->feature_flags().arb_texture_rectangle;

  if (prev_state) {
    const TextureUnit& prev_unit = prev_state->texture_units[unit];
    bind_texture_2d = service_id_2d != Get2dServiceId(prev_unit);
    bind_texture_cube = service_id_cube != GetCubeServiceId(prev_unit);
    bind_texture_oes =
        bind_texture_oes && service_id_oes != GetOesServiceId(prev_unit);
    bind_texture_arb =
        bind_texture_arb && service_id_arb != GetArbServiceId(prev_unit);
  }

  // Early-out if nothing has changed from the previous state.
  if (!bind_texture_2d && !bind_texture_cube
      && !bind_texture_oes && !bind_texture_arb) {
    return;
  }

  glActiveTexture(GL_TEXTURE0 + unit);
  if (bind_texture_2d) {
    glBindTexture(GL_TEXTURE_2D, service_id_2d);
  }
  if (bind_texture_cube) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, service_id_cube);
  }
  if (bind_texture_oes) {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, service_id_oes);
  }
  if (bind_texture_arb) {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, service_id_arb);
  }
}

void ContextState::RestoreBufferBindings() const {
  if (vertex_attrib_manager.get()) {
    Buffer* element_array_buffer =
        vertex_attrib_manager->element_array_buffer();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
        element_array_buffer ? element_array_buffer->service_id() : 0);
  }
  glBindBuffer(GL_ARRAY_BUFFER,
               bound_array_buffer.get() ? bound_array_buffer->service_id() : 0);
}

void ContextState::RestoreRenderbufferBindings() const {
  // Restore Bindings
  glBindRenderbufferEXT(
      GL_RENDERBUFFER,
      bound_renderbuffer.get() ? bound_renderbuffer->service_id() : 0);
}

void ContextState::RestoreProgramBindings() const {
  glUseProgram(current_program.get() ? current_program->service_id() : 0);
}

void ContextState::RestoreActiveTexture() const {
  glActiveTexture(GL_TEXTURE0 + active_texture_unit);
}

void ContextState::RestoreAllTextureUnitBindings(
    const ContextState* prev_state) const {
  // Restore Texture state.
  for (size_t ii = 0; ii < texture_units.size(); ++ii) {
    RestoreTextureUnitBindings(ii, prev_state);
  }
  RestoreActiveTexture();
}

void ContextState::RestoreAttribute(GLuint attrib_index) const {
  const VertexAttrib* attrib =
      vertex_attrib_manager->GetVertexAttrib(attrib_index);
  const void* ptr = reinterpret_cast<const void*>(attrib->offset());
  Buffer* buffer = attrib->buffer();
  glBindBuffer(GL_ARRAY_BUFFER, buffer ? buffer->service_id() : 0);
  glVertexAttribPointer(
      attrib_index, attrib->size(), attrib->type(), attrib->normalized(),
      attrib->gl_stride(), ptr);
  if (attrib->divisor())
    glVertexAttribDivisorANGLE(attrib_index, attrib->divisor());
  // Never touch vertex attribute 0's state (in particular, never
  // disable it) when running on desktop GL because it will never be
  // re-enabled.
  if (attrib_index != 0 ||
      gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2) {
    if (attrib->enabled()) {
      glEnableVertexAttribArray(attrib_index);
    } else {
      glDisableVertexAttribArray(attrib_index);
    }
  }
  glVertexAttrib4fv(attrib_index, attrib_values[attrib_index].v);
}

void ContextState::RestoreGlobalState() const {
  InitCapabilities();
  InitState();
}

void ContextState::RestoreState(const ContextState* prev_state) const {
  RestoreAllTextureUnitBindings(prev_state);

  // Restore Attrib State
  // TODO: This if should not be needed. RestoreState is getting called
  // before GLES2Decoder::Initialize which is a bug.
  if (vertex_attrib_manager.get()) {
    // TODO(gman): Move this restoration to VertexAttribManager.
    for (size_t attrib = 0; attrib < vertex_attrib_manager->num_attribs();
         ++attrib) {
      RestoreAttribute(attrib);
    }
  }

  RestoreBufferBindings();
  RestoreRenderbufferBindings();
  RestoreProgramBindings();
  RestoreGlobalState();
}

ErrorState* ContextState::GetErrorState() {
  return error_state_.get();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/context_state_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu


