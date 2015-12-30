
/* (c) Intel Corporation 2015, All Rights Reserved */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>


#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>

#include <signal.h>
#include <pthread.h>

#include "parse.h"

#include "common.h"
#include <iostream>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/stitching/stitcher.hpp"

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace cv;

#define VIS_SERVER_PORT 1236

#define NUM_ENTRIES 1024

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK_CL_INFO() pthread_mutex_lock(&client_mutex)
#define UNLOCK_CL_INFO() pthread_mutex_unlock(&client_mutex)


typedef struct camera_info_t {
	analyzed_color_t color_info[7];
	char map[NUM_HORIZ_CELLS * NUM_VERT_CELLS];	
	int frame_id;
	struct timeval tv_start;
	int zone_id;
} camera_info_t;

typedef struct client_info_t {
	int valid;
	camera_info_t camera_info[NUM_CAMERAS_PER_HUB];
} client_info_t;


volatile int client_info_updated = 0;
client_info_t client_info[NUM_ENTRIES];



static char vis_window_name[100];

#define VIS_WIDTH 1100
#define VIS_HEIGHT 900

#define PER_CLIENT_W 200
#define PER_CLIENT_H 120 

//#define PER_CLIENT_W (200*2)
//#define PER_CLIENT_H (120*2)

#define X_PADDING 10
#define Y_PADDING 20

#define START_PAD_X 30
#define START_PAD_Y 30


static void __get_approx_rgb(int h, int *r, int *g, int *b) {
	if ((h>=0 && h<15) || h>=171) {
		*r=255; *g=0; *b=0;
		return;
	}

	if (h>=15 && h<33) {
		*r=255; *g=255; *b=0;
		return;
	}

	if (h>=33 && h<76) {
		*r=0; *g=255; *b=0;
		return;
	}

	if (h>=76 && h<96) {
		*r=0; *g=255; *b=255;
		return;
	}

	if (h>=96 && h<131) {
		*r=0; *g=0; *b=255;
		return;
	}

	if (h>=131 && h<171) {
		*r=255; *g=0; *b=255;
		return;
	}
}

static void __get_visual_offset(int client_id, int camera_id, int *x_offset, int *y_offset)
{
	int x;
	int y;

	int index;
	int divider;

	index = (client_id - 1) * NUM_CAMERAS_PER_HUB + camera_id;

	divider = VIS_WIDTH / ((PER_CLIENT_W + X_PADDING));

	divider &= 0xFFFFFFFE;

	x = (index % divider)  * (PER_CLIENT_W + X_PADDING) + START_PAD_X;
	y = (index / divider) * (PER_CLIENT_H + Y_PADDING) + START_PAD_Y;

	*x_offset = x;
	*y_offset = y;

	return;
	
}

static void __get_visual_offset_by_zone(int zone_id, int *x_offset, int *y_offset)
{
	int x;
	int y;

	int index;
	int divider;

	index = zone_id - 1;

	//divider = VIS_WIDTH / ((PER_CLIENT_W + X_PADDING));

	//divider &= 0xFFFFFFFE;
	divider = 5;

	x = (index % divider)  * (PER_CLIENT_W + X_PADDING) + START_PAD_X;
	y = (index / divider) * (PER_CLIENT_H + Y_PADDING) + START_PAD_Y;

	*x_offset = x;
	*y_offset = y;

}

