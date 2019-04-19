#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>					//O_RDONLY
#include <sys/types.h>				//uint64_t
#include <sys/socket.h>				//Socket related constants
#include <sys/un.h>					//Unix domain constants
#include <netinet/in.h>				//Socket related constants

#ifdef __linux__
	#include <linux/limits.h>		//PATH_MAX
#else
	#define PATH_MAX 4096
#endif

#define X11_OP_REQ_CREATE_WINDOW	0x01
#define X11_OP_REQ_MAP_WINDOW		0x08
#define X11_OP_REQ_CREATE_PIX		0x35
#define X11_OP_REQ_CREATE_GC		0x37
#define X11_OP_REQ_PUT_IMG			0x48

struct x11_conn_req
{
	uint8_t order;
	uint8_t pad1;
	uint16_t major,
			minor;
	uint16_t auth_proto,
			auth_data;
	uint16_t pad2;
};

struct x11_conn_reply
{
	uint8_t success;
	uint8_t pad1;
	uint16_t major,
			minor;
	uint16_t length;
};

struct x11_conn_setup
{
	uint32_t release;
	uint32_t id_base,
			id_mask;
	uint32_t motion_buffer_size;
	uint16_t vendor_length;
	uint16_t request_max;
	uint8_t roots;
	uint8_t formats;
	uint8_t image_order;
	uint8_t bitmap_order;
	uint8_t scanline_unit,
			scanline_pad;
	uint8_t keycode_min,
			keycode_max;
	uint32_t pad;
};

struct x11_pixmap_format
{
	uint8_t depth;
	uint8_t bpp;
	uint8_t scanline_pad;
	uint8_t pad1;
	uint32_t pad2;
};

struct x11_root_window
{
	uint32_t id;
	uint32_t colormap;
	uint32_t white,
			black;
	uint32_t input_mask;   
	uint16_t width,
			height;
	uint16_t width_mm,
			height_mm;
	uint16_t maps_min,
			maps_max;
	uint32_t root_visual_id;
	uint8_t backing_store;
	uint8_t save_unders;
	uint8_t depth;
	uint8_t depths;
};

struct x11_depth
{
	uint8_t depth;
	uint8_t pad1;
	uint16_t visuals;
	uint32_t pad2;
};

struct x11_visual
{
	uint8_t group;
	uint8_t bits;
	uint16_t colormap_entries;
	uint32_t mask_red,
			mask_green,
			mask_blue;
	uint32_t pad;
};

struct x11_connection
{
	struct x11_conn_reply header;
	struct x11_conn_setup *setup;
	struct x11_pixmap_format *format;
	struct x11_root_window *root;
	struct x11_depth *depth;
	struct x11_visual *visual;
};

struct x11_error
{
	uint8_t success;
	uint8_t code;
	uint16_t seq;
	uint32_t id;
	uint16_t op_major;
	uint8_t op_minor;
	uint8_t pad[21];
};

//Copy characters from one string to another until end
int strcopy(char *dest, const char *src, char end)
{
	int i;
	for (i=0; src[i]!=end; i++)
		dest[i]=src[i];
	return i;
}

//Copy n characters from one string to another
char *strncopy(char *dest, const char *src, size_t n)
{
	int i;
	for (i=0; i<n; i++)
		dest[i]=src[i];
	return dest;
}

int count_bits(uint32_t n)
{
	unsigned int c;

	c = n - ((n >> 1) & 0x55555555);
	c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
	c = ((c >> 4) + c) & 0x0F0F0F0F;
	c = ((c >> 8) + c) & 0x00FF00FF;
	c = ((c >> 16) + c) & 0x0000FFFF;

	return c;
}

void error(char *msg, int len)
{
	write(0, msg, len);
	exit(1);

	return;
}

int x11_handshake(int sock, struct x11_connection *conn)
{
	struct x11_conn_req req = {0};
	req.order='l';	//Little endian
	req.major=11; req.minor=0; //Version 11.0
	write(sock,&req,sizeof(struct x11_conn_req)); //Send request

	read(sock,&conn->header,sizeof(struct x11_conn_reply)); //Read reply header

	if (conn->header.success==0)
		return conn->header.success;

	conn->setup = sbrk(conn->header.length*4);	//Allocate memory for remainder of data
	read(sock,conn->setup,conn->header.length*4);	//Read remainder of data
	void* p = ((void*)conn->setup)+sizeof(struct x11_conn_setup)+conn->setup->vendor_length;	//Ignore the vendor
	conn->format = p;	//Align struct with format sections
	p += sizeof(struct x11_pixmap_format)*conn->setup->formats;	//move pointer to end of section
	conn->root = p;	//Align struct with root section(s)
	p += sizeof(struct x11_root_window)*conn->setup->roots;	//move pointer to end of section
	conn->depth = p; //Align depth struct with first depth section
	p += sizeof(struct x11_depth);	//move pointer to end of section
	conn->visual = p; //Align visual with first visual for first depth

	return conn->header.success;
}

uint32_t x11_generate_id(struct x11_connection *conn)
{
	static uint32_t id = 0;
	return (id++ | conn->setup->id_base);
}


