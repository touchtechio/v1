// USE TCP connectiont o vis server instead of UDP

//#define VIS_SERVER_TCP_CONNECTION


// Add timestamps to packet info
//#define ENABLE_TIMESTAMPS

#ifdef ENABLE_TIMESTAMPS
#define GET_TIMESTAMP(x) gettimeofday(x, 0);
#else
#define GET_TIMESTAMP(x)
#endif


#define SERVER_PORT 12000

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720

#define NUM_HORIZ_CELLS 20
#define NUM_VERT_CELLS 20

#define MIN_AREA_TO_DETECT_PIXELS 100



// If REQUIRED_AREA is set then use it, if set to 0 then use 
// CELL_THRESH_PERCENT 
#define REQUIRED_AREA 0


// What % from 0 to 100 should be filled with detected color
// in order to trigger a 'detected' mark
#define CELL_THRESH_PERCENT (5.0)



#define BIT0 (1<<0)
#define BIT1 (1<<1)
#define BIT2 (1<<2)
#define BIT3 (1<<3)
#define BIT4 (1<<4)
#define BIT5 (1<<5)

#define COLOR0_PRESENT BIT0
#define COLOR1_PRESENT BIT1
#define COLOR2_PRESENT BIT2
#define COLOR3_PRESENT BIT3
#define COLOR4_PRESENT BIT4
#define COLOR5_PRESENT BIT5
#define COLOR6_PRESENT BIT6


typedef struct analyzed_color_t {
	short x;
	short y;
	int area;
	unsigned char h_low, s_low, v_low;
	unsigned char h_high, s_high, v_high;
} __attribute__((packed)) analyzed_color_t;


typedef struct analyzed_info_t {
	unsigned char camera_id;
	analyzed_color_t color_info[7];
	unsigned char map[NUM_HORIZ_CELLS * NUM_VERT_CELLS];
	int frame_id;

#ifdef ENABLE_TIMESTAMPS
	struct timeval timestamp_gotframe;
	struct timeval timestamp_sent;
#endif

} __attribute__((packed)) analyzed_info_t;

typedef struct client_packet_header_t {
	unsigned int signature;
} client_packet_header_t;



#define CLIENT_HEADER_SIGNATURE 0xFFAB

typedef struct client_packet_t {
	client_packet_header_t 	header;
	analyzed_info_t 		data;
	unsigned char 			checksum;
} __attribute__((packed)) client_packet_t;


typedef struct visual_packet_t {
	char 				zone_id;
	char 				client_id;
	analyzed_info_t 	data;
} __attribute__((packed)) visual_packet_t;


unsigned char __calc_checksum(analyzed_info_t *info) {
	unsigned char *ptr;
	int size, i;
	unsigned char result = 0x0;

	size = sizeof(analyzed_info_t);
	ptr = (unsigned char*)info;

	for (i = 0; i < size; i++) {
		result += ptr[i];
	}

	return result;
}


inline void __dbg_print_analyzed_data(analyzed_info_t *info) {
	int i;

	printf("(%d, ", info->camera_id);
	for (i = 0; i < 7; i++) {
			printf("%d, %d, %d, ",
			info->color_info[i].x,
			info->color_info[i].y,
			info->color_info[i].area);
	}
	printf(")\n");
}

inline void __dbg_print_vis_data(visual_packet_t *vpacket) {
	int i;
	analyzed_info_t *p_data;

	p_data = &vpacket->data;

	printf("(%d, %d, ", vpacket->client_id, p_data->camera_id);
	for (i=0; i<7; i++) {
		printf("%d, %d, %d, ",
			p_data->color_info[i].x,
			p_data->color_info[i].y,
			p_data->color_info[i].area);
	}
	printf(")\n");
}


#define DRAW_X(img, x, y, color, width) \
	do {\
		line(img, Point(x-3, y), Point(x+3, y), color, width);\
		line(img, Point(x, y-3), Point(x, y+3), color, width); \
	} while(0)

#define DRAW_X_EXT(img, x, y, color, sz, width) \
	do {\
		line(img, Point(x-sz, y), Point(x+sz, y), color, width);\
		line(img, Point(x, y-sz), Point(x, y+sz), color, width); \
	} while(0)


int __recvfrom(int sockfd, void *data, int size) {
	int n = 0;
	int packet_size = size;
	int rc = 0;

	while (1) {
		n = recvfrom(sockfd, ((char*)data) + size - packet_size, packet_size, 0, NULL, NULL);

		if (n <= 0) {
			rc = -1;
			break;
		}

		packet_size -= n;

		if (packet_size == 0) {
			rc = 0;
			break;
		}
		
	}

	return rc;
}


typedef struct send_udp_sock_t {
	struct sockaddr_in addr;
	int sock_fd;
} send_udp_sock_t;

int __create_udp_send_socket(char *ip, int port, send_udp_sock_t *__sock) 
{
	int rc;

	__sock->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&__sock->addr, 0x0, sizeof(struct sockaddr_in));

	
	__sock->addr.sin_family = AF_INET;
	__sock->addr.sin_port = htons(port);
	__sock->addr.sin_addr.s_addr = inet_addr(ip);


	return 0;
}


int __send_udp_packet(send_udp_sock_t *__sock, void *data, int size) {
	return sendto(__sock->sock_fd,
					data,
					size,
					MSG_DONTWAIT,
					(struct sockaddr*)&__sock->addr,
					sizeof(__sock->addr));
}


int __create_udp_listen_socket(int port) {
	int sockfd;
	struct sockaddr_in addr;
	socklen_t sock_len;
	int rc;
	int option = 1;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, sizeof(int));

	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	rc = bind(sockfd,(struct sockaddr *)&addr,sizeof(addr));

	if (rc == -1) {
		perror("bind error: ");
		return -1;
	}

	return sockfd;
}

typedef enum map_serv_cmd {
	MAP_SERV_START_CAPTURE = 0x100,
	MAP_SERV_STOP_CAPTURE,
	MAP_GET_RESP,
	MAP_SET_RESP,
} map_serv_cmd;

typedef struct map_serv_packet_t {
	map_serv_cmd cmd_id;
	char ip[200];
	int port;
	int val;
} map_serv_packet_t;

typedef enum option_id_t {
	OID_NONE = 0x0,
	OID_DUMP_STATS = 0x200,
	OID_SET_DFLT_BRIGHTNESS,
	OID_SET_DFLT_CONTRAST,
	OID_SET_DFLT_SATURATION,
	OID_SET_DFLT_EXPOSURE_ABSOLUTE,
	OID_SET_DFLT_FOCUS_ABSOLUTE,
	OID_SET_DFLT_WHITE_BALANCE_TEMP,
	OID_SET_DFLT_ZOOM_ABSOLUTE,
	OID_GENERATE_CONFIG,
} option_id_t;

#define MAX_OPTION_ARGS 6
typedef struct option_packet_t {
	option_id_t id;
	int arg[MAX_OPTION_ARGS];
} option_packet_t;

#define OPTION_PORT_CLIENT 1500
#define OPTION_PORT_MAIN_SERV 1501
#define OPTION_PORT_MAP_SERV 1502
#define OPTION_PORT_VIS_SERV 1503
#define OPTION_PORT_OPT_SERV 1504
