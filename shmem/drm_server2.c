#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <X11/Xlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define GL_GLEXT_PROTOTYPES
// 3840 x 1080 x 4 x 2 = ~33MB
#define SH_MEM_SIZE 33177600

// chunk sizes
#define CHUNK_SIZE 1024

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define MAX_FBS 16

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

int main(int argc, char *argv[]) {

    uint32_t fb_id;
    int scrW, scrH;
    int shmID;
    int drm_fd;
    int dma_buf_fd;

    int sockfd;
    const char *sockname = argv[1];

    char *shmdev = "/drmxuw";
    void *drm_ptr;
    void *sh_ptr;

    const char *card = "/dev/dri/card0";
    drmModeFBPtr fb;

    // Process ID
    pid_t pid;

    // Get process ID
    pid = getpid();

    drm_fd = open(card, O_RDWR); // O_RDONLY | O_RDWR

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
        goto cleanup;
    }

    if (!fb->handle) {
        MSG("Can't get fb handle. Ask root.");
        goto cleanup;
    }
    

    int ret = drmPrimeHandleToFD(drm_fd, fb->handle, 0, &dma_buf_fd);
    //open (dma_buf_fd, (mode_t) 0666);
   
    DmaBuf img; 
	img.width = fb->width;
	img.height = fb->height;
	img.pitch = fb->pitch;
	img.offset = 0;
	img.fourcc = DRM_FORMAT_XRGB8888;
    img.fd = dma_buf_fd;
    
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	
	{
		struct sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		if (strlen(sockname) >= sizeof(addr.sun_path)) {
			MSG("Socket filename '%s' is too long, max %d",
				sockname, (int)sizeof(addr.sun_path));
			goto cleanup;
		}
		strcpy(addr.sun_path, sockname);
		if (-1 == bind(sockfd, (const struct sockaddr*)&addr, sizeof(addr))) {
			perror("Cannot bind unix socket");
			goto cleanup;
		}

		if (-1 == listen(sockfd, 1)) {
			perror("Cannot listen on unix socket");
			goto cleanup;
		}
	}

	for (;;) {
		int connfd = accept(sockfd, NULL, NULL);
		if (connfd < 0) {
			perror("Cannot accept unix socket");
			goto cleanup;
		}

		MSG("accepted socket %d", connfd);

		struct msghdr msg = {0};

		struct iovec io = {
			.iov_base = &img,
			.iov_len = sizeof(img),
		};
		msg.msg_iov = &io;
		msg.msg_iovlen = 1;

		char cmsg_buf[CMSG_SPACE(sizeof(dma_buf_fd))];
		msg.msg_control = cmsg_buf;
		msg.msg_controllen = sizeof(cmsg_buf);
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(dma_buf_fd));
		memcpy(CMSG_DATA(cmsg), &dma_buf_fd, sizeof(dma_buf_fd));

		ssize_t sent = sendmsg(connfd, &msg, MSG_CONFIRM);
		if (sent < 0) {
			perror("cannot sendmsg");
			goto cleanup;
		}

		MSG("sent %d", (int)sent);

		close(connfd);
	}

cleanup:
	if (sockfd >= 0)
		close(sockfd);
	if (dma_buf_fd >= 0)
		close(dma_buf_fd);
	if (fb)
		drmModeFreeFB(fb);
	close(drm_fd);
    
    return 0;
}
