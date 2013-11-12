lib_LTLIBRARIES = libEGL.la

libEGL_la_SOURCES =	\
	egl.c

libEGL_la_LIBADD =	\
	@LIBGBM_LIBS@ \
	@WAYLAND_KMS_LIBS@


libEGL_la_CFLAGS =	\
	-I$(top_srcdir)/include \
	@LIBGBM_CFLAGS@ \
	@WAYLAND_KMS_CFLAGS@

libEGL_la_LDFLAGS = -version-info 1:0:0

egldir = $(includedir)/EGL
egl_HEADERS = \
	$(top_srcdir)/include/EGL/eglmesaext.h \
	$(top_srcdir)/include/EGL/egl.h \
	$(top_srcdir)/include/EGL/eglplatform.h \
	$(top_srcdir)/include/EGL/eglext.h

khrdir = $(includedir)/KHR
khr_HEADERS = \
	$(top_srcdir)/include/KHR/khrplatform.h

gles2dir = $(includedir)/GLES2
gles2_HEADERS = \
	$(top_srcdir)/include/GLES2/gl2ext.h \
	$(top_srcdir)/include/GLES2/gl2platform.h \
	$(top_srcdir)/include/GLES2/gl2.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = egl.pc glesv2.pc