#define X11_FLAG_GC_FUNC 0x00000001
#define X11_FLAG_GC_PLANE 0x00000002
#define X11_FLAG_GC_BG 0x00000004
#define X11_FLAG_GC_FG 0x00000008
#define X11_FLAG_GC_LINE_WIDTH 0x00000010
#define X11_FLAG_GC_LINE_STYLE 0x00000020
#define X11_FLAG_GC_FONT 0x00004000
#define X11_FLAG_GC_EXPOSE 0x00010000
void x11_create_gc(int sock, struct x11_connection *conn, uint32_t id, uint32_t target, uint32_t flags, uint32_t *list)
{
	uint16_t flag_count = count_bits(flags);

	uint16_t length = 4 + flag_count;
	uint32_t *packet = sbrk(length*4);

	packet[0]=X11_OP_REQ_CREATE_GC | length<<16;
	packet[1]=id;
	packet[2]=target;
	packet[3]=flags;
	int i;
	for (i=0;i<flag_count;i++)
		packet[4+i] = list[i];

	write(sock,packet,length*4);

	sbrk(-(length*4));

	return;
}

#define X11_FLAG_WIN_BG_IMG 0x00000001
#define X11_FLAG_WIN_BG_COLOR 0x00000002
#define X11_FLAG_WIN_BORDER_IMG 0x00000004
#define X11_FLAG_WIN_BORDER_COLOR 0x00000008
#define X11_FLAG_WIN_EVENT 0x00000800
void x11_create_win(int sock, struct x11_connection *conn, uint32_t id, uint32_t parent,
					uint16_t x, uint16_t y, uint16_t w, uint16_t h,
					uint16_t border, uint16_t group, uint32_t visual,
					uint32_t flags, uint32_t *list)
{
	uint16_t flag_count = count_bits(flags);

	uint16_t length = 8 + flag_count;
	uint32_t *packet = sbrk(length*4);

	packet[0]=X11_OP_REQ_CREATE_WINDOW | length<<16;
	packet[1]=id;
	packet[2]=parent;
	packet[3]=x | y<<16;
	packet[4]=w | h<<16;
	packet[5]=border<<16 | group;
	packet[6]=visual;
	packet[7]=flags;
	int i;
	for (i=0;i<flag_count;i++)
		packet[8+i] = list[i];

	write(sock,packet,length*4);

	sbrk(-(length*4));

	return;
}

void x11_map_window(int sock, struct x11_connection *conn, uint32_t id)
{
	uint32_t packet[2];
	packet[0]=X11_OP_REQ_MAP_WINDOW | 2<<16;
	packet[1]=id;
	write(sock,packet,8);

	return;
}

#define X11_IMG_FORMAT_MONO 0x0
#define X11_IMG_FORMAT_XY 0x01
#define X11_IMG_FORMAT_Z 0x02
void x11_put_img(int sock, struct x11_connection *conn, uint8_t format, uint32_t target, uint32_t gc,
				uint16_t w, uint16_t h, uint16_t x, uint16_t y, uint8_t depth, void *data)
{
	uint32_t packet[6];
	uint16_t length = ((w * h)) + 6;

	packet[0]=X11_OP_REQ_PUT_IMG | format<<8 | length<<16;
	packet[1]=target;
	packet[2]=gc;
	packet[3]=w | h<<16;
	packet[4]=x | y<<16;
	packet[5]=depth<<8;

	write(sock,packet,24);
	write(sock,data,(w*h)*4);

	return;
}

int main(int argc, char **argv)
{
	int sockfd, clilen, srv_len, openfd;

	if (argc==1)
	{
		struct sockaddr_un serv_addr={0};

		//Create the socket
		sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (sockfd < 0)
			error("Error opening socket\n",21);

		serv_addr.sun_family = AF_UNIX;
		strcopy(serv_addr.sun_path, "/tmp/.X11-unix/X0", 0);
		srv_len = sizeof(struct sockaddr_un);

		connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	}
	else
	{
		struct sockaddr_in serv_addr={0};

		//Create the socket
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			error("Error opening socket\n",21);

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr=0x0100007f;
		serv_addr.sin_port = htons(6001);

		connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	}

	struct x11_connection conn = {0};
	x11_handshake(sockfd,&conn);

	int gc = x11_generate_id(&conn);
	uint32_t val[32];
	val[0]=0x00aa00ff;
	val[1]=0x00000000;
	x11_create_gc(sockfd,&conn,gc,conn.root[0].id,X11_FLAG_GC_BG|X11_FLAG_GC_EXPOSE,val);

	int win = x11_generate_id(&conn);
	val[0]=0x00aa00ff;
	x11_create_win(sockfd,&conn,win,conn.root[0].id,200,200,400,200,1,1,conn.root[0].root_visual_id,X11_FLAG_WIN_BG_COLOR,val);

	x11_map_window(sockfd,&conn,win);

	uint32_t data[1600]={0};
	int i;
	for (i=0;i<1600;i++)
		data[i]=0x0055ff33;

	x11_put_img(sockfd,&conn,X11_IMG_FORMAT_Z,win,gc,40,40,0,0,conn.root[0].depth,data);

	while (1) ;
}

