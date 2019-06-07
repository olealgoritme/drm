#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/Xlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <cairo/cairo.h>
#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define ASSERT(cond)                                                           \
  if (!(cond)) {                                                               \
    MSG("ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond);              \
    return;                                                                    \
  }

/*
 * https://devblogs.nvidia.com/egl-eye-opengl-visualization-without-x-server/
 */

static int width = 1920, height = 540;
// static int width = 854, height = 240;

typedef struct {
  int width, height;
  uint32_t fourcc;
  int offset, pitch;
  int fd;
} DmaBuf;

static const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                       EGL_PBUFFER_BIT,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_RED_SIZE,
                                       8,
                                       EGL_DEPTH_SIZE,
                                       8,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_BIT,
                                       EGL_NONE};

int main(int argc, const char *argv[]) {

  (void)argc;
  //(void)argv;
  // char *str_fb_id = "0x3d";

  DmaBuf *dmaBuf;
  dmaBuf = (DmaBuf *)malloc(sizeof(DmaBuf));

  uint32_t fb_id;
  int drm_fd;
  int dma_buf_fd;
  const char *card;
  drmModeFBPtr fb;

  // get DRM prime fd

  // step1: read card
  // step2: set/guess fb id
  // step3: test fb
  // step4: get fb handle

  card = "/dev/dri/card0";
  drm_fd = open(card, O_RDONLY);

  MSG("Opening card %s", card);
  if (drm_fd < 0) {
    perror("Cannot open card");
    exit(-1);
  }

  MSG("Trying fb id: %s", argv[1]);
  fb_id = strtol(argv[1], NULL, 0);
  fb = drmModeGetFB(drm_fd, fb_id);

  if (!fb) {
    MSG("Cant open fb id: %#x", fb_id);
  }

  MSG("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
      fb->fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth,
      fb->handle);

  if (!fb->handle) {
    MSG("Can't get framebuffer handle. Run either with sudo, or put user in "
        "video group");
    exit(-1);
  }

  int ret = drmPrimeHandleToFD(drm_fd, fb->handle, 0, &dma_buf_fd);

  MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);

  dmaBuf->width = fb->width;
  dmaBuf->height = fb->height;
  dmaBuf->pitch = fb->pitch;
  dmaBuf->offset = 0;
  dmaBuf->fourcc = DRM_FORMAT_XRGB8888; // FIXME
  dmaBuf->fd = dma_buf_fd;

  // CREATE EGL CONTEXT WITHOUT DISPLAY (just to get context)

  // 1. Initialize EGL
  // CREATE EGL CONTEXT WITHOUT DISPLAY (just to get context)
  // EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  Display *xdisp;
  xdisp = XOpenDisplay(NULL);
  EGLDisplay eglDpy = eglGetDisplay(xdisp);

  EGLint major, minor;
  eglInitialize(eglDpy, &major, &minor);

  // 2. Select an appropriate configuration
  EGLint numConfigs;
  EGLConfig eglCfg;

  eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

  // 3. Create a surface and window
  // EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg,
  // pbufferAttribs);

  XVisualInfo *vinfo = NULL;
  XVisualInfo xvisual_info = {0};
  int num_visuals;
  eglGetConfigAttrib(eglDpy, eglCfg, EGL_NATIVE_VISUAL_ID,
                     (EGLint *)&xvisual_info.visualid);
  vinfo = XGetVisualInfo(xdisp, VisualScreenMask | VisualIDMask, &xvisual_info,
                         &num_visuals);
  XSetWindowAttributes winattrs = {0};
  winattrs.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                        ButtonReleaseMask | PointerMotionMask | ExposureMask |
                        VisibilityChangeMask | StructureNotifyMask;
  winattrs.border_pixel = 0;
  winattrs.bit_gravity = StaticGravity;
  winattrs.colormap = XCreateColormap(xdisp, RootWindow(xdisp, vinfo->screen),
                                      vinfo->visual, AllocNone);
  winattrs.override_redirect = False;
  Window xwin = XCreateWindow(
      xdisp, RootWindow(xdisp, vinfo->screen), 0, 0, width, height, 0,
      vinfo->depth, InputOutput, vinfo->visual,
      CWBorderPixel | CWBitGravity | CWEventMask | CWColormap, &winattrs);
  XMapWindow(xdisp, xwin);
  EGLSurface eglSurf = eglCreateWindowSurface(eglDpy, eglCfg, xwin, 0);

  // 4. Bind the API
  eglBindAPI(EGL_OPENGL_API);

  // 5. Create a context and make it current
  EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);
  eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

  // 6. Use your OpenGL context

  // TODO check for EGL_EXT_image_dma_buf_import
  EGLAttrib eimg_attrs[] = {EGL_WIDTH,
                            dmaBuf->width,
                            EGL_HEIGHT,
                            dmaBuf->height,
                            EGL_LINUX_DRM_FOURCC_EXT,
                            dmaBuf->fourcc,
                            EGL_DMA_BUF_PLANE0_FD_EXT,
                            dmaBuf->fd,
                            EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                            dmaBuf->offset,
                            EGL_DMA_BUF_PLANE0_PITCH_EXT,
                            dmaBuf->pitch,
                            EGL_NONE};
  EGLImage eimg = eglCreateImage(eglDpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                 0, eimg_attrs);

  GLuint texture = 1;
  glBindTexture(GL_TEXTURE_2D, texture);
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eimg);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  const char *fragment = "#version 130\n"
                         "uniform vec2 res;\n"
                         "uniform sampler2D tex;\n"
                         "void main() {\n"
                         "vec2 uv = gl_FragCoord.xy / res;\n"
                         "uv.y = 1. - uv.y;\n"
                         "gl_FragColor = texture(tex, uv);\n"
                         "}\n";
  int prog = ((PFNGLCREATESHADERPROGRAMVPROC)(eglGetProcAddress(
      "glCreateShaderProgramv")))(GL_FRAGMENT_SHADER, 1, &fragment);
  glUseProgram(prog);
  glUniform1i(glGetUniformLocation(prog, "tex"), 0);

  for (;;) {
    while (XPending(xdisp)) {
      XEvent e;
      XNextEvent(xdisp, &e);
      switch (e.type) {
      case ConfigureNotify: {
        width = e.xconfigure.width;
        height = e.xconfigure.height;
      } break;

      case KeyPress:
        switch (XLookupKeysym(&e.xkey, 0)) {
        case XK_Escape:
        case XK_q:
          break;
        }
        break;

      case ClientMessage:
      case DestroyNotify:
      case UnmapNotify:
        break;
      }
    }

    {
      glViewport(0, 0, width, height);
      glClear(GL_COLOR_BUFFER_BIT);

      glUniform2f(glGetUniformLocation(prog, "res"), width, height);
      glRects(-1, -1, 1, 1);

      eglSwapBuffers(eglDpy, eglSurf);
    }
  }

  // 7. Terminate EGL when finished
  // eglTerminate(eglDpy);
  return 0;
}
