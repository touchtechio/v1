
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


#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>


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

	memset(client_info, 0x0, sizeof(client_info));

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
            
            printf("packet recieved of %lu \n", sizeof(visual_packet_t));


			if (rc != 0) {
				printf("client disconnectd...\n");
				break;
			}

			client_id = vis_packet.client_id;		
			cam_id = vis_packet.data.camera_id;
            
            printf("client: %d : cam: %d \n", client_id, cam_id);


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
            
            analyzed_color_t *color_info = vis_packet.data.color_info;

            unsigned char *map = vis_packet.data.map;

            for (int i = 0; i < 3; i++) {
                
                printf("color:%d : %hd, %hd : %d : " , i, color_info[i].x, color_info[i].y, color_info[i].area);

                
                
                for (int j = 0; j < 80; j++) {
                    if ((1 << i) & map[j]) {
                        printf("1");
                    } else {
                        printf("0");
                        
                    }
                    //printf(" ");
                }
                printf("\n");
                
               /*
                for (int i = 0; i < 80; i++) {
                    printf("%hhu ", map[i]);
                }
            */
                
            }
            

            
            printf("\n");

            
            
			client_info_updated = 1;
			UNLOCK_CL_INFO();
		}
		
	}

	return 0;
}
