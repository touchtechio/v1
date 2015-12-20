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


#define PR_DEF(x) printf("default_%s = %d\n", #x, s->default_setting.x)

static void __generate_cfg_file(settings_t *s, const char *filename) {
	int i,j;
	color_range_t *c;

	PR_DEF(exposure_auto);
	PR_DEF(exposure_absolute);
	PR_DEF(zoom_absolute);
	PR_DEF(focus_auto);
	PR_DEF(focus_absolute);
	PR_DEF(white_balance_temperature_auto);
	PR_DEF(white_balance_temperature);
	PR_DEF(brightness);
	PR_DEF(contrast);
	PR_DEF(saturation);

	printf("\n\n");

	hub_settings_t *h;
	spotlight_t *spot;
	getchar();
	for (i=0; i<NUM_COLORS_PER_CAMERA; i++) {
		c = &s->default_setting.color_range[i];

		if (c->h_low != -1) {
			printf("default_track_color = %d,%d,%d,%d,%d,%d\n",
					c->h_low, c->h_high,
					c->s_low, c->s_high,
					c->v_low, c->v_high);					
		}
	}

	for (i=0; i<s->num_hubs; i++) {

		h = &s->hub_setting[i];
		printf("[hub]\n");
		printf("\tclient_id = %d\n", h->client_id);
		printf("\n");

		#define PR_SET(x) if (h->camera_settings[j].x != -1) printf("\t\t%s = %d\n", #x, h->camera_settings[j].x)

		for (j=0; j<NUM_CAMERAS_PER_HUB; j++) { 
			printf("\t[camera]\n");
			spot = &h->camera_settings[j].spotlight;

			PR_SET(exposure_auto);
			PR_SET(focus_auto);
			PR_SET(white_balance_temperature_auto);
			
			PR_SET(zoom_absolute);
			PR_SET(exposure_absolute);
			PR_SET(focus_absolute);
			PR_SET(white_balance_temperature);

			PR_SET(brightness);
			PR_SET(contrast);
			PR_SET(saturation);
			printf("\t\tspotlight = %d,%d,%d,%d\n",spot->x0, spot->y0, spot->x1, spot->y1);
			printf("\n");
		}
		printf("\n");
		
	}

	printf("\n\n");
}


int main(int argc, char **argv)
{
	settings_t *settings;
	int rc;
	int listen_fd;
	option_packet_t packet;
	camera_settings_t *dflt;
	int gen_cfg_file = 0;
	int i,j;

	do {
		rc = __parse_options();
	} while (rc != 0);

	settings = __get_global_settings();

#define VERIFY_SETTING(x) if (settings->hub_setting[i].camera_settings[j].x == settings->default_setting.x) settings->hub_setting[i].camera_settings[j].x = -1;

	for (i=0; i<settings->num_hubs; i++) {
		for (j=0; j<NUM_CAMERAS_PER_HUB; j++) {
			VERIFY_SETTING(exposure_auto);
			VERIFY_SETTING(focus_auto);
			VERIFY_SETTING(white_balance_temperature_auto);
			
			VERIFY_SETTING(zoom_absolute);
			VERIFY_SETTING(exposure_absolute);
			VERIFY_SETTING(focus_absolute);
			VERIFY_SETTING(white_balance_temperature);

			VERIFY_SETTING(brightness);
			VERIFY_SETTING(contrast);
			VERIFY_SETTING(saturation);
		}
	}	

	listen_fd = __create_udp_listen_socket(OPTION_PORT_OPT_SERV);

	dflt = &settings->default_setting;

	while (1) {
		gen_cfg_file = 0;
		__recvfrom(listen_fd, &packet, sizeof(packet));

		switch (packet.id) {
		case OID_SET_DFLT_BRIGHTNESS:
			dflt->brightness = packet.arg[0];
			break;
		case OID_SET_DFLT_SATURATION:
			dflt->saturation = packet.arg[0];
			break;
		case OID_SET_DFLT_CONTRAST:
			dflt->contrast = packet.arg[0];
			break;
		case OID_GENERATE_CONFIG:
			gen_cfg_file = 1;
			break;
		}

		if (gen_cfg_file) {
			__generate_cfg_file(settings, "none");
		}
	}

	return 0;
}
