
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

int seconds_to_run;
int pix_threshold = 50;

int command_port;


typedef struct coord_t {
	int x,y;
} coord_t;

int last_id[3] = {0,0,0};
coord_t last_coord[3] = {0,0,0};

Mat mapping;

int state = MAP_SERV_START_CAPTURE;

void __add_color(int color_id, int x, int y) {
	int cur_pix_id;
	char to_print[40];

	if (state == MAP_SERV_START_CAPTURE) {

		if ( (abs(last_coord[color_id].x - x) > pix_threshold) ||
			 (abs(last_coord[color_id].y - y) > pix_threshold)) {
	
			if (last_id[color_id] < 30) {
				cur_pix_id = 3*last_id[color_id] + color_id;

		
				cur_pix_id = last_id[color_id];
				printf("#%d -  %d %d\n", cur_pix_id, x, y);
				sprintf(to_print, "%d", cur_pix_id);
			
				DRAW_X_EXT(mapping, x,y, Scalar(255,255,255), 3, 1);
				putText(mapping, to_print, Point(x+2,y-2), FONT_HERSHEY_SIMPLEX, 0.3,
						Scalar(255,255,255), 1, 8);

				last_id[color_id]++;
			}
		}
	}
	
		imshow("map", mapping);
		waitKey(20);
	last_coord[color_id].x = x;
	last_coord[color_id].y = y;
}

void __dump_stats_and_exit(void) {
	printf("Exiting...\n");
	exit(0);
}



void *command_fn(void *data) {
	int sock_fd;
	int rc;
	map_serv_packet_t packet;

	sock_fd = __create_udp_listen_socket(command_port);
	if (sock_fd < 0) {
		printf("FAILED to connect to socket\n");
		exit(1);
	}
	
	while (1) {
		rc = __recvfrom(sock_fd, &packet, sizeof(packet));

		if (rc != 0) {
			printf("error recieving packet\n");
			continue;
		}

		switch (packet.cmd_id) {
			case MAP_SERV_START_CAPTURE:
				state = MAP_SERV_START_CAPTURE;
				printf("MAP_START!\n");
				break;
			case MAP_SERV_STOP_CAPTURE:
				state = MAP_SERV_STOP_CAPTURE;
				printf("MAP_STOP!\n");
				break;	

			case MAP_GET_RESP:
				map_serv_packet_t resp;
				send_udp_sock_t send_sock;

				printf("MAP_TEST %s %d\n", packet.ip, packet.port);
				__create_udp_send_socket(packet.ip, packet.port, &send_sock);

				resp.cmd_id = MAP_SET_RESP;
				resp.val = 0xABCD;

				__send_udp_packet(&send_sock, &resp, sizeof(resp));
				break;

			default:
				printf("Unknown command id recieved :%d\n", packet.cmd_id);
				break;
		}
	}
}

int main(int argc, char **argv) 
{
	int listenfd,connfd;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t clilen;
	char client_name[INET6_ADDRSTRLEN];
	int rc;
	int port;
	int cam_id;
	int client_id;
	pthread_t command_thread;

	if (argc != 3) {
		printf("Error... usage: ./map_server <data_port> <command_port> \n");
		return -1;
	}

	port = atoi(argv[1]);
	command_port = atoi(argv[2]);

	printf("Initializing map server... on port %d [data=%d]\n",port, command_port);


	mapping = Mat::zeros(Size(1280,720), CV_8UC3);

#ifdef VIS_SERVER_TCP_CONNECTION
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
#else
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
#endif

	int option = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, sizeof(int));


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

	
	visual_packet_t vis_packet;

	
	pthread_create(&command_thread, 0, command_fn, 0);

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

		analyzed_color_t *p_colors;
		while (1) {

			rc = __recvfrom(connfd, &vis_packet, sizeof(visual_packet_t));

			if (rc != 0) {
				printf("client disconnectd...\n");
				break;
			}

			client_id = vis_packet.client_id;		
			cam_id = vis_packet.data.camera_id;
			
			p_colors = vis_packet.data.color_info;

			int color_id;

			if (cam_id == 0) {
				for (color_id = 0; color_id < 3; color_id++) {
					if (p_colors[color_id].x != -1) {
						__add_color(color_id, p_colors[color_id].x, p_colors[color_id].y);
					}
				}
			}
		}
		
	}

	return 0;
}
