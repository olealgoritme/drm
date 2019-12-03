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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define OUTPUT_WIDTH 1920
#define OUTPUT_HEIGHT 540

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

// 3840 x 1080 x 4 x 2 = ~33MB
#define SH_MEM_SIZE 33177600
#define MAX_FBS 16


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

void runEGL(const DmaBuf *dmaBuf) {

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
 EGLAttrib eimg_attrs[] = {
     EGL_WIDTH, dmaBuf->width, 
     EGL_HEIGHT, dmaBuf->height, 
     EGL_LINUX_DRM_FOURCC_EXT, dmaBuf->fourcc, 
     EGL_DMA_BUF_PLANE0_FD_EXT, dmaBuf->fd, 
     EGL_DMA_BUF_PLANE0_OFFSET_EXT, dmaBuf->offset, 
     EGL_DMA_BUF_PLANE0_PITCH_EXT, dmaBuf->pitch, 
     EGL_NONE};

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
  
    /*
     *GLuint fb = 0;
     *glGenFramebuffers(0, &fb);
     *glBindFramebuffer(GL_FRAMEBUFFER, fb);
     *printf("%d\n", fb); 
     */
    
    
    while (XPending(xdisp)) {
      XEvent e;
      XNextEvent(xdisp, &e);
      switch (e.type) {
      case KeyPress:
        switch (XLookupKeysym(&e.xkey, 0)) {
        case XK_Escape:
        case XK_q:
          eglTerminate(eglDpy);
          return;
        }
      }
    }

}

  // 7. Terminate EGL when finished
  eglTerminate(eglDpy);

}

int capture_to_ppm(void)
{
	int width, height, colorDepth, maxColorValue,y;
	unsigned char	*pixels;
	int fd;
	char sbuf[256]; /* for sprintf() */

	/* open output file: you can name it as you like */
	fd = open(picfile,O_CREAT|O_TRUNC|O_WRONLY,
					  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if(fd == -1) return -1;

	/* width & height of the window */
	width = glutGet(GLUT_WINDOW_WIDTH);
	height = glutGet(GLUT_WINDOW_HEIGHT);

	/* maxColorValue is 255 in most cases... */
	colorDepth = glutGet(GLUT_WINDOW_RED_SIZE);
	maxColorValue = (1 << colorDepth) - 1;

	/* allocate pixels[]: 3 is for RGB */
	pixels = malloc(3*width*height);
	if( !pixels ) return -2;

	/* get RGB values from the frame buffer into pixels[] */
	glReadBuffer(GL_FRONT); /* if you are using "double buffer" */
	glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels);

	/* write ppm file header */
	sprintf(sbuf,"P6 %d %d %d\n",width,height,maxColorValue);
	write(fd,sbuf,strlen(sbuf));

	/* write ppm RGB data: we must invert upside down */
	for(y = height-1; y >= 0; --y) {
		write(fd, pixels+3*width*y, 3*width);
	}

	close(fd);
	free(pixels);
	return 0;
}



int main(int argc, const char *argv[]) {

	if (argc < 2) {
		MSG("Usage: %s socket_filename", argv[0]);
		return 1;
	}

	const char *sockname = argv[1];

	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	{
		struct sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		if (strlen(sockname) >= sizeof(addr.sun_path)) {
			MSG("Socket filename '%s' is too long, max %d",
				sockname, (int)sizeof(addr.sun_path));
			goto cleanup;
		}

		strcpy(addr.sun_path, sockname);
		if (-1 == connect(sockfd, (const struct sockaddr*)&addr, sizeof(addr))) {
			perror("Cannot connect to unix socket");
			goto cleanup;
		}

		MSG("connected");
	}

	DmaBuf img = {0};

	{
		struct msghdr msg = {0};

		struct iovec io = {
			.iov_base = &img,
			.iov_len = sizeof(img),
		};
		msg.msg_iov = &io;
		msg.msg_iovlen = 1;

		char cmsg_buf[CMSG_SPACE(sizeof(img.fd))];
		msg.msg_control = cmsg_buf;
		msg.msg_controllen = sizeof(cmsg_buf);
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(img.fd));

		MSG("recvmsg");
		ssize_t recvd = recvmsg(sockfd, &msg, 0);
		if (recvd <= 0) {
			perror("cannot recvmsg");
			goto cleanup;
		}

		MSG("Received %d", (int)recvd);

		if (io.iov_len == sizeof(img) - sizeof(img.fd)) {
			MSG("Received metadata size mismatch: %d received, %d expected",
				(int)io.iov_len, (int)sizeof(img) - (int)sizeof(img.fd));
			goto cleanup;
		}

		if (cmsg->cmsg_len != CMSG_LEN(sizeof(img.fd))) {
			MSG("Received fd size mismatch: %d received, %d expected",
				(int)cmsg->cmsg_len, (int)CMSG_LEN(sizeof(img.fd)));
			goto cleanup;
		}

		memcpy(&img.fd, CMSG_DATA(cmsg), sizeof(img.fd));
	}

	close(sockfd);
	sockfd = -1;

	MSG("Received width=%d height=%d pitch=%u fourcc=%#x fd=%d",
		img.width, img.height, img.pitch, img.fourcc, img.fd);

	runEGL(&img);

cleanup:
	if (sockfd >= 0)
		close(sockfd);
	return 0;

}
