#ifndef __EGL_H__
#define __EGL_H__

int initializeEGL(Display *xdisp, Window &xwindow, EGLDisplay &display, EGLContext &context, EGLSurface &surface,bool verbose);
void destroyEGL(EGLDisplay &display, EGLContext &context, EGLSurface &surface);

#endif