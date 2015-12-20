#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "parse.h"

settings_t global_settings;

#define	SHOW_ERROR_DBG_INFO() printf("line: %s value: %s\n", line_name, line_value);

settings_t *__get_global_settings(void) {
	return &global_settings;
}

int __lookup_hub_id(char *name)
{
	int i;

	// TODO: protect with semaphores 
	for (i=0; i<global_settings.num_hubs; i++) {
		if (strcmp(global_settings.hub_setting[i].ip_address, name) == 0) {
			printf("HUB ID lookup returned index %d\n", i);

			return i;
		}
	}


	printf("Failed to lookup HUB ID\n");

	printf("Incomming data was ip=|%s|\n", name);
	{
		for (i=0; i<global_settings.num_hubs; i++) {
			printf("---|%s|---\n", global_settings.hub_setting[i].ip_address);
		}
	}	

	return -1;
}


hub_settings_t *__get_client_settings(int client_id) {
	int i;

	for (i=0; i<global_settings.num_hubs; i++) {
		if (global_settings.hub_setting[i].client_id == client_id) {
			return &global_settings.hub_setting[i];
		}
	}

	return NULL;
}


hub_settings_t *__get_hub_settings(char *name) 
{
	int id;

	id = __lookup_hub_id(name);

	if (id >= 0) {
		return &global_settings.hub_setting[id];
	}

	return NULL;
}

void __init_options(settings_t *o)
{
	int i;
	int k;
	int j;
	hub_settings_t *h;
	camera_settings_t *c;

	o->num_hubs = -1;
	
	for (i = 0; i<NUM_HUBS; i++) {

		h = &o->hub_setting[i];

		memset(h->ip_address, 0x0, sizeof(h->ip_address));

		h->client_id = -1;
		
		c = &o->default_setting;
		c->brightness = -1;
		c->contrast = -1;
		c->saturation = -1;
		c->zoom_absolute = -1;
		c->exposure_absolute = -1;
		c->exposure_auto = -1;
		c->focus_absolute = -1;
		c->focus_auto = -1;
		c->white_balance_temperature_auto = -1;
		c->white_balance_temperature = -1;

		for (j = 0; j<NUM_COLORS_PER_CAMERA; j++) {
			c->color_range[j].h_low = -1;
			c->color_range[j].h_high = -1;
			c->color_range[j].s_low = -1;
			c->color_range[j].s_high = -1;
			c->color_range[j].s_low = -1;
			c->color_range[j].s_high = -1;
		}


		for (k=0; k<NUM_CAMERAS_PER_HUB; k++) {
			c = &h->camera_settings[k];

			c->brightness = -1;
			c->contrast = -1;
			c->saturation = -1;


			c->zoom_absolute = -1;
			c->exposure_absolute = -1;
			c->exposure_auto = -1;
			c->focus_absolute = -1;
			c->focus_auto = -1;
			c->white_balance_temperature_auto = -1;
			c->white_balance_temperature = -1;
		
			for (j = 0; j<NUM_COLORS_PER_CAMERA; j++) {
				c->color_range[j].h_low = -1;
				c->color_range[j].h_high = -1;
				c->color_range[j].s_low = -1;
				c->color_range[j].s_high = -1;
				c->color_range[j].s_low = -1;
				c->color_range[j].s_high = -1;
			}

			c->spotlight.x0 = -1;
			c->spotlight.y0 = -1;
			c->spotlight.x1 = -1;
			c->spotlight.y1 = -1;
		}

	}
}


