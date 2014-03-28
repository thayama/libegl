/*
 * Copyright (c) 2013 Renesas Solutions Corp.
 * Copyright (c) 2012 Carsten Munk <carsten.munk@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 *  Takanari Hayama <taki@igel.co.jp>
 *
 */

/* EGL function pointers */
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define IMG_LIBEGL_PATH	"libEGL-pvr.so"

#if defined(DEBUG)
#	define EGL_DEBUG(s, x...)	{ printf(s, ## x); }
#else
#	define EGL_DEBUG(s, x...)	{ }
#endif

#ifndef EGL_WAYLAND_BUFFER_WL
/* TI headers don't define this */
#define EGL_WAYLAND_BUFFER_WL    0x31D5	/* eglCreateImageKHR target */
#endif

#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define WANT_WAYLAND

#ifdef WANT_WAYLAND
#include <wayland-kms.h>
#include <gbm/gbm.h>
#include <gbm/common.h>
#endif

static EGLint(*_eglGetError) (void);

static EGLDisplay(*_eglGetDisplay) (EGLNativeDisplayType display_id);
static EGLBoolean(*_eglInitialize) (EGLDisplay dpy, EGLint * major,
				   EGLint * minor);
static EGLBoolean(*_eglTerminate) (EGLDisplay dpy);

static const char *(*_eglQueryString) (EGLDisplay dpy, EGLint name);

static EGLBoolean(*_eglGetConfigs) (EGLDisplay dpy, EGLConfig * configs,
				   EGLint config_size, EGLint * num_config);
static EGLBoolean(*_eglChooseConfig) (EGLDisplay dpy,
				     const EGLint * attrib_list,
				     EGLConfig * configs,
				     EGLint config_size, EGLint * num_config);
static EGLBoolean(*_eglGetConfigAttrib) (EGLDisplay dpy,
					EGLConfig config,
					EGLint attribute, EGLint * value);

static EGLSurface(*_eglCreateWindowSurface) (EGLDisplay dpy,
					    EGLConfig config,
					    EGLNativeWindowType win,
					    const EGLint * attrib_list);
static EGLSurface(*_eglCreatePbufferSurface) (EGLDisplay dpy,
					     EGLConfig config,
					     const EGLint * attrib_list);
static EGLSurface(*_eglCreatePixmapSurface) (EGLDisplay dpy,
					    EGLConfig config,
					    EGLNativePixmapType pixmap,
					    const EGLint * attrib_list);
static EGLBoolean(*_eglDestroySurface) (EGLDisplay dpy, EGLSurface surface);
static EGLBoolean(*_eglQuerySurface) (EGLDisplay dpy, EGLSurface surface,
				     EGLint attribute, EGLint * value);

static EGLBoolean(*_eglBindAPI) (EGLenum api);
static EGLenum(*_eglQueryAPI) (void);

static EGLBoolean(*_eglWaitClient) (void);

static EGLBoolean(*_eglReleaseThread) (void);

static EGLSurface(*_eglCreatePbufferFromClientBuffer) (EGLDisplay dpy,
						      EGLenum buftype,
						      EGLClientBuffer
						      buffer,
						      EGLConfig config,
						      const EGLint *
						      attrib_list);

static EGLBoolean(*_eglSurfaceAttrib) (EGLDisplay dpy,
				      EGLSurface surface,
				      EGLint attribute, EGLint value);
static EGLBoolean(*_eglBindTexImage) (EGLDisplay dpy, EGLSurface surface,
				     EGLint buffer);
static EGLBoolean(*_eglReleaseTexImage) (EGLDisplay dpy,
					EGLSurface surface, EGLint buffer);

static EGLBoolean(*_eglSwapInterval) (EGLDisplay dpy, EGLint interval);

static EGLContext(*_eglCreateContext) (EGLDisplay dpy, EGLConfig config,
				      EGLContext share_context,
				      const EGLint * attrib_list);
static EGLBoolean(*_eglDestroyContext) (EGLDisplay dpy, EGLContext ctx);
static EGLBoolean(*_eglMakeCurrent) (EGLDisplay dpy, EGLSurface draw,
				    EGLSurface read, EGLContext ctx);

