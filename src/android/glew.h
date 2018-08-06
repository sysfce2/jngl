#pragma once

#define OPENGLES 1

#define glGenFramebuffers glGenFramebuffersOES
#define glBindFramebuffer glBindFramebufferOES
#define glGenRenderbuffers glGenRenderbuffersOES
#define glRenderbufferStorage glRenderbufferStorageOES
#define glBindRenderbuffer glBindRenderbufferOES
#define glFramebufferTexture2D glFramebufferTexture2DOES
#define glFramebufferRenderbuffer glFramebufferRenderbufferOES
#define glCheckFramebufferStatus glCheckFramebufferStatusOES
#define glDeleteFramebuffers glDeleteFramebuffersOES
#define glDeleteRenderbuffers glDeleteRenderbuffersOES
#define glBlendFuncSeparate glBlendFuncSeparateOES

extern PFNGLGENVERTEXARRAYSOESPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYOESPROC glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSOESPROC glDeleteVertexArrays;

#define GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_DEPTH_COMPONENT GL_DEPTH_COMPONENT16_OES
#define GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_OES
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define GL_RGBA8 GL_RGBA8_OES
#define GL_FRAMEBUFFER_BINDING GL_FRAMEBUFFER_BINDING_OES
#define GL_RENDERBUFFER_BINDING GL_RENDERBUFFER_BINDING_OES
