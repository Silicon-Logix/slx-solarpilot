// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#define GLFW_INCLUDE_NONE
#include <GL/glcorearb.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

#define SLX_GL_FUNCTIONS(X)                                                                        \
  X(PFNGLVIEWPORTPROC, Viewport)                                                                   \
  X(PFNGLCLEARCOLORPROC, ClearColor)                                                               \
  X(PFNGLCLEARPROC, Clear)                                                                         \
  X(PFNGLENABLEPROC, Enable)                                                                       \
  X(PFNGLDISABLEPROC, Disable)                                                                     \
  X(PFNGLDEPTHMASKPROC, DepthMask)                                                                 \
  X(PFNGLPOLYGONOFFSETPROC, PolygonOffset)                                                         \
  X(PFNGLDEPTHFUNCPROC, DepthFunc)                                                                 \
  X(PFNGLBLENDFUNCPROC, BlendFunc)                                                                 \
  X(PFNGLCULLFACEPROC, CullFace)                                                                   \
  X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays)                                                     \
  X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray)                                                     \
  X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)                                               \
  X(PFNGLGENBUFFERSPROC, GenBuffers)                                                               \
  X(PFNGLBINDBUFFERPROC, BindBuffer)                                                               \
  X(PFNGLBUFFERDATAPROC, BufferData) X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) X(                   \
      PFNGLENABLEVERTEXATTRIBARRAYPROC,                                                            \
      EnableVertexAttribArray) X(PFNGLVERTEXATTRIBPOINTERPROC,                                     \
                                 VertexAttribPointer) X(PFNGLCREATESHADERPROC, CreateShader)       \
      X(PFNGLSHADERSOURCEPROC, ShaderSource) X(PFNGLCOMPILESHADERPROC, CompileShader) X(           \
          PFNGLGETSHADERIVPROC, GetShaderiv) X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)        \
          X(PFNGLDELETESHADERPROC, DeleteShader) X(PFNGLCREATEPROGRAMPROC, CreateProgram) X(       \
              PFNGLATTACHSHADERPROC,                                                               \
              AttachShader) X(PFNGLLINKPROGRAMPROC,                                                \
                              LinkProgram) X(PFNGLGETPROGRAMIVPROC,                                \
                                             GetProgramiv) X(PFNGLGETPROGRAMINFOLOGPROC,           \
                                                             GetProgramInfoLog)                    \
              X(PFNGLDELETEPROGRAMPROC, DeleteProgram) X(PFNGLUSEPROGRAMPROC, UseProgram) X(       \
                  PFNGLGETUNIFORMLOCATIONPROC,                                                     \
                  GetUniformLocation) X(PFNGLUNIFORMMATRIX4FVPROC,                                 \
                                        UniformMatrix4fv) X(PFNGLUNIFORM4FVPROC, Uniform4fv)       \
                  X(PFNGLUNIFORM3FPROC, Uniform3f) X(PFNGLUNIFORM1IPROC, Uniform1i) X(             \
                      PFNGLUNIFORM1FPROC, Uniform1f) X(PFNGLGENTEXTURESPROC, GenTextures)          \
                      X(PFNGLBINDTEXTUREPROC, BindTexture) X(PFNGLTEXIMAGE2DPROC, TexImage2D) X(   \
                          PFNGLTEXPARAMETERIPROC, TexParameteri) X(PFNGLGENERATEMIPMAPPROC,        \
                                                                   GenerateMipmap)                 \
                          X(PFNGLDELETETEXTURESPROC, DeleteTextures) X(                            \
                              PFNGLACTIVETEXTUREPROC, ActiveTexture) X(PFNGLGENFRAMEBUFFERSPROC,   \
                                                                       GenFramebuffers)            \
                              X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) X(                      \
                                  PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D)             \
                                  X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers) X(            \
                                      PFNGLGENRENDERBUFFERSPROC, GenRenderbuffers)                 \
                                      X(PFNGLBINDRENDERBUFFERPROC, BindRenderbuffer) X(            \
                                          PFNGLRENDERBUFFERSTORAGEPROC,                            \
                                          RenderbufferStorage) X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, \
                                                                 FramebufferRenderbuffer)          \
                                          X(PFNGLDELETERENDERBUFFERSPROC, DeleteRenderbuffers) X(  \
                                              PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,             \
                                              RenderbufferStorageMultisample)                      \
                                              X(PFNGLBLITFRAMEBUFFERPROC, BlitFramebuffer) X(      \
                                                  PFNGLDRAWELEMENTSPROC,                           \
                                                  DrawElements) X(PFNGLDRAWBUFFERPROC, DrawBuffer) \
                                                  X(PFNGLREADBUFFERPROC, ReadBuffer)               \
                                                      X(PFNGLREADPIXELSPROC, ReadPixels)           \
                                                          X(PFNGLPIXELSTOREIPROC, PixelStorei)     \
                                                              X(PFNGLGETSTRINGPROC, GetString)     \
                                                                  X(PFNGLGETERRORPROC, GetError)

namespace slx::desktop::gl {
#define DECLARE_GL(type, name) inline type name = nullptr;
SLX_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
inline void load() {
#define LOAD_GL(type, name)                                                                        \
  name = reinterpret_cast<type>(glfwGetProcAddress("gl" #name));                                   \
  if (!name)                                                                                       \
    throw std::runtime_error("OpenGL 3.3 entry point missing: " #name);
  SLX_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
}
} // namespace slx::desktop::gl
