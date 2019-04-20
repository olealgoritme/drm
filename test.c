#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define MSG(args, ...) printf(args "\n", ##__VA_ARGS__)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        MSG("Needs fb id");
        return 0;
    }

    uint32_t fb_id = strtol(argv[1], NULL, 0);
    const char *card = "/dev/dri/card0";
    int drm_fd;
    int prime_fd;
    int dma_buf_fd;
    /*int sock_fd; */
    drmModeFBPtr fb_ptr;

    drm_fd = open(card, O_CLOEXEC);
    fb_ptr = drmModeGetFB(drm_fd, fb_id);
    MSG("Width: %u, Height: %u (FB ID: %#x)", fb_ptr->height, fb_ptr->width,
        fb_id);

    prime_fd = drmPrimeHandleToFD(drm_fd, fb_ptr->handle, 0, &dma_buf_fd);
    if (prime_fd < 0) {
        MSG("Can't get prime FD handle. Do you have correct permissions to "
            "access DRM FD?");
        return -1;
    } else {
        MSG("drmPrimeHandleToFD: prime_fd = %d | dma_buf_fd = %d (size: %d)",
            prime_fd, dma_buf_fd, (int)sizeof(dma_buf_fd));
    }

drmModeFreeFB(fb_ptr);

close(dma_buf_fd);
close(prime_fd);
close(drm_fd);

return 0;
}
