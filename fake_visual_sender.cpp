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

#include <iostream>

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include "common.h"

using namespace std;


int intensity = 1;

char *server_ip;
int server_port;

typedef struct client_info_t {
	int camera_id;
	int client_id;
} client_info_t;


void *client_fn(void *thread_data) {
	struct sockaddr_in server_addr;
	int rc;
	client_info_t *cl;
	int sockfd;
	analyzed_info_t *p_data;
	send_udp_sock_t socket;
	visual_packet_t fake_packet;	

	cl = (client_info_t*)thread_data;

	printf("Starting thread for %d:%d\n", cl->client_id, cl->camera_id);

	__create_udp_send_socket(server_ip, server_port, &socket);

	p_data = &fake_packet.data;
	p_data->frame_id = 120;
	p_data->camera_id = cl->camera_id;

	fake_packet.client_id = cl->client_id;
	while(1) {

		fake_packet.zone_id = __lookup_zone_id(cl->client_id, cl->camera_id);
		// Fill in info here
		p_data->frame_id++;

		int i,x,y;
		
		for (i=0; i<7; i++) {
			p_data->color_info[i].x = p_data->frame_id % 1280;
			p_data->color_info[i].y = p_data->frame_id % 720;
			p_data->color_info[i].area = 4000*(i+1);

			p_data->color_info[i].h_low = 10*(i+1);
			p_data->color_info[i].h_high = 11*(i+1)
;
			p_data->color_info[i].s_low = 30*(i+1);
			p_data->color_info[i].s_high = 31*(i+1);

			p_data->color_info[i].v_low = 20*(i+1);
			p_data->color_info[i].v_high = 21*(i+1);
		}

		i=0;
		
		for (y=0; y<NUM_VERT_CELLS; y++) {
			for (x=0; x<NUM_HORIZ_CELLS; x++) {
				p_data->map[i] = 0x0;
				i++;
			}
		}

#if 0
		for (i=0; i<400; i++) {
			p_data->map[i] = i * fake_packet.zone_id;
		}
#endif 
#if 1
		for (i=0; i<intensity; i++) {
			p_data->map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR0_PRESENT;
			p_data->map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR1_PRESENT;
			p_data->map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR2_PRESENT;
		}
#endif
		
		printf("Sending packet %d\n", p_data->frame_id);
		__send_udp_packet(&socket, &fake_packet, sizeof(fake_packet));
		//usleep(40000 + rand() % 10000);

	}
}

int main(int argc, char **argv) 
{
	int i;
	pthread_t client_thread[30];
	int num_clients = 1;

	if (argc < 3) {
		printf("Usage: ./fake_client <server_ip> <server_port> <num_clients> <inensity>\n");
		return -1;
	}

	server_ip = argv[1];
	server_port = atoi(argv[2]);

	if (argc >= 4) {
		num_clients = atoi(argv[3]);
	}

	if (argc == 5) {
		intensity = atoi(argv[4]);
	}


	for (i=0; i<num_clients; i++) {
		client_info_t *cl = (client_info_t*)malloc(sizeof(client_info_t));

		cl->client_id = i/2 + 1;
		cl->camera_id = i%2;

		pthread_create(&client_thread[i], 0, client_fn, cl);
	}
	

	for (i=0; i<num_clients; i++) {
		pthread_join(client_thread[i], 0);
	}

	return 0;
}
