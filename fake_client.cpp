/* (c) Intel Corporation 2015, All Rights Reserved */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

void __send_analyzed_info(int sockfd, analyzed_info_t *info) {
	int rc;
	client_packet_t packet;


	packet.header.signature = CLIENT_HEADER_SIGNATURE;

	GET_TIMESTAMP(&info->timestamp_gotframe);
	GET_TIMESTAMP(&info->timestamp_sent);
	memcpy(&packet.data, info, sizeof(analyzed_info_t));

	packet.checksum = __calc_checksum(info);

	rc = send(sockfd, &packet, sizeof(client_packet_t), MSG_DONTWAIT);

	if (rc == -1) {
		perror("Error sending data : ");
	}


}

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
	analyzed_info_t data;
	int one = 1;

	cl = (client_info_t*)thread_data;

	printf("Starting thread for %d:%d\n", cl->client_id, cl->camera_id);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
	memset(&server_addr, 0x0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	server_addr.sin_addr.s_addr = inet_addr(server_ip);

	
	rc = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	if (rc < 0 ) {
		perror("Faield to connect\n");
		return NULL;
	}	
	printf("Connected!\n");

	int client_id;

	client_id = cl->client_id;
	
	rc = send(sockfd, &client_id, sizeof(client_id), 0);

	if (rc < 0) {
		perror("Failed to send client id\n");
		return NULL;
	}

	data.frame_id = 120;
	data.camera_id = cl->camera_id;

	while(1) {

		// Fill in info here
		data.frame_id++;

		int i,x,y;
		
		for (i=0; i<7; i++) {
			data.color_info[i].x = 100;
			data.color_info[i].y = 100;
			data.color_info[i].area = 300;
		}

		i=0;
		

		for (y=0; y<NUM_VERT_CELLS; y++) {
			for (x=0; x<NUM_HORIZ_CELLS; x++) {
				data.map[i] = 0x0;
				i++;
			}
		}

		for (i=0; i<intensity; i++) {
			data.map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR0_PRESENT;
			data.map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR1_PRESENT;
			data.map[rand() % (NUM_VERT_CELLS * NUM_HORIZ_CELLS)] |= COLOR2_PRESENT;
		}
		__send_analyzed_info(sockfd, &data);
		usleep(30000 + rand() % 10000);

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