static EGLContext(*_eglGetCurrentContext) (void);
static EGLSurface(*_eglGetCurrentSurface) (EGLint readdraw);
static EGLDisplay(*_eglGetCurrentDisplay) (void);
static EGLBoolean(*_eglQueryContext) (EGLDisplay dpy, EGLContext ctx,
				     EGLint attribute, EGLint * value);

static EGLBoolean(*_eglWaitGL) (void);
static EGLBoolean(*_eglWaitNative) (EGLint engine);
static EGLBoolean(*_eglSwapBuffers) (EGLDisplay dpy, EGLSurface surface);
static EGLBoolean(*_eglCopyBuffers) (EGLDisplay dpy, EGLSurface surface,
				    EGLNativePixmapType target);

static EGLImageKHR(*_eglCreateImageKHR) (EGLDisplay dpy, EGLContext ctx,
					EGLenum target,
					EGLClientBuffer buffer,
					const EGLint * attrib_list);
static EGLBoolean(*_eglDestroyImageKHR) (EGLDisplay dpy, EGLImageKHR image);

static __eglMustCastToProperFunctionPointerType(*_eglGetProcAddress) (const char *procname);

static void *_libegl = NULL;

static void _init_egl()
{
	const char *filename = (getenv("LIBEGL")) ? getenv("LIBEGL") : IMG_LIBEGL_PATH;
	_libegl = dlopen(filename, RTLD_LAZY);
}

#define EGL_DLSYM(f)			\
	if (!_libegl) _init_egl();	\
	if (!_##f) _##f = dlsym(_libegl, #f);	\

EGLint eglGetError(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetError);
	return _eglGetError();
}

#ifdef WANT_WAYLAND
static struct gbm_device *__gbm;
static struct wl_display *__wl_display = NULL;
#endif

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetDisplay);

#ifdef WANT_WAYLAND
	if (display_id) {
		void *head = *(void**)display_id;
		if (head == gbm_create_device)
			__gbm = (struct gbm_device*)display_id;
		if (head == &wl_display_interface)
			__wl_display = (struct wl_display*)display_id;
	}
#endif

#if 0
	{
		FILE *fp = fopen("/proc/self/maps", "r");
		char buf[BUFSIZ];
		int count;
		while(!feof(fp)) {
			count = fread(buf, 1, BUFSIZ, fp);
			fwrite(buf, count, 1, stdout);
		}
		fclose(fp);

		EGL_DEBUG("\n\n%s: %p\n", __func__, _eglGetDisplay);
	}
#endif
	return _eglGetDisplay(display_id);
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint * major, EGLint * minor)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglInitialize);
	return _eglInitialize(dpy, major, minor);
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglTerminate);
	return _eglTerminate(dpy);
}

#define EGL_WL_EXT_STRING "EGL_WL_bind_wayland_display "

const char *eglQueryString(EGLDisplay dpy, EGLint name)
{
	static char *_eglextstr = NULL;
	const char *ret;

	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglQueryString);
	ret = _eglQueryString(dpy, name);

#ifdef WANT_WAYLAND
	if (name == EGL_EXTENSIONS) {
		assert(ret != NULL);

		if (!_eglextstr) {
			_eglextstr = calloc(1, strlen(ret) + strlen(EGL_WL_EXT_STRING) + 1);
			if (_eglextstr) {
				strcat(_eglextstr, ret);
				strcat(_eglextstr, EGL_WL_EXT_STRING);
			} else {
				_eglextstr = (char*)ret;
			}
		}

		ret = _eglextstr;
	}
#endif
	return ret;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig * configs,
			 EGLint config_size, EGLint * num_config)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetConfigs);
	return _eglGetConfigs(dpy, configs, config_size, num_config);
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint * attrib_list,
			   EGLConfig * configs, EGLint config_size,
			   EGLint * num_config)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglChooseConfig);
	return _eglChooseConfig(dpy, attrib_list,
			       configs, config_size, num_config);
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
			      EGLint attribute, EGLint * value)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetConfigAttrib);
	return _eglGetConfigAttrib(dpy, config, attribute, value);
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
				  EGLNativeWindowType win,
				  const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCreateWindowSurface);
	return _eglCreateWindowSurface(dpy, config, win, attrib_list);
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
				   const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCreatePbufferSurface);
	return _eglCreatePbufferSurface(dpy, config, attrib_list);
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
				  EGLNativePixmapType pixmap,
				  const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCreatePixmapSurface);
	return _eglCreatePixmapSurface(dpy, config, pixmap, attrib_list);
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglDestroySurface);
	return _eglDestroySurface(dpy, surface);
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
			   EGLint attribute, EGLint * value)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglQuerySurface);
	return _eglQuerySurface(dpy, surface, attribute, value);
}

