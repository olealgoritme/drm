lib_LTLIBRARIES = libgstfakedmabufsrc.la

noinst_HEADERS = gstfakedmabufsrc.h

libgstfakedmabufsrc_la_SOURCES = gstfakedmabufsrc.c

libgstfakedmabufsrc_la_CFLAGS = \
	$(GST_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_VIDEO_CFLAGS) \
	$(GST_ALLOCATORS_CFLAGS) \
	$(KMS_DRM_CFLAGS) \
	$(NULL)

libgstfakedmabufsrc_la_LIBADD = \
	$(GST_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_VIDEO_LIBS) \
	$(GST_ALLOCATORS_LIBS) \
	$(KMS_DRM_LIBS) \
	$(NULL)

libdir = ${exec_prefix}/lib/gstreamer-1.0
