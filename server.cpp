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

#include <sys/syscall.h>


struct timeval tv_start;

static int options_socket_fd = -1;

static int __lookup_zone_id(int client_id, int camera_id) {
	int row;
	int column;
	row = ((client_id-1)/5) * 2 + camera_id;
	column = 4 - (client_id - 1) % 5;
	return row * 5 + (column + 1);
}

static void __show_timeval(void) {
	struct timeval tv_now;
	unsigned long delta;

	int sec;
	int usec;
	gettimeofday(&tv_now, 0);

	delta = (tv_now.tv_sec - tv_start.tv_sec) * 1000000 + (tv_now.tv_usec - tv_start.tv_usec);

	sec = delta / 1000000;

	usec = delta % 1000000;

	printf("[%d:%d][tid:%lu] ", sec, (int)((usec)/10000.0), syscall(SYS_gettid));
}

#define PR_LOG(x...) __show_timeval(); printf(x);

static void __dump_log_and_exit(void) {

	// TODO: gather log
	PR_LOG("Fatal error.. exiting\n");
	exit(-1);
}


static int do_exit = 0;


static int no_vis_server = 1;

using namespace std;

typedef struct connect_info_t {
	pthread_t *thread;
	int connfd;
	struct sockaddr_in cliaddr;

	hub_settings_t *hub_settings;

	pthread_mutex_t update_ready_mutex;
} connect_info_t;


typedef struct connect_node_t {
	connect_info_t *info;
	struct connect_node_t *next;
} connect_node_t;


connect_node_t *cl_head = NULL;
pthread_mutex_t cl_list_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t options_mutex = PTHREAD_MUTEX_INITIALIZER;

static int client_list_changed = 0;

void __client_list_del(connect_info_t *info) {
	connect_node_t *curr;
	connect_node_t *prev;


	pthread_mutex_lock(&cl_list_mutex);

	client_list_changed = 1;

	curr = cl_head;
	prev = NULL;

	if (cl_head == NULL) {
		pthread_mutex_unlock(&cl_list_mutex);
		return;
	}

	while (curr) {
		if (curr->info == info) {
			// printf("DBG: found node \n");
			break;
		}
		prev = curr;
		curr = curr->next;
	}
	
	if (curr == NULL) {
		pthread_mutex_unlock(&cl_list_mutex);
		return;
	}

	if (curr == cl_head) {
		cl_head = cl_head->next;
	} else {
		prev->next = curr->next;
	}

	free(curr);

	pthread_mutex_unlock(&cl_list_mutex);
}


void __client_list_add(connect_info_t *info) {
	connect_node_t *node;

	node = (connect_node_t*)malloc(sizeof(connect_node_t));

	pthread_mutex_lock(&cl_list_mutex);

	client_list_changed = 1;

	node->info = info;
	node->next = cl_head;
	cl_head = node;

	pthread_mutex_unlock(&cl_list_mutex);

	return;
}

void __client_list_update(int options_changed) {
	connect_node_t *tmp;

	pthread_mutex_lock(&cl_list_mutex);

	// If nothing changed in client list and options havent changed
	// then dont send update
	if (client_list_changed == 0 && options_changed == 0) {
		pthread_mutex_unlock(&cl_list_mutex);
		return;
	}

	tmp = cl_head;

	while (tmp != NULL) {
		pthread_mutex_unlock(&tmp->info->update_ready_mutex);
		tmp = tmp->next;
	}

	client_list_changed = 0;
	pthread_mutex_unlock(&cl_list_mutex);

	return;
}




typedef struct vis_server_info_t {
	char *ip;
	int port;
	int v_sockfd;
	struct sockaddr_in addr;
} vis_server_info_t;

#define MAX_NUM_VIS_SERVERS 5
vis_server_info_t vis_server[MAX_NUM_VIS_SERVERS];

int num_vis_servers = 0;


