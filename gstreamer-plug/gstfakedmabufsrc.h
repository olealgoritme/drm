/* fakedmabufsrc
 *
 * Copyright 2018: 
 *   Sreerenj Balachandran <sreerenjb@gnome.org/sreerenj.balachandran@intel.com>
 *   Philippuse, Philippe <.Lecluse@intel.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_FAKEDMABUFSRC_H_
#define _GST_FAKEDMABUFSRC_H_

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/allocators/gstdmabuf.h>

G_BEGIN_DECLS

#define GST_TYPE_FAKEDMABUFSRC   (gst_fakedmabufsrc_get_type())
#define GST_FAKEDMABUFSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FAKEDMABUFSRC,GstFakeDmabufSrc))
#define GST_FAKEDMABUFSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FAKEDMABUFSRC,GstFakeDmabufSrcClass))
#define GST_IS_FAKEDMABUFSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FAKEDMABUFSRC))
#define GST_IS_FAKEDMABUFSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FAKEDMABUFSRC))

typedef struct _GstFakeDmabufSrc GstFakeDmabufSrc;
typedef struct _GstFakeDmabufSrcClass GstFakeDmabufSrcClass;

struct _GstFakeDmabufSrc
{
  GstPushSrc base_fakedmabufsrc;
  GstVideoInfo info;
  int io_mode;
  int fd;
  GstAllocator* allocator;

};

struct _GstFakeDmabufSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_fakedmabufsrc_get_type (void);

G_END_DECLS

#endif
