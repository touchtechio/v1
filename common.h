
#define SERVER_PORT 12000

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720

#define NUM_HORIZ_CELLS 20
#define NUM_VERT_CELLS 20

#define MIN_AREA_TO_DETECT_PIXELS 1000



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
} __attribute__((packed)) analyzed_color_t;


typedef struct analyzed_info_t {
	unsigned char camera_id;
	analyzed_color_t color_info[7];
	unsigned char map[NUM_HORIZ_CELLS * NUM_VERT_CELLS];
	int frame_id;
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
	int 				client_id;
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




