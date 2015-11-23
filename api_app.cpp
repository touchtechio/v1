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


int main(int argc, char **argv)
{
	send_udp_sock_t socket;
	option_packet_t packet;
	option_id_t opt_id;

	char *ip;
	int port;
	char *id;
	char *cmd;

	if (argc != 4) {
		printf("Usage: ./api_app <ip> <id> <command>\n");
		printf("ID: main_server, map_server, vis_server, client\n");
		printf("CMD: dump_stats\n");
		return -1;
	}

	ip = argv[1];
	id = argv[2];
	cmd = argv[3];

	if (strcmp(id, "main_server") == 0) {
		port = OPTION_PORT_MAIN_SERV;
	} else if (strcmp(id, "map_srver") == 0) {
		port = OPTION_PORT_MAP_SERV;
	} else if (strcmp(id, "vis_server") == 0) {
		port = OPTION_PORT_VIS_SERV;
	} else if (strcmp(id, "client") == 0) {
		port = OPTION_PORT_CLIENT;
	} else {
		printf ("Unknown entry %s\n", id);
		return -1;
	}

	if (strcmp(cmd, "dump_stats") == 0) {
		opt_id = OPTION_DUMP_STATS;
	} else {
		printf("Unknown command %s\n", cmd);
		return -1;
	}

	__create_udp_send_socket(ip, port, &socket);
	
	
	packet.id = opt_id;
	__send_udp_packet(&socket, &packet, sizeof(packet));
	printf("Option sent\n");

	return 0;
}