void* vis_function(void *data)
{
	int i;
	client_info_t *client;
	Mat vis_mat;
	int x_offset;
	int y_offset;

	char cl_msg[100];
	int cam_id;
	
	int cell_w;
	int cell_h;

	cell_w = PER_CLIENT_W / NUM_HORIZ_CELLS;
	cell_h = PER_CLIENT_H / NUM_VERT_CELLS;

	while (1) {


		LOCK_CL_INFO();

		// Quick polling way -- use signal/mutex instead
		if (client_info_updated == 0) {

			UNLOCK_CL_INFO();
			usleep(10000); 
			continue;
		}

		
		client_info_updated = 0;

		vis_mat = Mat::zeros(Size(VIS_WIDTH, VIS_HEIGHT), CV_8UC3);

		for (i=0; i<NUM_ENTRIES; i++) {
			client = &client_info[i];

			if (!client->valid) continue;


			for (cam_id = 0; cam_id < NUM_CAMERAS_PER_HUB; cam_id++) {
				int cell_id;
				int cell_x;
				int cell_y;
				struct timeval tv_now;
				camera_info_t *cam_info;

				cam_info = &client->camera_info[cam_id];

				__get_visual_offset_by_zone(cam_info->zone_id, &x_offset, &y_offset);

				if (cam_info->tv_start.tv_sec == 0) {
					gettimeofday(&cam_info->tv_start, 0);
				}


				rectangle(vis_mat,
							Point(x_offset-1, y_offset-1),
							Point(x_offset + PER_CLIENT_W +1, y_offset + PER_CLIENT_H+1),
							cam_id == 0 ? Scalar(255,0,0) : Scalar(0,0,255), 1, 8);
				

				gettimeofday(&tv_now, 0);

				long delta = (tv_now.tv_sec - cam_info->tv_start.tv_sec) * 1000000 +
							(tv_now.tv_usec - cam_info->tv_start.tv_usec);

				
				float fps = (cam_info->frame_id * 1000000.0) / delta;
				
//				sprintf(cl_msg, "ID: %d:%d (zone: %d)  fps = %.2f (%d)", i, cam_id, cam_info->zone_id, fps, cam_info->frame_id);
				sprintf(cl_msg, "ID: %d:%d (zone: %d)  (%d)", i, cam_id, cam_info->zone_id,  cam_info->frame_id);

				putText(vis_mat, cl_msg, Point(x_offset+5, y_offset-5), FONT_HERSHEY_SIMPLEX, 0.3, 
						Scalar(0,255,0), 1, 8);

				char *map;
				analyzed_color_t *color_info;
				int avg_h;

				map = cam_info->map;
				color_info = cam_info->color_info;		
				int r,g,b;
				//color_info[0].h_low, s_low, v_low, h_high, s_high, v_high

				for (cell_id = 0; cell_id < NUM_HORIZ_CELLS * NUM_VERT_CELLS; cell_id++) {
					
					cell_x = x_offset + (cell_id % NUM_HORIZ_CELLS) * cell_w;
					cell_y = y_offset + (cell_id / NUM_HORIZ_CELLS) * cell_h;

					rectangle(vis_mat,
							Point(cell_x, cell_y),
							Point(cell_x + cell_w, cell_y + cell_h),
							Scalar(128,128,128), 1, 8);

					if (map[cell_id] & COLOR0_PRESENT) {

						__get_approx_rgb( (color_info[0].h_low+color_info[0].h_high)/2,
											&r, &g, &b);
						rectangle(vis_mat,
							Point(cell_x + 1, cell_y + 1),
							Point(cell_x + cell_w/3 - 1, cell_y+cell_h-1),
							Scalar(b,g,r), -1, 8);
					} 
					
					if (map[cell_id] & COLOR1_PRESENT) {
						__get_approx_rgb( (color_info[1].h_low+color_info[1].h_high)/2,
											&r, &g, &b);
						rectangle(vis_mat,
							Point(cell_x + 1 + cell_w/3, cell_y + 1),
							Point(cell_x + cell_w*2/3 - 1, cell_y+cell_h-1),
							Scalar(b,g,r), -1, 8);
					}

					if (map[cell_id] & COLOR2_PRESENT) {
						__get_approx_rgb( (color_info[2].h_low+color_info[2].h_high)/2,
											&r, &g, &b);
						rectangle(vis_mat,
							Point(cell_x + 1 + cell_w*2/3, cell_y + 1),
							Point(cell_x + cell_w - 1, cell_y+cell_h-1),
							Scalar(b,g,r), -1, 8);
					}

				}
			}
			
		}	

		UNLOCK_CL_INFO();

		imshow(vis_window_name, vis_mat);
		waitKey(30);

		//usleep(30000);
	}
}

int main(int argc, char **argv) 
{
	pthread_t vis_thread0;

	
	int listenfd,connfd;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t clilen;
	char client_name[INET6_ADDRSTRLEN];
	int rc;
	int port;
	int cam_id;
	int client_id;
	camera_info_t *cam_info;

	if (argc != 2) {
		printf("Error... usage: ./vis_server <port>\n");
		return -1;
	}

	port = atoi(argv[1]);
	printf("Initializing visual server... on port %d\n",port);

	sprintf(vis_window_name, "Visualization:%d", port);

	memset(client_info, 0x0, sizeof(client_info));

#ifdef VIS_SERVER_TCP_CONNECTION
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
#else
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
#endif

	int option = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, sizeof(int));
	
	int udp_buff_size;
	udp_buff_size = sizeof(visual_packet_t) * 5;
	setsockopt(listenfd, SOL_SOCKET, SO_RCVBUF, &udp_buff_size, sizeof(udp_buff_size));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	rc = bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

	if (rc == -1) {
		perror("bind error: ");
		return -1;
	}

#ifdef VIS_SERVER_TCP_CONNECTION
	rc = listen(listenfd,1024);
	if (rc == -1) { 
		perror("Listen error: ");
		return -1;
	}
#endif

	
	pthread_create(&vis_thread0, 0, vis_function, 0);
	printf("Accepting connections on visualization...\n");

	visual_packet_t vis_packet;


	while (1) {

#if VIS_SERVER_TCP_CONNECTION 
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);

		if (connfd == -1) {
			perror("ACCEPT ERROR\n");
			continue;
		}

		printf("Connection accepted\n");
#else 
		connfd = listenfd;
#endif

		memset(client_info, 0x0, sizeof(client_info));

		while (1) {



			rc = __recvfrom(connfd, &vis_packet, sizeof(visual_packet_t));

			if (rc != 0) {
				printf("client disconnectd...\n");
				break;
			}

			client_id = vis_packet.client_id;		
			cam_id = vis_packet.data.camera_id;

			cam_info = &client_info[client_id].camera_info[cam_id];

			
			LOCK_CL_INFO();
		
			client_info[client_id].valid = 1;

			memcpy(cam_info->color_info,
					vis_packet.data.color_info,
					sizeof(analyzed_color_t)*7);

			memcpy(cam_info->map, vis_packet.data.map, sizeof(char) * NUM_HORIZ_CELLS * NUM_VERT_CELLS);
			cam_info->frame_id = vis_packet.data.frame_id;
			cam_info->zone_id = vis_packet.zone_id;

			if (vis_packet.data.frame_id == 0 || vis_packet.data.frame_id == 1 || vis_packet.data.frame_id == -1) {
				printf("WARN: frame_id came as %d\n", vis_packet.data.frame_id);
			}

			client_info_updated = 1;
			UNLOCK_CL_INFO();
		}
		
	}

	return 0;
}