static void __connect_to_visualization(void) {
	int rc;
	int i;

	for (i=0; i<num_vis_servers; i++) {
	
#ifdef VIS_SERVER_TCP_CONNECTION
		PR_LOG("Connecting to visual server %s:%d... \n", vis_server[i].ip, vis_server[i].port);
		vis_server[i].v_sockfd = socket(AF_INET, SOCK_STREAM, 0);
#else
		vis_server[i].v_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#endif

		memset(&vis_server[i].addr, 0x0, sizeof(struct sockaddr_in));

		vis_server[i].addr.sin_family = AF_INET;
		vis_server[i].addr.sin_port = htons(vis_server[i].port);
		vis_server[i].addr.sin_addr.s_addr = inet_addr(vis_server[i].ip);

#ifdef VIS_SERVER_TCP_CONNECTION
		rc = connect(vis_server[i].v_sockfd, (struct sockaddr*)&vis_server[i].addr, sizeof(vis_server[i].addr));

		if (rc != 0) {
			PR_LOG("!!!!!!!!!ERROR: failed to connect to %s:%d!!!!!!!!!!\n", vis_server[i].ip, vis_server[i].port);
		} else {
			PR_LOG("[connected to %s:%d]\n", vis_server[i].ip, vis_server[i].port);
		}
#endif
	}
}

static pthread_mutex_t vis_mutex = PTHREAD_MUTEX_INITIALIZER;

// Thread function to read from a given clients socket
// Clients will send analyzed data back to server
void *client_read_function(void *data) {
	analyzed_info_t *p_data;
	client_packet_t client_packet;
	connect_info_t *c_info;
	int rc;
	visual_packet_t vis_packet;

	c_info = (connect_info_t*)data;

	PR_LOG("Starting read function\n");
	
	int valid_packet = 1;

	while (1) {
	
		valid_packet = 1;
		rc = __recvfrom(c_info->connfd, &client_packet, sizeof(client_packet_t));
		if (rc != 0) {
			PR_LOG("Failed in __recvfrom\n");
			return NULL;
		}


		pthread_mutex_lock(&options_mutex);
		vis_packet.client_id = c_info->hub_settings->client_id;
		pthread_mutex_unlock(&options_mutex);

		p_data = &client_packet.data;

		
		// Verify packet
		{
			unsigned char exp_checksum;
			unsigned char actual_checksum;

			actual_checksum = client_packet.checksum;
			exp_checksum = __calc_checksum(p_data);

			if (actual_checksum != exp_checksum) {
				PR_LOG("Packet checksum from client [id = %d] doesnt match\n", vis_packet.client_id);
				PR_LOG("expected = 0x%x actual = 0x%x\n\n", exp_checksum, actual_checksum);
		
				valid_packet = 0;
			}

			if (client_packet.header.signature != CLIENT_HEADER_SIGNATURE) {
				PR_LOG("Packet signature failure for client [id = %d]; got 0x%x\n",
							vis_packet.client_id,
							 client_packet.header.signature);
				valid_packet = 0;
			}

		}


		int retry_count = 0;
		if (!valid_packet) {
			client_packet_header_t hdr;

			PR_LOG("Packet was invalid... attempting recovery\n");
			while (1) {
				rc = __recvfrom(c_info->connfd, &hdr, sizeof(hdr));
				if (rc != 0) {
					PR_LOG("Failed in __recvfrom for signature retrieval\n");
				}

				// If we found client header signature, read the rest of the packet
				if (hdr.signature == CLIENT_HEADER_SIGNATURE) {
					PR_LOG("Packet signature detected\n");
					rc = __recvfrom(c_info->connfd, 
								((unsigned char*)&client_packet) + sizeof(hdr), 
								sizeof(client_packet_t) - sizeof(hdr));

					if (client_packet.checksum == __calc_checksum(&client_packet.data)) {
						break;
					}
				} else {
					PR_LOG("Not a signature.. retrying\n");
				}
			}

			retry_count++;
			
			if (retry_count > sizeof(client_packet_t)/sizeof(client_packet_header_t)) {
				PR_LOG("FATAL: Retry count exceeded.. signature not spotted\n");
				__dump_log_and_exit();
			}
		}

		pthread_mutex_lock(&vis_mutex);


		memcpy(&vis_packet.data, p_data, sizeof(analyzed_info_t));


		if (vis_packet.data.frame_id == 0) {
			PR_LOG("FRAME ID was 0 for client %d packet was %lu\n", vis_packet.client_id, sizeof(analyzed_info_t));
		}

		if (!no_vis_server) {
			int i;
			vis_packet.zone_id = __lookup_zone_id(vis_packet.client_id, vis_packet.data.camera_id);
			// Send same packet to all servers
			for (i=0; i<num_vis_servers; i++) {		
				rc = sendto(vis_server[i].v_sockfd, &vis_packet, sizeof(visual_packet_t), MSG_DONTWAIT, (struct sockaddr*)&vis_server[i].addr, sizeof(vis_server[i].addr));
				if (rc < 0) {
					PR_LOG("Failed to send data to visual server %s:%d\n",
							vis_server[i].ip, vis_server[i].port);
	//				__dump_log_and_exit();
				} 
			}
		}

		pthread_mutex_unlock(&vis_mutex);
		
	}
	
}

