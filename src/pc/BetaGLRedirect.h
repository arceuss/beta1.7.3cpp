#pragma once
#include "BetaGLCalls.h"
#undef glAlphaFunc
#define glAlphaFunc BetaGL::AlphaFunc
#undef glBeginQuery
#define glBeginQuery BetaGL::BeginQuery
#undef glBindBuffer
#define glBindBuffer BetaGL::BindBuffer
#undef glBindTexture
#define glBindTexture BetaGL::BindTexture
#undef glBlendFunc
#define glBlendFunc BetaGL::BlendFunc
#undef glBufferData
#define glBufferData BetaGL::BufferData
#undef glCallList
#define glCallList BetaGL::CallList
#undef glCallLists
#define glCallLists BetaGL::CallLists
#undef glClear
#define glClear BetaGL::Clear
#undef glClearColor
#define glClearColor BetaGL::ClearColor
#undef glClearDepth
#define glClearDepth BetaGL::ClearDepth
#undef glColor3f
#define glColor3f BetaGL::Color3f
#undef glColor4f
#define glColor4f BetaGL::Color4f
#undef glColorMask
#define glColorMask BetaGL::ColorMask
#undef glColorMaterial
#define glColorMaterial BetaGL::ColorMaterial
#undef glColorPointer
#define glColorPointer BetaGL::ColorPointer
#undef glCullFace
#define glCullFace BetaGL::CullFace
#undef glDeleteLists
#define glDeleteLists BetaGL::DeleteLists
#undef glDeleteQueries
#define glDeleteQueries BetaGL::DeleteQueries
#undef glDeleteTextures
#define glDeleteTextures BetaGL::DeleteTextures
#undef glDepthFunc
#define glDepthFunc BetaGL::DepthFunc
#undef glDepthMask
#define glDepthMask BetaGL::DepthMask
#undef glDisable
#define glDisable BetaGL::Disable
#undef glDisableClientState
#define glDisableClientState BetaGL::DisableClientState
#undef glDrawArrays
#define glDrawArrays BetaGL::DrawArrays
#undef glEnable
#define glEnable BetaGL::Enable
#undef glEnableClientState
#define glEnableClientState BetaGL::EnableClientState
#undef glEndList
#define glEndList BetaGL::EndList
#undef glEndQuery
#define glEndQuery BetaGL::EndQuery
#undef glFogf
#define glFogf BetaGL::Fogf
#undef glFogfv
#define glFogfv BetaGL::Fogfv
#undef glFogi
#define glFogi BetaGL::Fogi
#undef glFrustum
#define glFrustum BetaGL::Frustum
#undef glGenBuffers
#define glGenBuffers BetaGL::GenBuffers
#undef glGenLists
#define glGenLists BetaGL::GenLists
#undef glGenQueries
#define glGenQueries BetaGL::GenQueries
#undef glGenTextures
#define glGenTextures BetaGL::GenTextures
#undef glGetError
#define glGetError BetaGL::GetError
#undef glGetFloatv
#define glGetFloatv BetaGL::GetFloatv
#undef glGetQueryObjectuiv
#define glGetQueryObjectuiv BetaGL::GetQueryObjectuiv
#undef glGetString
#define glGetString BetaGL::GetString
#undef glLightModelfv
#define glLightModelfv BetaGL::LightModelfv
#undef glLightfv
#define glLightfv BetaGL::Lightfv
#undef glLineWidth
#define glLineWidth BetaGL::LineWidth
#undef glLoadIdentity
#define glLoadIdentity BetaGL::LoadIdentity
#undef glMatrixMode
#define glMatrixMode BetaGL::MatrixMode
#undef glNewList
#define glNewList BetaGL::NewList
#undef glNormal3f
#define glNormal3f BetaGL::Normal3f
#undef glNormalPointer
#define glNormalPointer BetaGL::NormalPointer
#undef glOrtho
#define glOrtho BetaGL::Ortho
#undef glPixelStorei
#define glPixelStorei BetaGL::PixelStorei
#undef glPolygonOffset
#define glPolygonOffset BetaGL::PolygonOffset
#undef glPopMatrix
#define glPopMatrix BetaGL::PopMatrix
#undef glPushMatrix
#define glPushMatrix BetaGL::PushMatrix
#undef glReadPixels
#define glReadPixels BetaGL::ReadPixels
#undef glRotatef
#define glRotatef BetaGL::Rotatef
#undef glScaled
#define glScaled BetaGL::Scaled
#undef glScalef
#define glScalef BetaGL::Scalef
#undef glShadeModel
#define glShadeModel BetaGL::ShadeModel
#undef glTexCoordPointer
#define glTexCoordPointer BetaGL::TexCoordPointer
#undef glTexImage2D
#define glTexImage2D BetaGL::TexImage2D
#undef glTexParameteri
#define glTexParameteri BetaGL::TexParameteri
#undef glTexSubImage2D
#define glTexSubImage2D BetaGL::TexSubImage2D
#undef glTranslatef
#define glTranslatef BetaGL::Translatef
#undef glVertexPointer
#define glVertexPointer BetaGL::VertexPointer
#undef glViewport
#define glViewport BetaGL::Viewport
#undef glDeleteBuffers
#define glDeleteBuffers BetaGL::DeleteBuffers
#undef glBufferSubData
#define glBufferSubData BetaGL::BufferSubData
#undef glGetBufferSubData
#define glGetBufferSubData BetaGL::GetBufferSubData
#undef glDrawElements
#define glDrawElements BetaGL::DrawElements
#undef glGetIntegerv
#define glGetIntegerv BetaGL::GetIntegerv
#undef glReadBuffer
#define glReadBuffer BetaGL::ReadBuffer
#undef glFinish
#define glFinish BetaGL::Finish
