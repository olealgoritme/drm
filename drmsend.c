#include <xf86drm.h>
#include <libdrm/drm_fourcc.h>
#include <xf86drmMode.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

typedef struct {
	int width, height;
	uint32_t fourcc;
	int offset, pitch;
} DmaBufMetadata;

void printUsage(const char *name) {
	MSG("usage: %s fb_id socket_filename </dev/dri/card>", name);
}

int tcp_start_server() {
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;

	/* creates an UN-named socket inside the kernel and returns
	 * an integer known as socket descriptor
	 * This function takes domain/family as its first argument.
	 * For Internet family of IPv4 addresses we use AF_INET
	 */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(1337);

	/* The call to the function "bind()" assigns the details specified
	 * in the structure 『serv_addr' to the socket created in the step above
	 */
	bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	/* The call to the function "listen()" with second argument as 10 specifies
	 * maximum number of client connections that server will queue for this listening
	 * socket.
	 */
	listen(listenfd, 10);

	while(1)
    {
		/* In the call to accept(), the server is put to sleep and when for an incoming
		 * client request, the three way TCP handshake* is complete, the function accept()
		 * wakes up and returns the socket descriptor representing the client socket.
		 */
		connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
		return connfd;
        /* As soon as server gets a request from client, it prepares the date and time and
		 * writes on the client socket through the descriptor returned by accept()
		 */
	    //sendfd(connfd, fd_to_send);	
        // write(connfd, sendBuff, strlen(sendBuff));
		//close(connfd);
		//sleep(1);
	
    }
}


int main(int argc, const char *argv[]) {

	uint32_t fb_id = 0;

	if (argc < 2) {
		printUsage(argv[0]);
		return 1;
	}

	{
		char *endptr;
		fb_id = strtol(argv[1], &endptr, 0);
		if (*endptr != '\0') {
			MSG("%s is not valid framebuffer id", argv[1]);
			printUsage(argv[0]);
			return 1;
		}
	}


	const char *card = (argc > 3) ? argv[3] : "/dev/dri/card0";

	MSG("Opening card %s", card);
	const int drmfd = open(card, O_RDWR);
	if (drmfd < 0) {
		perror("Cannot open card");
		return 1;
	}

	int dma_buf_fd = -1;
	drmModeFBPtr fb = drmModeGetFB(drmfd, fb_id);
	if (!fb) {
		MSG("Cannot open fb %#x", fb_id);
		goto cleanup;
	}

	MSG("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
		fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

	if (!fb->handle) {
		MSG("Not permitted to get fb handles. Run either with sudo, or setcap cap_sys_admin+ep %s", argv[0]);
		goto cleanup;
	}
    
    
	DmaBufMetadata img;
	img.width = fb->width;
	img.height = fb->height;
	img.pitch = fb->pitch;
	img.offset = 0;
	img.fourcc = DRM_FORMAT_XRGB8888; // FIXME

	const int ret = drmPrimeHandleToFD(drmfd, fb->handle, DRM_FORMAT_XRGB8888, &dma_buf_fd);
	MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
    
    /* starting tcp socket server */
    int clientfd = tcp_start_server();
    
    struct msghdr msg;
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

		ssize_t sent = sendmsg(clientfd, &msg, MSG_CONFIRM);
		if (sent < 0) {
			perror("cannot sendmsg");
			goto cleanup;
		}

		MSG("sent %d", (int)sent);

		close(clientfd);
    
cleanup:
	if (dma_buf_fd >= 0)
		close(dma_buf_fd);
	if (fb)
		drmModeFreeFB(fb);
	close(drmfd);
	return 0;

}
