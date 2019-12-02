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

    char *shmdev = "/drmxuw";
    void *drm_ptr;
    void *sh_ptr;

    const char *card = "/dev/dri/card0";
    drmModeFBPtr fb;

    // Process ID
    pid_t pid;


    // DmaBuf struct containing image data
    DmaBuf *dmaBuf;
    dmaBuf = (DmaBuf *) malloc(sizeof(DmaBuf));

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
        exit(-1);
    }

    if (!fb->handle) {
        MSG("Can't get fb handle. Ask root.");
        exit(-1);
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
    
   
    /** SHARED MEMORY START **/
    // create shared memory object
    shmID = shm_open(shmdev, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(!shmID) {
        perror("shm_open");
        exit(-1);
    }

    // shared memory is initalized with size 0, lets set to SH_MEM_SIZE
    ftruncate(shmID, SH_MEM_SIZE);

    // shared memory mapping
    sh_ptr = mmap(NULL, SH_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmID, 0);
    if(sh_ptr == MAP_FAILED) {
        perror("cant mmap");
        exit(-1);
    }

    memcpy(sh_ptr, &img, sizeof(dmaBuf));


    printf("Shared BO: /dev/shm%s\n", shmdev);
    printf("PID: %d\n", pid);
    /** SHARED MEMORY END **/
/*
 *
 *  for(;;) {
 *   int ret, fd;
 *    char buf[sizeof(img)];
 *
 *    const char * dv = "/dev/shm/drmxuw";
 *  if ((fd = open(dv, O_RDONLY)) < 0)
 *    perror("open() error");
 *  else {
 *
 *
 *    while ((ret = read(fd, buf, sizeof(buf)-1)) > 0) {
 *      printf("block read: \n<%s>\n", buf);
 *    }
 *    close(fd);
 *  }
 *    sleep(1);
 *  }
 */


   for(;;) {
        sleep(1);
            
   } 

    return 0;
}
