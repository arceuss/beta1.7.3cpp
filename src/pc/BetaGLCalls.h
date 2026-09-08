#pragma once
#include "BetaGL.h"

namespace BetaGL
{
void APIENTRY AlphaFunc(GLenum func, GLfloat ref);
void APIENTRY BeginQuery(GLenum target, GLuint id);
void APIENTRY BindBuffer(GLenum target, GLuint buffer);
void APIENTRY BindTexture(GLenum target, GLuint texture);
void APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor);
void APIENTRY BufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
void APIENTRY CallList(GLuint list);
void APIENTRY CallLists(GLsizei n, GLenum type, const void *lists);
void APIENTRY Clear(GLbitfield mask);
void APIENTRY ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void APIENTRY ClearDepth(GLdouble depth);
void APIENTRY Color3f(GLfloat red, GLfloat green, GLfloat blue);
void APIENTRY Color4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void APIENTRY ColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void APIENTRY ColorMaterial(GLenum face, GLenum mode);
void APIENTRY ColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void APIENTRY CullFace(GLenum mode);
void APIENTRY DeleteLists(GLuint list, GLsizei range);
void APIENTRY DeleteQueries(GLsizei n, const GLuint *ids);
void APIENTRY DeleteTextures(GLsizei n, const GLuint *textures);
void APIENTRY DepthFunc(GLenum func);
void APIENTRY DepthMask(GLboolean flag);
void APIENTRY Disable(GLenum cap);
void APIENTRY DisableClientState(GLenum array);
void APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count);
void APIENTRY Enable(GLenum cap);
void APIENTRY EnableClientState(GLenum array);
void APIENTRY EndList();
void APIENTRY EndQuery(GLenum target);
void APIENTRY Fogf(GLenum pname, GLfloat param);
void APIENTRY Fogfv(GLenum pname, const GLfloat *params);
void APIENTRY Fogi(GLenum pname, GLint param);
void APIENTRY Frustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void APIENTRY GenBuffers(GLsizei n, GLuint *buffers);
GLuint APIENTRY GenLists(GLsizei range);
void APIENTRY GenQueries(GLsizei n, GLuint *ids);
void APIENTRY GenTextures(GLsizei n, GLuint *textures);
GLenum APIENTRY GetError();
void APIENTRY GetFloatv(GLenum pname, GLfloat *data);
void APIENTRY GetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
const GLubyte *APIENTRY GetString(GLenum name);
void APIENTRY LightModelfv(GLenum pname, const GLfloat *params);
void APIENTRY Lightfv(GLenum light, GLenum pname, const GLfloat *params);
void APIENTRY LineWidth(GLfloat width);
void APIENTRY LoadIdentity();
void APIENTRY MatrixMode(GLenum mode);
void APIENTRY NewList(GLuint list, GLenum mode);
void APIENTRY Normal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void APIENTRY NormalPointer(GLenum type, GLsizei stride, const void *pointer);
void APIENTRY Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void APIENTRY PixelStorei(GLenum pname, GLint param);
void APIENTRY PolygonOffset(GLfloat factor, GLfloat units);
void APIENTRY PopMatrix();
void APIENTRY PushMatrix();
void APIENTRY ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
void APIENTRY Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void APIENTRY Scaled(GLdouble x, GLdouble y, GLdouble z);
void APIENTRY Scalef(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY ShadeModel(GLenum mode);
void APIENTRY TexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void APIENTRY TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
						 GLenum format, GLenum type, const void *pixels);
void APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param);
void APIENTRY TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
							GLenum format, GLenum type, const void *pixels);
void APIENTRY Translatef(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY VertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height);
void APIENTRY DeleteBuffers(GLsizei n, const GLuint *buffers);
void APIENTRY BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
void APIENTRY GetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, void *data);
void APIENTRY DrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void APIENTRY GetIntegerv(GLenum pname, GLint *data);
void APIENTRY ReadBuffer(GLenum mode);
void APIENTRY Finish();
} // namespace BetaGL