// Main client thread function; spawned for each client
// Spawns 'read' thread for the client, and sends periodic updates to the client regarding clients settings
void *client_function(void *data) {
	hub_settings_t current_hub_settings;
	int rc;
	pthread_t recv_thread;
	connect_info_t *c_info;

	c_info = (connect_info_t*)data;

	pthread_create(&recv_thread, 0, client_read_function, c_info);

	while (1) {

		// We dont send anything until there is an update ready
		pthread_mutex_lock(&c_info->update_ready_mutex);

		pthread_mutex_lock(&options_mutex);
		current_hub_settings = *(c_info->hub_settings);
		pthread_mutex_unlock(&options_mutex);

		pthread_mutex_unlock(&c_info->update_ready_mutex);

		rc = sendto(c_info->connfd, &current_hub_settings, sizeof(hub_settings_t), 0, (struct sockaddr *)&c_info->cliaddr,sizeof(c_info->cliaddr));

		
		if (rc == 0 || rc == -1) {
			PR_LOG("Client %s is no longer alive\n", c_info->hub_settings->ip_address);
			break;
		}

		pthread_mutex_lock(&c_info->update_ready_mutex);
	}

	pthread_join(recv_thread, NULL);
	shutdown(c_info->connfd, SHUT_RDWR);
	close(c_info->connfd);

	PR_LOG("Client %s exiting...\n", c_info->hub_settings->ip_address);

	__client_list_del(c_info);

	pthread_mutex_destroy(&c_info->update_ready_mutex);
	free(c_info->thread);
	free(c_info);

	return NULL;
}


/* Starts listening server on SERVER_PORT port */
void *start_server(void* arg)
{
	int listenfd,connfd;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t clilen;
	char client_name[INET6_ADDRSTRLEN];
	hub_settings_t *hub_settings;

	int rc;
	int client_id;
	

	pthread_t *client_thread;
	connect_info_t *p_connect_info;

	if (!no_vis_server) {
		__connect_to_visualization();
	}

	PR_LOG("Initializing main server...\n");
	listenfd = socket(AF_INET,SOCK_STREAM,0);

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);
	rc = bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

	if (rc == -1) {
		perror("bind error: ");
		return NULL;
	}

	rc = listen(listenfd,1024);
	if (rc == -1) { 
		perror("Listen error: ");
		return NULL;
	}

	
	PR_LOG("Accepting connections on main server...\n");
	// Repeat until we get killed
	while (do_exit == 0) {
		clilen = sizeof(cliaddr);
		connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen);

		if (connfd == -1) {
			PR_LOG("ACCEPT ERROR\n");
			continue;
		}

		
		client_id = -1;
		hub_settings = NULL;

		__recvfrom(connfd, &client_id, sizeof(int));
		
		PR_LOG("Received client id = %d\n", client_id);

		// Extract clients IP address 
		if (inet_ntop(AF_INET, &cliaddr.sin_addr.s_addr, client_name, sizeof(client_name)) != NULL) {
			PR_LOG("Client connected; Address = %s\n", client_name);
			
			// Lookup client based on its stringified address
			// hub_settings = __get_hub_settings(client_name);
		}

		if (client_id != -1) {
			hub_settings = __get_client_settings(client_id);
		}

		if (hub_settings == NULL) {
			PR_LOG("ERROR: unknown client connected from IP %s\n", client_name);
			continue;
		}
		

		// Allocate new structure that holds all client information
		p_connect_info = (connect_info_t*)malloc(sizeof(connect_info_t));

		client_thread = (pthread_t*)malloc(sizeof(pthread_t));
		p_connect_info->thread = client_thread;
		p_connect_info->connfd = connfd;
		memcpy(&p_connect_info->cliaddr, &cliaddr, sizeof(cliaddr));
		p_connect_info->hub_settings = hub_settings;

		pthread_mutex_init(&p_connect_info->update_ready_mutex, NULL);

		pthread_mutex_lock(&p_connect_info->update_ready_mutex);

		// Add client to list of clients
		__client_list_add(p_connect_info);

		//	__dbg_init_analyzed_data(p_connect_info);

		// Spawn a thread for reading/writing to socket for the given client
		pthread_create(client_thread, NULL, client_function, p_connect_info);

	}


	PR_LOG("-Server Exited-\n");
	return NULL;
}



