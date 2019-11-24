/* fakedmabufsrc
 *
 * Copyright 2018
 * Authors: 
 *   Sreerenj Balachandran <sreerenjb@gnome.org/sreernej.balachandran@intel.com>
 *   Lecluse, Philippe <philippe.lecluse@intel.com>
 *   Hatcher, Philip <philip.hatcher@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-plugin
 *
 * The fakedmabufsrc element is for dumping dmabuf buffers
 *
 * <refsect2>
 * <title>Force this</title>
 * |[
 * gst-launch-1.0 -v fakedmabufsrc ! "video/x-raw,format=BGRx,width=3840,height=1080" ! vaapipostproc format=nv12 !  vaapih264enc tune=low-power ! filesink location=sample.h264
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstfakedmabufsrc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <drm/drm.h>
#include <libdrm/drm_fourcc.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#ifndef DRM_RDWR
#define DRM_RDWR O_RDWR
#endif

#define GST_FAKEDMABUFSRC_FORMATS "{ BGRx, RGBx }"

static const char gst_fakedmabuf_src_caps_str[] = "video/x-raw,"
    "format = " GST_FAKEDMABUFSRC_FORMATS ","
    "framerate = (fraction)[0/1, 2147483647/1],"
    "width = (int)[1, 2147483647]," "height = (int)[1, 2147483647]";

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_fakedmabuf_src_caps_str));

enum
{
  GST_FAKEDMABUFSRC_IO_MODE_DMABUF_EXPORT = 1,
  GST_FAKEDMABUFSRC_IO_MODE_DMABUF_IMPORT = 2,
};

#define GST_FAKEDMABUFSRC_IO_MODE_TYPE (gst_fakedmabufsrc_io_mode_get_type())
static GType
gst_fakedmabufsrc_io_mode_get_type (void)
{
  static GType type = 0;

  static const GEnumValue values[] = {
    {GST_FAKEDMABUFSRC_IO_MODE_DMABUF_EXPORT, "Export dmabuf", "dmabuf-export"},
    {GST_FAKEDMABUFSRC_IO_MODE_DMABUF_IMPORT, "Import dmabuf", "dmabuf-import"},
    {0, NULL, NULL}
  };

  if (!type) {
    type = g_enum_register_static ("GstFakeDmabufIOMode", values);
  }
  return type;
}

extern int drmModeAddFB (int fd, uint32_t width, uint32_t height, uint8_t depth,
    uint8_t bpp, uint32_t pitch, uint32_t bo_handle, uint32_t * buf_id);
int drmIoctl (int fd, unsigned long request, void *arg);
int
drmIoctl (int fd, unsigned long request, void *arg)
{
  int ret;
  do {
    ret = ioctl (fd, request, arg);
  } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
  return ret;
}

GST_DEBUG_CATEGORY_STATIC (gst_fakedmabufsrc_debug);
#define GST_CAT_DEFAULT gst_fakedmabufsrc_debug

enum
{
  PROP_0,
  PROP_IO_MODE,
  PROP_LAST
};

static void gst_fakedmabufsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_fakedmabufsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_fakedmabufsrc_start (GstBaseSrc * src);
static gboolean gst_fakedmabufsrc_stop (GstBaseSrc * src);
static GstFlowReturn gst_fakedmabufsrc_create (GstPushSrc * psrc,
    GstBuffer ** outbuf);
static gboolean gst_fakedmabufsrc_is_seekable (GstBaseSrc * src);
static gboolean gst_fakedmabufsrc_setcaps (GstBaseSrc * bsrc, GstCaps * caps);
static void gst_fakedmabufsrc_finalize (GObject * object);

#define _do_init \
	  GST_DEBUG_CATEGORY_INIT (gst_fakedmabufsrc_debug, "fakedmabufsrc", 0, "fakedmabufsrc element");
#define gst_fakedmabufsrc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstFakeDmabufSrc, gst_fakedmabufsrc, GST_TYPE_PUSH_SRC,
    _do_init);


static void
gst_fakedmabufsrc_class_init (GstFakeDmabufSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpush_src_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpush_src_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_fakedmabufsrc_set_property;
  gobject_class->get_property = gst_fakedmabufsrc_get_property;
  gobject_class->finalize = gst_fakedmabufsrc_finalize;

  g_object_class_install_property (gobject_class, PROP_IO_MODE,
      g_param_spec_enum ("io-mode", "io-mode",
          "I/O mode ", gst_fakedmabufsrc_io_mode_get_type (),
          GST_FAKEDMABUFSRC_IO_MODE_DMABUF_EXPORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Fake Dmabuf source", "Source/Video",
      "dump dmabuf buffers",
      "Sreerenj Balachandran <sreerenj.balachandran@intel.com>,"
      "Lecluse, Philippe <philippe.lecluse@intel.com>,"
      "Hatcher, Philip <philip.hatcher@intel.com>");

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &srctemplate);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_fakedmabufsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_fakedmabufsrc_stop);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_fakedmabufsrc_is_seekable);
  gstpush_src_class->create = GST_DEBUG_FUNCPTR (gst_fakedmabufsrc_create);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_fakedmabufsrc_setcaps);
}

static void
gst_fakedmabufsrc_init (GstFakeDmabufSrc * fakedmabufsrc)
{
  fakedmabufsrc->io_mode = GST_FAKEDMABUFSRC_IO_MODE_DMABUF_EXPORT;
  gst_video_info_init (&fakedmabufsrc->info);
  fakedmabufsrc->allocator = gst_dmabuf_allocator_new ();
}

void
gst_fakedmabufsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (object);

  GST_DEBUG_OBJECT (fakedmabufsrc, "set_property");

  switch (property_id) {
    case PROP_IO_MODE:
      fakedmabufsrc->io_mode = g_value_get_enum (value);
      if (fakedmabufsrc->io_mode != GST_FAKEDMABUFSRC_IO_MODE_DMABUF_EXPORT) {
        GST_WARNING_OBJECT (fakedmabufsrc,
            "dmabuf export is the only supported mode!");
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_fakedmabufsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (object);

  GST_DEBUG_OBJECT (fakedmabufsrc, "get_property");

  switch (property_id) {
    case PROP_IO_MODE:
      g_value_set_enum (value, fakedmabufsrc->io_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_fakedmabufsrc_start (GstBaseSrc * src)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (src);

  GST_DEBUG_OBJECT (fakedmabufsrc, "start");

  if (!fakedmabufsrc->allocator) {
    GST_ERROR_OBJECT (fakedmabufsrc, "No gst-dmabuf Allocator!!");
    return FALSE;
  }

  fakedmabufsrc->fd = open ("/dev/dri/card0", O_RDWR);
  if (fakedmabufsrc->fd < 0) {
    GST_ERROR ("Couldn't open /dev/dri/card0");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_fakedmabufsrc_stop (GstBaseSrc * src)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (src);

  GST_DEBUG_OBJECT (fakedmabufsrc, "stop");

  if (fakedmabufsrc->fd)
    close (fakedmabufsrc->fd);

  //PPP -- Need to free up some stuff here??
  return TRUE;
}

static gboolean
gst_fakedmabufsrc_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (bsrc);
  GstStructure *structure;
  GstVideoInfo info;

  structure = gst_caps_get_structure (caps, 0);
  GST_DEBUG_OBJECT (fakedmabufsrc, "Getting currently set caps%" GST_PTR_FORMAT,
      caps);
  if (gst_structure_has_name (structure, "video/x-raw")) {
    /* we can use the parsing code */
    if (!gst_video_info_from_caps (&info, caps))
      goto parse_failed;
  }

  fakedmabufsrc->info = info;

  return TRUE;

  /* ERRORS */
