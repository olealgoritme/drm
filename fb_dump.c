/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * Read back all the KMS framebuffers attached to the CRTC and record as PNG.
 */

#define _GNU_SOURCE
#include <cairo/cairo.h>
#include <drm/drm_gem.h>
#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/*#include "intel_io.h"
#include "drmtest.h"
*/

int main(int argc, char **argv) {

  (void)argc;
  drmModeResPtr res;
  int n;
  int fd = open("/dev/dri/card0", O_RDWR);
  if (fd < 0) {
    perror("Cannot open card");

    res = drmModeGetResources(fd);
    if (res == NULL)
      return ENOMEM;

    for (n = 0; n < res->count_crtcs; n++) {
      struct drm_gem_open open_arg;
      struct drm_gem_flink flink;
      drmModeCrtcPtr crtc;
      drmModeFBPtr fb;

      crtc = drmModeGetCrtc(fd, res->crtcs[n]);
      if (crtc == NULL)
        continue;

      fb = drmModeGetFB(fd, crtc->buffer_id);
      drmModeFreeCrtc(crtc);
      if (fb == NULL)
        continue;

      flink.handle = fb->handle;
      if (drmIoctl(fd, DRM_IOCTL_GEM_FLINK, &flink)) {
        drmModeFreeFB(fb);
        continue;
      }

      open_arg.name = flink.name;
      if (drmIoctl(fd, DRM_IOCTL_GEM_OPEN, &open_arg) == 0) {
        struct drm_gem_mmap mmap_arg;
        void *ptr;

        /* prepare buffer for memory mapping */
        memset(&mmap_arg, 0, sizeof(mmap_arg));

        mmap_arg.handle = open_arg.handle;
        if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg) == 0 &&
            (ptr = mmap(0, open_arg.size, PROT_READ, MAP_SHARED, fd,
                        mmap_arg.offset)) != (void *)-1) {
          cairo_surface_t *surface;
          cairo_format_t format;
          char name[80];

          snprintf(name, sizeof(name), "fb-%d.png", fb->fb_id);

          switch (fb->depth) {
          case 16:
            format = CAIRO_FORMAT_RGB16_565;
            break;
          case 24:
            format = CAIRO_FORMAT_RGB24;
            break;
          case 30:
            format = CAIRO_FORMAT_RGB30;
            break;
          case 32:
            format = CAIRO_FORMAT_ARGB32;
            break;
          default:
            format = CAIRO_FORMAT_INVALID;
            break;
          }

          surface = cairo_image_surface_create_for_data(ptr, format, fb->width,
                                                        fb->height, fb->pitch);
          cairo_surface_write_to_png(surface, name);
          cairo_surface_destroy(surface);

          munmap(ptr, open_arg.size);
        }
        drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &open_arg.handle);
      }

      drmModeFreeFB(fb);
    }

    return 0;
  }
