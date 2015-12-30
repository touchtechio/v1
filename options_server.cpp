#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>


#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
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

#define WRITE_CFG(x...) fprintf(file, x);
//#define WRITE_CFG(x...) printf(x);

#define PR_DEF(x) if (s->default_setting.x != -1) WRITE_CFG("default_%s = %d\n", #x, s->default_setting.x)
#define PR_SET(x) if (h->camera_settings[j].x != -1) WRITE_CFG("\t\t%s = %d\n", #x, h->camera_settings[j].x)

int fd;

static void __generate_cfg_file(settings_t *s) {
	int i,j;
	color_range_t *c;
	FILE *file;

	fd = open(GLOBAL_OPTIONS_FILE, O_WRONLY | O_TRUNC);
	if (fd<=0) {
		printf("Failed to open file for writing.. returning\n");
		return;
	}

	flock(fd, LOCK_EX);

	do {
		file = fopen(GLOBAL_OPTIONS_FILE, "w");
	} while (file == NULL);

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

	WRITE_CFG("\n\n");

	hub_settings_t *h;
	spotlight_t *spot;

	for (i=0; i<NUM_COLORS_PER_CAMERA; i++) {
		c = &s->default_setting.color_range[i];

		if (c->h_low != -1) {
			WRITE_CFG("default_track_color = %d,%d,%d,%d,%d,%d\n",
					c->h_low, c->h_high,
					c->s_low, c->s_high,
					c->v_low, c->v_high);					
		}
	}

	int k;
	WRITE_CFG("\n\n");
	for (i=0; i<s->num_hubs; i++) {

		h = &s->hub_setting[i];
		WRITE_CFG("[hub]\n");
		WRITE_CFG("\tclient_id = %d\n", h->client_id);
		WRITE_CFG("\n");


		for (j=0; j<NUM_CAMERAS_PER_HUB; j++) { 
			WRITE_CFG("\t[camera]\n");
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

			for (k=0; k<NUM_COLORS_PER_CAMERA; k++) {
				c = &h->camera_settings[j].color_range[k];

				if (c->h_low != -1) {
					WRITE_CFG("\t\ttrack_color = %d,%d,%d,%d,%d,%d\n",
						c->h_low, c->h_high,
						c->s_low, c->s_high,
						c->v_low, c->v_high);					
				}
			}

			if (spot->x0 != -1) {
				WRITE_CFG("\t\tspotlight = %d,%d,%d,%d\n",spot->x0, spot->y0, spot->x1, spot->y1);
			}
			WRITE_CFG("\n");
		}
		WRITE_CFG("\n");
		
	}

	WRITE_CFG("\n\n");
	flock(fd, LOCK_UN);
	close(fd);
	fclose(file);
}


int main(int argc, char **argv)
{
	settings_t *settings;
	int rc;
	int listen_fd;
	option_packet_t packet;
	camera_settings_t *s;
	int gen_cfg_file = 0;
	int i,j;

	settings = __get_global_settings();
	__init_options(settings);

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



	while (1) {
		gen_cfg_file = 0;
		__recvfrom(listen_fd, &packet, sizeof(packet));

		switch (packet.id) {
		case OID_NEW_SETTINGS:
			printf("Got settings\n");
			{
				int i, zone_id, client_id;

				s = &settings->default_setting;
				memcpy(s, &packet.settings[0], sizeof(camera_settings_t));

				settings->num_hubs = 15;
				for (i=0; i<30; i++) {
					hub_settings_t *hub_settings;

					zone_id = i+1;
					client_id = __lookup_client_id(zone_id);
					hub_settings  = &settings->hub_setting[client_id - 1];
					hub_settings->client_id = client_id;

					s = &(hub_settings->camera_settings[__lookup_camera_id(zone_id)]);
					memcpy(s, &packet.settings[zone_id], sizeof(camera_settings_t));
				}

			}
			gen_cfg_file = 1;
			break;
		}

		if (gen_cfg_file) {
			__generate_cfg_file(settings);
		}
	}

	return 0;
}