static void sighandler(int val)
{
	if (val == SIGTERM || val == SIGINT) {
		do_exit = 1;
	}
}

static void signals_init(void)
{
	struct sigaction action;

	action.sa_handler = sighandler;
	action.sa_flags = 0;
	sigemptyset (&action.sa_mask);
	sigaction (SIGINT, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
	sigaction (SIGHUP, &action, NULL);
	sigaction (SIGPIPE, &action, NULL);
}


static void __opt_dump_stats(void) {
	printf("\n");
	connect_node_t *tmp;
	struct sockaddr_in *p_addr;

	char client_name[INET6_ADDRSTRLEN];
	pthread_mutex_lock(&cl_list_mutex);

	tmp = cl_head;

	printf("-----------STATS-------------\n");
	while (tmp != NULL) {
	
		p_addr = &tmp->info->cliaddr;	
		if (inet_ntop(AF_INET, &p_addr->sin_addr.s_addr, client_name, sizeof(client_name)) != NULL) {
			printf("IP = %s\n", client_name);
		}
		tmp = tmp->next;
	}
	printf("-----------------------------\n\n");
	pthread_mutex_unlock(&cl_list_mutex);

}

void *handle_options(void *data) {
	option_packet_t option_packet;
	int rc;

	while (1) {
		memset(&option_packet, 0x0, sizeof(option_packet));

		rc = __recvfrom(options_socket_fd, &option_packet, sizeof(option_packet));		
		if (rc != 0) {
			PR_LOG("Failed to recieve option.. ignoring\n");
			continue;
		}

		switch (option_packet.id) {
			case OPTION_DUMP_STATS:
				__opt_dump_stats();
				break;
			default:
				PR_LOG("Unknown option recieved %d\n", option_packet.id);
				break;
		}
	}
}



int main(int argc, char **argv)
{
	int update_options = 0;
	pthread_t main_server_thread;
	int i;
	vis_server_info_t *vinfo;
	pthread_t options_thread;

	gettimeofday(&tv_start, 0);
	

	options_socket_fd = __create_udp_listen_socket(OPTION_PORT_MAIN_SERV);
	pthread_create(&options_thread, 0, handle_options, 0);

	if (argc == 1) {

	} else if ((argc >= 3) && (argc % 2 == 1)) {

		for (i = 1; i < argc; i += 2) {	
			vinfo = &vis_server[num_vis_servers];

			vinfo->ip = argv[i];	
			vinfo->port = atoi(argv[i+1]);

			num_vis_servers++;
		}
		no_vis_server = 0;

	} else {

		printf("usage: ./server [visual_server_ip] [visual_server_port] [v_ip2] [port2] [vip3] [port3] ...\n");
		return -1;
	}

	signals_init();

	__parse_options();
	//__print_options(); // DEBUG

	pthread_create(&main_server_thread, NULL, start_server, NULL);

	while (1) {
		
		update_options = 0;
		pthread_mutex_lock(&options_mutex);
		
		if (__parse_options() == 0) {
			update_options = 1;
		}
		pthread_mutex_unlock(&options_mutex);

		// Update clients with new options if needed
		__client_list_update(update_options);

		//TODO: check if clients have been active
		{
		}


		usleep(50000);
	}

	return 0;
}
