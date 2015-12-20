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

typedef struct opt_info_t {
	const char *option_name;
	option_id_t oid;
	int num_args;
} opt_info_t;

opt_info_t opt_info[] = {
	{"set_def_brightness", OID_SET_DFLT_BRIGHTNESS, 1},
	{"set_def_contrast", OID_SET_DFLT_CONTRAST, 1},
	{"set_def_saturation", OID_SET_DFLT_SATURATION, 1},
	{"set_def_focus_abs", OID_SET_DFLT_FOCUS_ABSOLUTE, 1},
	{"set_def_zoom_abs", OID_SET_DFLT_ZOOM_ABSOLUTE, 1},
	{"set_def_exposure_abs", OID_SET_DFLT_EXPOSURE_ABSOLUTE, 1},
	{"set_def_white_balance", OID_SET_DFLT_WHITE_BALANCE_TEMP, 1},
	{"dump_stats", OID_DUMP_STATS, 0},
	{"generate_config", OID_GENERATE_CONFIG, 0},
};
#define OPT_SIZE (sizeof(opt_info)/sizeof(opt_info[0]))

int main(int argc, char **argv)
{
	send_udp_sock_t socket;
	option_packet_t packet;
	option_id_t opt_id;

	visual_packet_t p;
	char *ip;
	int port;
	char *id;
	char *cmd;
	int i, k;
	
	
	if (argc < 4) {
		printf("Usage: ./api_app <ip> <id> <command> [arguments]\n");
		printf("ID: main_server, map_server, vis_server, client, opt_server\n");
		printf("CMDs:\n");
		
		for (i=0; i<OPT_SIZE; i++) {
			printf("\t%s\n", opt_info[i].option_name);
		}
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
	} else if (strcmp(id, "opt_server") == 0) {
		port = OPTION_PORT_OPT_SERV;
	} else {
		printf ("Unknown entry %s\n", id);
		return -1;
	}



	packet.id = OID_NONE;
	for (i=0; i<OPT_SIZE; i++) {
		if (strcmp(opt_info[i].option_name, cmd) == 0) {
			packet.id = opt_info[i].oid;
			
			for (k=0; k < opt_info[i].num_args; k++) {
				packet.arg[k] = atoi(argv[4 + k]);
			}
			break;
		}
	}
	if (packet.id == OID_NONE) {
		printf("Unknown command |%s|\n", cmd);
		return -1;
	}

	__create_udp_send_socket(ip, port, &socket);
	
	
	__send_udp_packet(&socket, &packet, sizeof(packet));
	printf("Option sent\n");

	return 0;
}