int __parse_options(void)
{
	FILE *f;
	int rc;

	char line_name[255];
	char line_value[255];
	
	int hub_id = -1;
	int camera_id = -1;
	int color_id = -1;
	int default_color_id = -1;

	camera_settings_t *p_camera_settings = NULL;
	settings_t old_settings;

	old_settings = global_settings;


	f = fopen(GLOBAL_OPTIONS_FILE, "r");
	
	/* Can legally happen when file is being re-created 
		Not a fatal error as we will read options on next iteration
	*/
	if (f == NULL) {
		return 1;
	}

	__init_options(&global_settings);

	global_settings.num_hubs = 0;

	camera_settings_t *d;

	d = &global_settings.default_setting;

	while (1) {
		rc = fscanf(f, "%s = %s\n", line_name, line_value);
		
		if (rc == EOF) break;
	//	printf("Analyzing |%s| |%s|\n", line_name, line_value);

#define PARSE_DEFAULT_SETTING(x) \
if (strcmp(line_name, "default_"#x) == 0) { \
	d->x = atoi(line_value); \
	continue; \
}

		PARSE_DEFAULT_SETTING(zoom_absolute);
		PARSE_DEFAULT_SETTING(exposure_absolute);
		PARSE_DEFAULT_SETTING(exposure_auto);
		PARSE_DEFAULT_SETTING(focus_absolute);
		PARSE_DEFAULT_SETTING(focus_auto);
		PARSE_DEFAULT_SETTING(white_balance_temperature_auto);
		PARSE_DEFAULT_SETTING(white_balance_temperature);
		PARSE_DEFAULT_SETTING(brightness);
		PARSE_DEFAULT_SETTING(contrast);
		PARSE_DEFAULT_SETTING(saturation);

		if (strcmp(line_name, "default_track_color") == 0) {
			int h_high, h_low, s_high, s_low, v_high, v_low;

			default_color_id++;

			rc = sscanf(line_value, "%d,%d,%d,%d,%d,%d",
				&h_low, &h_high, &s_low, &s_high, &v_low, &v_high);
			
			d->color_range[default_color_id].h_low = h_low;
			d->color_range[default_color_id].h_high = h_high;
			d->color_range[default_color_id].s_low = s_low;
			d->color_range[default_color_id].s_high = s_high;
			d->color_range[default_color_id].v_low = v_low;
			d->color_range[default_color_id].v_high = v_high;
			continue;
		}


		if (strcmp(line_name, "[hub]") == 0) {
			hub_id++;
			camera_id = -1;
			global_settings.num_hubs++;
			global_settings.hub_setting[hub_id].hub_id = hub_id;
			continue;
		}



		
		if (line_name[0] == '#') continue;
		if (hub_id < 0) {
			printf("Error... options listed before hub defined!\n");
			SHOW_ERROR_DBG_INFO();
			continue;
		}


		if (strcmp(line_name, "ip") == 0) {
			strcpy(global_settings.hub_setting[hub_id].ip_address, line_value);
			//printf("Found hub with ip %s\n", line_value);
			continue;
		}

		if (strcmp(line_name, "client_id") == 0) {
			global_settings.hub_setting[hub_id].client_id = atoi(line_value);
			continue;
		}		

		if (strcmp(line_name, "[camera]") == 0) {
			camera_id++;
			color_id = -1;
			continue;
		}


		if (camera_id >= 0 && camera_id <= 3) {
			p_camera_settings = &(global_settings.hub_setting[hub_id].camera_settings[camera_id]);
		} else  {
			printf("ERROR: camera options defined before camera id!\n");
			SHOW_ERROR_DBG_INFO();
			continue;
		}




		if (strcmp(line_name, "exposure_absolute") == 0) {
			p_camera_settings->exposure_absolute = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "saturation") == 0) {
			p_camera_settings->saturation = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "brightness") == 0) {
			p_camera_settings->brightness = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "contrast") == 0) {
			p_camera_settings->contrast = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "track_color") == 0) {
			int h_high, h_low, s_high, s_low, v_high, v_low;

			color_id++;

			rc = sscanf(line_value, "%d,%d,%d,%d,%d,%d",
				&h_low, &h_high, &s_low, &s_high, &v_low, &v_high);
			
			p_camera_settings->color_range[color_id].h_low = h_low;
			p_camera_settings->color_range[color_id].h_high = h_high;
			p_camera_settings->color_range[color_id].s_low = s_low;
			p_camera_settings->color_range[color_id].s_high = s_high;
			p_camera_settings->color_range[color_id].v_low = v_low;
			p_camera_settings->color_range[color_id].v_high = v_high;
			continue;
		} else if (strcmp(line_name, "zoom_absolute") == 0) {
			p_camera_settings->zoom_absolute = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "exposure_absolute") == 0) {
			p_camera_settings->exposure_absolute = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "exposure_auto") == 0) {
			p_camera_settings->exposure_auto = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "focus_auto") == 0) {
			p_camera_settings->focus_auto = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "focus_absolute") == 0) {
			p_camera_settings->focus_absolute = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "white_balance_temperature_auto") == 0) {
			p_camera_settings->white_balance_temperature_auto = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "white_balance_temperature") == 0) {
			p_camera_settings->white_balance_temperature = atoi(line_value);
			continue;
		} else if (strcmp(line_name, "spotlight") == 0) {
			int x0,y0,x1,y1;
			rc = sscanf(line_value, "%d,%d,%d,%d", &x0,&y0,&x1,&y1);

			p_camera_settings->spotlight.x0 = x0;
			p_camera_settings->spotlight.y0 = y0;
			p_camera_settings->spotlight.x1 = x1;
			p_camera_settings->spotlight.y1 = y1;
			continue;
		}
	

		printf("Unknown option %s\n", line_name);

	}

	int i,j,c;
	for (i=0; i<global_settings.num_hubs; i++) {
		for (j=0; j<NUM_CAMERAS_PER_HUB; j++) {
			p_camera_settings = &global_settings.hub_setting[i].camera_settings[j];

			// Set default value
			#define SET_DEFAULT_SETTING(x) if (p_camera_settings->x == -1) { p_camera_settings->x = global_settings.default_setting.x; }

			SET_DEFAULT_SETTING(zoom_absolute);
			SET_DEFAULT_SETTING(exposure_absolute);
			SET_DEFAULT_SETTING(exposure_auto);
			SET_DEFAULT_SETTING(focus_absolute);
			SET_DEFAULT_SETTING(focus_auto);
			SET_DEFAULT_SETTING(white_balance_temperature_auto);
			SET_DEFAULT_SETTING(white_balance_temperature);
			SET_DEFAULT_SETTING(brightness);
			SET_DEFAULT_SETTING(contrast);
			SET_DEFAULT_SETTING(saturation);
			
			for (c = 0; c < NUM_COLORS_PER_CAMERA; c++) {
				if (p_camera_settings->color_range[c].h_low == -1) {
					p_camera_settings->color_range[c] = global_settings.default_setting.color_range[c];

				}
			}
		}
	}


	fclose(f);
	



	if (memcmp(&old_settings, &global_settings, sizeof(settings_t)) == 0) {
		return 1;
	}

	return 0;
}



void __print_options(void)
{
	int i;

	for (i = 0; i < global_settings.num_hubs; i++) {
		__print_hub_settings(&global_settings.hub_setting[i]);
	}
}