parse_failed:
  {
    GST_DEBUG_OBJECT (bsrc, "failed to parse caps dickhead!");
    return FALSE;
  }

}

static gboolean
gst_fakedmabufsrc_is_seekable (GstBaseSrc * basesrc)
{
  return FALSE;
}

static void
gst_fakedmabufsrc_finalize (GObject * object)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (object);
  if (fakedmabufsrc->allocator)
    gst_object_unref (fakedmabufsrc->allocator);
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_fakedmabufsrc_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstFakeDmabufSrc *fakedmabufsrc = GST_FAKEDMABUFSRC (psrc);
  GstVideoInfo *info = &fakedmabufsrc->info;
  gsize offset[GST_VIDEO_MAX_PLANES] = { 0, };
  gint stride[GST_VIDEO_MAX_PLANES] = { 0, };
  gint aligned_stride = 0;

  GST_DEBUG_OBJECT (fakedmabufsrc, "create");

  if (GST_VIDEO_INFO_FORMAT (info) != GST_VIDEO_FORMAT_RGBx &&
      GST_VIDEO_INFO_FORMAT (info) != GST_VIDEO_FORMAT_BGRx) {
    GST_ERROR_OBJECT (fakedmabufsrc, "Unsupported video format dickhead!");
    return GST_FLOW_ERROR;
  }

  /* Should handle this in a better way, outside of create() ! */
  aligned_stride = GST_ROUND_UP_32 (info->width) * 4;
  stride[0] = aligned_stride;

  {
    GstMemory *myMem;
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    struct drm_prime_handle preq;
    uint32_t fb;
    int ret;
    void *map;

    /* create dumb buffer */
    memset (&creq, 0, sizeof (creq));
    creq.width = info->width;
    creq.height = info->height;
    creq.bpp = 32;

    ret = drmIoctl (fakedmabufsrc->fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
      GST_ERROR_OBJECT (fakedmabufsrc, "DRM Create Dumb failed ");
      return GST_FLOW_ERROR;
      /* buffer creation failed; see "errno" for more error codes */
    }
    /* creq.pitch, creq.handle and creq.size are filled by this ioctl with
     * the requested values and can be used now. */

    /* create framebuffer object for the dumb-buffer */
    ret =
        drmModeAddFB (fakedmabufsrc->fd, info->width, info->height, 24, 32,
        creq.pitch, creq.handle, &fb);
    if (ret) {
      /* frame buffer creation failed; see "errno" */
      GST_ERROR_OBJECT (fakedmabufsrc, "AddfD failed");
      return GST_FLOW_ERROR;
    }
    /* the framebuffer "fb" can now used for scanout with KMS */

    /* prepare buffer for memory mapping */
    memset (&mreq, 0x00, sizeof (mreq));
    mreq.handle = creq.handle;
    ret = drmIoctl (fakedmabufsrc->fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
      /* DRM buffer preparation failed; see "errno" */
      GST_ERROR_OBJECT (fakedmabufsrc, "drm map_dumb failed\n");
      return GST_FLOW_ERROR;
    }
    /* mreq.offset now contains the new offset that can be used with mmap() */

    /* perform actual memory mapping */
    map =
        mmap (0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED,
        fakedmabufsrc->fd, mreq.offset);
    if (map == MAP_FAILED) {
      /* memory-mapping failed; see "errno" */
      GST_ERROR_OBJECT (fakedmabufsrc, "mapping failed");
      return GST_FLOW_ERROR;
    }

    /* clear the framebuffer to 0 */
    memset (map, 0xFF, creq.size);
    munmap (map, creq.size);

    preq.handle = mreq.handle;
    preq.flags = DRM_CLOEXEC | DRM_RDWR;

    ret = drmIoctl (fakedmabufsrc->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &preq);
    if (ret < 0) {
      /* buffer creation failed; see "errno" for more error codes */
      GST_ERROR_OBJECT (fakedmabufsrc, "PRIME_HANDLE_TO_FD Failed");
      return GST_FLOW_ERROR;
    } else

      printf("FD is %d (s= 0x%x)\n", preq.fd, (gint) creq.size);
        GST_DEBUG_OBJECT (fakedmabufsrc, "FD is %d (s= 0x%x)\n", preq.fd,
          (gint) creq.size);

    myMem =
        gst_dmabuf_allocator_alloc (fakedmabufsrc->allocator, preq.fd,
        creq.size);
    *outbuf = gst_buffer_new ();
    gst_buffer_append_memory (*outbuf, myMem);
    gst_buffer_add_video_meta_full (*outbuf, 0, GST_VIDEO_INFO_FORMAT (info),
        info->width, info->height, 1, offset, stride);
  }


  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "fakedmabufsrc", GST_RANK_NONE,
      GST_TYPE_FAKEDMABUFSRC);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "fakedmabufsrc"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "fakedmabufsrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "sreerenjb@gnome.org"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    fakedmabufsrc,
    "Zero copy VM display scraping",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