EGLBoolean eglBindAPI(EGLenum api)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglBindAPI);
	return _eglBindAPI(api);
}

EGLenum eglQueryAPI(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglQueryAPI);
	return _eglQueryAPI();
}

EGLBoolean eglWaitClient(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglWaitClient);
	return _eglWaitClient();
}

EGLBoolean eglReleaseThread(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglReleaseThread);
	return _eglReleaseThread();
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
					    EGLClientBuffer buffer,
					    EGLConfig config,
					    const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCreatePbufferFromClientBuffer);
	return _eglCreatePbufferFromClientBuffer(dpy, buftype, buffer,
						config, attrib_list);
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
			    EGLint attribute, EGLint value)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglSurfaceAttrib);
	return _eglSurfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglBindTexImage);
	return _eglBindTexImage(dpy, surface, buffer);
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglReleaseTexImage);
	return _eglReleaseTexImage(dpy, surface, buffer);
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglSwapInterval);
	return _eglSwapInterval(dpy, interval);
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
			    EGLContext share_context,
			    const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCreateContext);
	return _eglCreateContext(dpy, config, share_context, attrib_list);
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglDestroyContext);
	return _eglDestroyContext(dpy, ctx);
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
			  EGLSurface read, EGLContext ctx)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglMakeCurrent);
	return _eglMakeCurrent(dpy, draw, read, ctx);
}

EGLContext eglGetCurrentContext(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetCurrentContext);
	return _eglGetCurrentContext();
}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetCurrentSurface);
	return _eglGetCurrentSurface(readdraw);
}

EGLDisplay eglGetCurrentDisplay(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglGetCurrentDisplay);
	return _eglGetCurrentDisplay();
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx,
			   EGLint attribute, EGLint * value)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglQueryContext);
	return _eglQueryContext(dpy, ctx, attribute, value);
}

EGLBoolean eglWaitGL(void)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglWaitGL);
	return _eglWaitGL();
}

EGLBoolean eglWaitNative(EGLint engine)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglWaitNative);
	return _eglWaitNative(engine);
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglSwapBuffers);
	return _eglSwapBuffers(dpy, surface);
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
			  EGLNativePixmapType target)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglCopyBuffers);
	return _eglCopyBuffers(dpy, surface, target);
}

static EGLImageKHR __eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx,
					 EGLenum target, EGLClientBuffer buffer,
					 const EGLint * attrib_list)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	if (!_eglCreateImageKHR) {
		/* we can't EGL_DLSYM this, because it doesn't exist in libEGL.
		 * IMG's libEGL. we also can't ask ourselves for the location of
		 * eglGetProcAddress, otherwise we'll end up calling ourselves again, so
		 * we must look up eglGetProcAddress first and ask SGX
		 */
		EGL_DLSYM(eglGetProcAddress);
		_eglCreateImageKHR = (void*)_eglGetProcAddress("eglCreateImageKHR");
	}

	/*
	 * We consider EGL_WAYLAND_BUFFER_WL to be EGL_ANTIVE_PIXMAP_KHR.
	 * This way IMG EGL will pass the request to underlying WSEGL.
	 */
	if (target == EGL_WAYLAND_BUFFER_WL) {
		EGL_DEBUG("%s: %s: mapping EGL_WAYLAND_BUFFER_WL to EGL_NATIVE_PIXMAP_KHR\n", __FILE__, __func__);
		target = EGL_NATIVE_PIXMAP_KHR;
	}

