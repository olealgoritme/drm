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
#include <X11/Xlib.h>


#define OUTPUT_WIDTH 1920
#define OUTPUT_HEIGHT 540

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define MAX_FBS 16

#define ASSERT(cond)                                                           \
  if (!(cond)) {                                                               \
    MSG("ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond);              \
    return;                                                                    \
  }

typedef struct {
  int width, height;
  uint32_t fourcc;
  int offset, pitch;
  int fd;
} DmaBuf;


int getScreenSize(int *w, int *h) {

 Display* dsp;
 Screen* scr;

 dsp = XOpenDisplay(NULL);
 if (!dsp) {
  fprintf(stderr, "Failed to open default display.\n");
  return -1;
 }

 scr = DefaultScreenOfDisplay(dsp);
 if (!scr) {
  fprintf(stderr, "Failed to obtain the default screen of given display.\n");
  return -1;
 }

 *w = scr->width;
 *h = scr->height;
 XCloseDisplay(dsp);
 return 0;
}

uint32_t getFbId(int scrW, int scrH) {

    uint32_t magicFbId;

    
	const int available = drmAvailable();
	if (!available) {
        MSG("DRM NOT AVAILABLE!\n");
        exit(-1);
    }

	const char *card = "/dev/dri/card0";
	const int fd = open(card, O_RDONLY);
    if (fd < 0) {
	    MSG("Can't open: %s", card);
        exit(-1);
    }

	drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	drmModeResPtr res = drmModeGetResources(fd);
	if (res) {
		drmModeFreeResources(res);
	}

	uint32_t fbs[MAX_FBS];
	int count_fbs = 0;

	drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
	if (planes) {
		for (uint32_t i = 0; i < planes->count_planes; ++i) {
			MSG("Possible framebuffer ID: %#x", planes->planes[i]);
			drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
			if (plane) {
				for (uint32_t j = 0; j < plane->count_formats; ++j) {
					const uint32_t f = plane->formats[j];
				}

				if (plane->fb_id) {
					int found = 0;
					for (int k = 0; k < count_fbs; ++k) {
						if (fbs[k] == plane->fb_id) {
							found = 1;
							break;
						}
					}

					if (!found) {
						if (count_fbs == MAX_FBS) {
							MSG("Max number of fbs (%d) exceeded", MAX_FBS);
						} else {
							fbs[count_fbs++] = plane->fb_id;
						}
					}
				}
				drmModeFreePlane(plane);
			}
		}
		drmModeFreePlaneResources(planes);
	}

	for (int i = 0; i < count_fbs; ++i) {
		drmModeFBPtr fb = drmModeGetFB(fd, fbs[i]);
		if (!fb) {
			continue;
		}

        if(scrW == fb->width && scrH == fb->height) {
           MSG("Guessing FB ID: %#x (same resolution as screen) %dx%d", fbs[i], scrW, scrH);
            magicFbId = fbs[i];
            return magicFbId;
        }
		drmModeFreeFB(fb);
	}

	close(fd);
    return 0;
}


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


int main() {

  DmaBuf *dmaBuf;
  dmaBuf = (DmaBuf *)malloc(sizeof(DmaBuf));

  // match fb_id with screen width and height
  uint32_t fb_id;
  int scrW, scrH;
  
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

  // Trying to match screen size with fbid
  getScreenSize(&scrW, &scrH);
  fb_id = getFbId(scrW, scrH);
  MSG("Trying fb id: %#x", fb_id);
  
  fb = drmModeGetFB(drm_fd, fb_id);

  if (!fb) {
    MSG("Cant open fb id: %#x", fb_id);
  }

  if (!fb->handle) {
    MSG("Can't get framebuffer handle. Run either with sudo, or put user in "
        "video group");
    exit(-1);
  }

  int ret = drmPrimeHandleToFD(drm_fd, fb->handle, 0, &dma_buf_fd);

  dmaBuf->width = fb->width;
  dmaBuf->height = fb->height;
  dmaBuf->pitch = fb->pitch;
  dmaBuf->offset = 0;
  dmaBuf->fourcc = DRM_FORMAT_XRGB8888;
  dmaBuf->fd = dma_buf_fd;

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

  // Bind egl surface to window
  XVisualInfo *vinfo = NULL;
  XVisualInfo xvisual_info = {0};
  int num_visuals;
  eglGetConfigAttrib(eglDpy, eglCfg, EGL_NATIVE_VISUAL_ID, (EGLint *)&xvisual_info.visualid);
  vinfo = XGetVisualInfo(xdisp, VisualScreenMask | VisualIDMask, &xvisual_info, &num_visuals);
  XSetWindowAttributes winattrs = {0};
  winattrs.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | VisibilityChangeMask | StructureNotifyMask;
  winattrs.border_pixel = 0;
  winattrs.bit_gravity = StaticGravity;
  winattrs.colormap = XCreateColormap(xdisp, RootWindow(xdisp, vinfo->screen), vinfo->visual, AllocNone);
  winattrs.override_redirect = False;
  Window xwin = XCreateWindow(xdisp, RootWindow(xdisp, vinfo->screen), 0, 0, OUTPUT_WIDTH, OUTPUT_HEIGHT, 0, vinfo->depth, InputOutput, vinfo->visual, CWBorderPixel | CWBitGravity | CWEventMask | CWColormap, &winattrs);
  XMapWindow(xdisp, xwin);
  EGLSurface eglSurf = eglCreateWindowSurface(eglDpy, eglCfg, xwin, 0);

  // 4. Bind the API
  eglBindAPI(EGL_OPENGL_API);

  // 5. Create a context and make it current
  EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);
  eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

  // 6. Use OpenGL context
 EGLAttrib eimg_attrs[] = {EGL_WIDTH, dmaBuf->width, EGL_HEIGHT, dmaBuf->height, EGL_LINUX_DRM_FOURCC_EXT, dmaBuf->fourcc, EGL_DMA_BUF_PLANE0_FD_EXT, dmaBuf->fd, EGL_DMA_BUF_PLANE0_OFFSET_EXT, dmaBuf->offset, EGL_DMA_BUF_PLANE0_PITCH_EXT, dmaBuf->pitch, EGL_NONE};
 EGLImage eimg = eglCreateImage(eglDpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, eimg_attrs);

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
   int prog = ( (PFNGLCREATESHADERPROGRAMVPROC) (eglGetProcAddress("glCreateShaderProgramv"))) (GL_FRAGMENT_SHADER, 1, &fragment);
   glUseProgram(prog);
   glUniform1i(glGetUniformLocation(prog, "tex"), 0);
   

  for (;;) {

    glViewport(0, 0, OUTPUT_WIDTH, OUTPUT_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniform2f(glGetUniformLocation(prog, "res"), OUTPUT_WIDTH, OUTPUT_HEIGHT);
    glRects(-1, -1, 1, 1);
    eglSwapBuffers(eglDpy, eglSurf);

    while (XPending(xdisp)) {
      XEvent e;
      XNextEvent(xdisp, &e);
      switch (e.type) {
      case KeyPress:
        switch (XLookupKeysym(&e.xkey, 0)) {
        case XK_Escape:
        case XK_q:
          eglTerminate(eglDpy);
          return 0;
        }
      }
    }

}

  // 7. Terminate EGL when finished
  eglTerminate(eglDpy);
  return 0;
}
