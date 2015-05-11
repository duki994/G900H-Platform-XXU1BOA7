// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace skia_bindings {

GrGLInterface* CreateCommandBufferSkiaGLBinding() {
  GrGLInterface* interface = new GrGLInterface;
  
#if defined(SBROWSER_GPU_RASTERIZATION_ENABLE)
  interface->fStandard = kGLES_GrGLStandard;
  interface->fExtensions.init(kGLES_GrGLStandard,
                              glGetString,
                              NULL,
                              glGetIntegerv);

  GrGLInterface::Functions* functions = &interface->fFunctions;
  functions->fActiveTexture = glActiveTexture;
  functions->fAttachShader = glAttachShader;
  functions->fBindAttribLocation = glBindAttribLocation;
  functions->fBindBuffer = glBindBuffer;
  functions->fBindTexture = glBindTexture;
  functions->fBindVertexArray = glBindVertexArrayOES;
  functions->fBlendColor = glBlendColor;
  functions->fBlendFunc = glBlendFunc;
  functions->fBufferData = glBufferData;
  functions->fBufferSubData = glBufferSubData;
  functions->fClear = glClear;
  functions->fClearColor = glClearColor;
  functions->fClearStencil = glClearStencil;
  functions->fColorMask = glColorMask;
  functions->fCompileShader = glCompileShader;
  functions->fCompressedTexImage2D = glCompressedTexImage2D;
  functions->fCopyTexSubImage2D = glCopyTexSubImage2D;
  functions->fCreateProgram = glCreateProgram;
  functions->fCreateShader = glCreateShader;
  functions->fCullFace = glCullFace;
  functions->fDeleteBuffers = glDeleteBuffers;
  functions->fDeleteProgram = glDeleteProgram;
  functions->fDeleteShader = glDeleteShader;
  functions->fDeleteTextures = glDeleteTextures;
  functions->fDeleteVertexArrays = glDeleteVertexArraysOES;
  functions->fDepthMask = glDepthMask;
  functions->fDisable = glDisable;
  functions->fDisableVertexAttribArray = glDisableVertexAttribArray;
  functions->fDiscardFramebuffer = glDiscardFramebufferEXT;
  functions->fDrawArrays = glDrawArrays;
  functions->fDrawElements = glDrawElements;
  functions->fEnable = glEnable;
  functions->fEnableVertexAttribArray = glEnableVertexAttribArray;
  functions->fFinish = glFinish;
  functions->fFlush = glFlush;
  functions->fFrontFace = glFrontFace;
  functions->fGenBuffers = glGenBuffers;
  functions->fGenTextures = glGenTextures;
  functions->fGenVertexArrays = glGenVertexArraysOES;
  functions->fGetBufferParameteriv = glGetBufferParameteriv;
  functions->fGetError = glGetError;
  functions->fGetIntegerv = glGetIntegerv;
  functions->fGetProgramInfoLog = glGetProgramInfoLog;
  functions->fGetProgramiv = glGetProgramiv;
  functions->fGetShaderInfoLog = glGetShaderInfoLog;
  functions->fGetShaderiv = glGetShaderiv;
  functions->fGetString = glGetString;
  functions->fGetUniformLocation = glGetUniformLocation;
  functions->fInsertEventMarker = glInsertEventMarkerEXT;
  functions->fLineWidth = glLineWidth;
  functions->fLinkProgram = glLinkProgram;
  functions->fMapBufferSubData = glMapBufferSubDataCHROMIUM;
  functions->fMapTexSubImage2D = glMapTexSubImage2DCHROMIUM;
  functions->fPixelStorei = glPixelStorei;
  functions->fPopGroupMarker = glPopGroupMarkerEXT;
  functions->fPushGroupMarker = glPushGroupMarkerEXT;
  functions->fReadPixels = glReadPixels;
  functions->fScissor = glScissor;
  functions->fShaderSource = glShaderSource;
  functions->fStencilFunc = glStencilFunc;
  functions->fStencilFuncSeparate = glStencilFuncSeparate;
  functions->fStencilMask = glStencilMask;
  functions->fStencilMaskSeparate = glStencilMaskSeparate;
  functions->fStencilOp = glStencilOp;
  functions->fStencilOpSeparate = glStencilOpSeparate;
  functions->fTexImage2D = glTexImage2D;
  functions->fTexParameteri = glTexParameteri;
  functions->fTexParameteriv = glTexParameteriv;
  functions->fTexStorage2D = glTexStorage2DEXT;
  functions->fTexSubImage2D = glTexSubImage2D;
  functions->fUniform1f = glUniform1f;
  functions->fUniform1i = glUniform1i;
  functions->fUniform1fv = glUniform1fv;
  functions->fUniform1iv = glUniform1iv;
  functions->fUniform2f = glUniform2f;
  functions->fUniform2i = glUniform2i;
  functions->fUniform2fv = glUniform2fv;
  functions->fUniform2iv = glUniform2iv;
  functions->fUniform3f = glUniform3f;
  functions->fUniform3i = glUniform3i;
  functions->fUniform3fv = glUniform3fv;
  functions->fUniform3iv = glUniform3iv;
  functions->fUniform4f = glUniform4f;
  functions->fUniform4i = glUniform4i;
  functions->fUniform4fv = glUniform4fv;
  functions->fUniform4iv = glUniform4iv;
  functions->fUniformMatrix2fv = glUniformMatrix2fv;
  functions->fUniformMatrix3fv = glUniformMatrix3fv;
  functions->fUniformMatrix4fv = glUniformMatrix4fv;
  functions->fUnmapBufferSubData = glUnmapBufferSubDataCHROMIUM;
  functions->fUnmapTexSubImage2D = glUnmapTexSubImage2DCHROMIUM;
  functions->fUseProgram = glUseProgram;
  functions->fVertexAttrib4fv = glVertexAttrib4fv;
  functions->fVertexAttribPointer = glVertexAttribPointer;
  functions->fViewport = glViewport;
  functions->fBindFramebuffer = glBindFramebuffer;
  functions->fBindRenderbuffer = glBindRenderbuffer;
  functions->fCheckFramebufferStatus = glCheckFramebufferStatus;
  functions->fDeleteFramebuffers = glDeleteFramebuffers;
  functions->fDeleteRenderbuffers = glDeleteRenderbuffers;
  functions->fFramebufferRenderbuffer = glFramebufferRenderbuffer;
  functions->fFramebufferTexture2D = glFramebufferTexture2D;
  functions->fFramebufferTexture2DMultisample =
    glFramebufferTexture2DMultisampleEXT;
  functions->fGenFramebuffers = glGenFramebuffers;
  functions->fGenRenderbuffers = glGenRenderbuffers;
  functions->fGetFramebufferAttachmentParameteriv =
    glGetFramebufferAttachmentParameteriv;
  functions->fGetRenderbufferParameteriv = glGetRenderbufferParameteriv;
  functions->fRenderbufferStorage = glRenderbufferStorage;
  functions->fRenderbufferStorageMultisample =
    glRenderbufferStorageMultisampleCHROMIUM;
  functions->fRenderbufferStorageMultisampleES2EXT =
    glRenderbufferStorageMultisampleEXT;
  functions->fBindUniformLocation = glBindUniformLocationCHROMIUM;
  functions->fBlitFramebuffer = glBlitFramebufferCHROMIUM;
  functions->fGenerateMipmap = glGenerateMipmap;
#else
  interface->fBindingsExported = kES2_GrGLBinding;
  interface->fActiveTexture = glActiveTexture;
  interface->fAttachShader = glAttachShader;
  interface->fBindAttribLocation = glBindAttribLocation;
  interface->fBindBuffer = glBindBuffer;
  interface->fBindTexture = glBindTexture;
  interface->fBindVertexArray = glBindVertexArrayOES;
  interface->fBlendColor = glBlendColor;
  interface->fBlendFunc = glBlendFunc;
  interface->fBufferData = glBufferData;
  interface->fBufferSubData = glBufferSubData;
  interface->fClear = glClear;
  interface->fClearColor = glClearColor;
  interface->fClearStencil = glClearStencil;
  interface->fColorMask = glColorMask;
  interface->fCompileShader = glCompileShader;
  interface->fCompressedTexImage2D = glCompressedTexImage2D;
  interface->fCopyTexSubImage2D = glCopyTexSubImage2D;
  interface->fCreateProgram = glCreateProgram;
  interface->fCreateShader = glCreateShader;
  interface->fCullFace = glCullFace;
  interface->fDeleteBuffers = glDeleteBuffers;
  interface->fDeleteProgram = glDeleteProgram;
  interface->fDeleteShader = glDeleteShader;
  interface->fDeleteTextures = glDeleteTextures;
  interface->fDeleteVertexArrays = glDeleteVertexArraysOES;
  interface->fDepthMask = glDepthMask;
  interface->fDisable = glDisable;
  interface->fDisableVertexAttribArray = glDisableVertexAttribArray;
  interface->fDrawArrays = glDrawArrays;
  interface->fDrawElements = glDrawElements;
  interface->fEnable = glEnable;
  interface->fEnableVertexAttribArray = glEnableVertexAttribArray;
  interface->fFinish = glFinish;
  interface->fFlush = glFlush;
  interface->fFrontFace = glFrontFace;
  interface->fGenBuffers = glGenBuffers;
  interface->fGenTextures = glGenTextures;
  interface->fGenVertexArrays = glGenVertexArraysOES;
  interface->fGetBufferParameteriv = glGetBufferParameteriv;
  interface->fGetError = glGetError;
  interface->fGetIntegerv = glGetIntegerv;
  interface->fGetProgramInfoLog = glGetProgramInfoLog;
  interface->fGetProgramiv = glGetProgramiv;
  interface->fGetShaderInfoLog = glGetShaderInfoLog;
  interface->fGetShaderiv = glGetShaderiv;
  interface->fGetString = glGetString;
  interface->fGetUniformLocation = glGetUniformLocation;
  interface->fLineWidth = glLineWidth;
  interface->fLinkProgram = glLinkProgram;
  interface->fPixelStorei = glPixelStorei;
  interface->fReadPixels = glReadPixels;
  interface->fScissor = glScissor;
  interface->fShaderSource = glShaderSource;
  interface->fStencilFunc = glStencilFunc;
  interface->fStencilFuncSeparate = glStencilFuncSeparate;
  interface->fStencilMask = glStencilMask;
  interface->fStencilMaskSeparate = glStencilMaskSeparate;
  interface->fStencilOp = glStencilOp;
  interface->fStencilOpSeparate = glStencilOpSeparate;
  interface->fTexImage2D = glTexImage2D;
  interface->fTexParameteri = glTexParameteri;
  interface->fTexParameteriv = glTexParameteriv;
  interface->fTexStorage2D = glTexStorage2DEXT;
  interface->fTexSubImage2D = glTexSubImage2D;
  interface->fUniform1f = glUniform1f;
  interface->fUniform1i = glUniform1i;
  interface->fUniform1fv = glUniform1fv;
  interface->fUniform1iv = glUniform1iv;
  interface->fUniform2f = glUniform2f;
  interface->fUniform2i = glUniform2i;
  interface->fUniform2fv = glUniform2fv;
  interface->fUniform2iv = glUniform2iv;
  interface->fUniform3f = glUniform3f;
  interface->fUniform3i = glUniform3i;
  interface->fUniform3fv = glUniform3fv;
  interface->fUniform3iv = glUniform3iv;
  interface->fUniform4f = glUniform4f;
  interface->fUniform4i = glUniform4i;
  interface->fUniform4fv = glUniform4fv;
  interface->fUniform4iv = glUniform4iv;
  interface->fUniformMatrix2fv = glUniformMatrix2fv;
  interface->fUniformMatrix3fv = glUniformMatrix3fv;
  interface->fUniformMatrix4fv = glUniformMatrix4fv;
  interface->fUseProgram = glUseProgram;
  interface->fVertexAttrib4fv = glVertexAttrib4fv;
  interface->fVertexAttribPointer = glVertexAttribPointer;
  interface->fViewport = glViewport;
  interface->fBindFramebuffer = glBindFramebuffer;
  interface->fBindRenderbuffer = glBindRenderbuffer;
  interface->fCheckFramebufferStatus = glCheckFramebufferStatus;
  interface->fDeleteFramebuffers = glDeleteFramebuffers;
  interface->fDeleteRenderbuffers = glDeleteRenderbuffers;
  interface->fFramebufferRenderbuffer = glFramebufferRenderbuffer;
  interface->fFramebufferTexture2D = glFramebufferTexture2D;
  interface->fFramebufferTexture2DMultisample =
    glFramebufferTexture2DMultisampleEXT;
  interface->fGenFramebuffers = glGenFramebuffers;
  interface->fGenRenderbuffers = glGenRenderbuffers;
  interface->fGetFramebufferAttachmentParameteriv =
    glGetFramebufferAttachmentParameteriv;
  interface->fGetRenderbufferParameteriv = glGetRenderbufferParameteriv;
  interface->fRenderbufferStorage = glRenderbufferStorage;
  interface->fRenderbufferStorageMultisample =
    glRenderbufferStorageMultisampleCHROMIUM;
  interface->fRenderbufferStorageMultisampleES2EXT =
    glRenderbufferStorageMultisampleEXT;
  interface->fBindUniformLocation = glBindUniformLocationCHROMIUM;
  interface->fBlitFramebuffer = glBlitFramebufferCHROMIUM;
  interface->fGenerateMipmap = glGenerateMipmap;
#endif
  return interface;
}

}  // namespace skia