#if 0
	{
		int count = 0;
		EGLint *_attr, *_attr2, *head;
		
		_attr = attrib_list;
		while (*_attr != EGL_NONE) {
			count += 2;
			_attr++; _attr++;
		}
		count++;
		EGL_DEBUG("%s: %s: count=%d\n", __FILE__, __func__, count);
		head = _attr2 = alloca(sizeof(EGLint) * count);

		_attr = attrib_list;
		while (*_attr != EGL_NONE) {
			if (*_attr != EGL_WAYLAND_PLANE_WL) {
				EGL_DEBUG("%s: %s: copying %x\n", __FILE__, __func__, *_attr);
				*_attr2 = *_attr;
				_attr2++; _attr++;

				*_attr2 = *_attr;
				_attr2++; _attr++;
			} else {
				EGL_DEBUG("%s: %s: skipping %x\n", __FILE__, __func__, *_attr);
				_attr++; _attr++;
			}
		}
		*_attr2 = - EGL_NONE;
		attrib_list = head;
	}
#endif

	EGL_DEBUG("%s: %s (target=%d, EGL_WAYLAND_BUFFER_WL=%d, EGL_NATIVE_PIXMAP_KHR=%d) : %p\n", __FILE__, __func__, target, EGL_WAYLAND_BUFFER_WL, EGL_NATIVE_PIXMAP_KHR, _eglCreateImageKHR);
//	return _eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
	return _eglCreateImageKHR(dpy, ctx, target, buffer, NULL);
}

#ifdef WANT_WAYLAND
static struct wl_kms *__wl_kms = NULL;

static EGLBoolean __eglBindWaylandDisplayWL(EGLDisplay dpy,
					      struct wl_display *display)
{
	char *device_name = "/dev/dri/card0";
	int fd = -1;

	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	// TODO: create server side for wl_kms.
	if (__wl_kms)
		return EGL_FALSE;

	if (__gbm) {
		fd = gbm_device_get_fd(__gbm);
		device_name = _gbm_fd_get_device_name(fd);
	} else {
		fd = open(device_name, O_RDWR);
		if (fd < 0)
			return EGL_FALSE;
		__gbm = gbm_create_device(fd);
	}
	EGL_DEBUG("%s: decice_name=%s\n", __func__, device_name);

	__wl_kms = wayland_kms_init(display, __wl_display, device_name, fd);
	return EGL_TRUE;
}

static EGLBoolean __eglUnbindWaylandDisplayWL(EGLDisplay dpy,
						struct wl_display *display)
{
	// TODO: destroy wl_kms?
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	if (__wl_kms)
		wayland_kms_uninit(__wl_kms);

	return EGL_TRUE;
}

static EGLBoolean __eglQueryWaylandBufferWL(EGLDisplay dpy, struct wl_resource *buffer,
					      EGLint attribute, EGLint * value)
{
	int ret, val;
	enum wl_kms_attribute attr;
	EGLBoolean result = EGL_FALSE;

	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	switch (attribute) {
	case EGL_TEXTURE_FORMAT:
		attr = WL_KMS_TEXTURE_FORMAT;
		break;
	case EGL_WIDTH:
		attr = WL_KMS_WIDTH;
		break;
	case EGL_HEIGHT:
		attr = WL_KMS_HEIGHT;
		break;
	default:
		return EGL_FALSE;
	}
	
	ret = wayland_kms_query_buffer(__wl_kms, buffer, attr, &val);
	if (!ret) { 
		*value = (EGLint)val;
		result = EGL_TRUE;
	}

	return result;
}
#endif

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname)
{
	EGL_DLSYM(eglGetProcAddress);

	EGL_DEBUG("%s: %s(%s)\n", __FILE__, __func__, procname);
	if (strcmp(procname, "eglCreateImageKHR") == 0) {
		return (__eglMustCastToProperFunctionPointerType)__eglCreateImageKHR;
	}
#ifdef WANT_WAYLAND
	else if (strcmp(procname, "eglBindWaylandDisplayWL") == 0) {
		return (__eglMustCastToProperFunctionPointerType)__eglBindWaylandDisplayWL;
	} else if (strcmp(procname, "eglUnbindWaylandDisplayWL") == 0) {
		return (__eglMustCastToProperFunctionPointerType)__eglUnbindWaylandDisplayWL;
	} else if (strcmp(procname, "eglQueryWaylandBufferWL") == 0) {
		return (__eglMustCastToProperFunctionPointerType)__eglQueryWaylandBufferWL;
	}
#endif
	return _eglGetProcAddress(procname);
}

EGLBoolean eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image)
{
	EGL_DEBUG("%s: %s\n", __FILE__, __func__);
	EGL_DLSYM(eglDestroyImageKHR);
	return _eglDestroyImageKHR(dpy, image);
}
