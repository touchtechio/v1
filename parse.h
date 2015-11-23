
#ifndef __PARSE_H__
#define __PARSE_H__

#define GLOBAL_OPTIONS_FILE "camera_options.cfg"

#define NUM_HUBS 40
#define NUM_CAMERAS_PER_HUB 2
//#define NUM_SPOTLIGHTS_PER_CAMERA 1
#define NUM_COLORS_PER_CAMERA 3

typedef struct spotlight_t {
	int x0;
	int y0;
	int x1;
	int y1;
} spotlight_t;

typedef struct color_range_t {
	int h_low, h_high;
	int s_low, s_high;
	int v_low, v_high;
} color_range_t;


typedef struct camera_settings_t {
	int zoom_absolute;
	int exposure_absolute;
	int exposure_auto;
	int focus_absolute;
	int focus_auto;

	int white_balance_temperature_auto;
	int white_balance_temperature;

	int brightness;
	int contrast;
	int saturation;

	color_range_t color_range[NUM_COLORS_PER_CAMERA];
	//spotlight_t spotlight[NUM_SPOTLIGHTS_PER_CAMERA];
	spotlight_t spotlight;

} camera_settings_t;


typedef struct hub_settings_t {
	char ip_address[100];
	int hub_id;
	int client_id;
	camera_settings_t camera_settings[NUM_CAMERAS_PER_HUB];
} hub_settings_t;


typedef struct settings_t {
	hub_settings_t hub_setting[NUM_HUBS];
	int num_hubs;
	camera_settings_t default_setting;
} settings_t;


int __lookup_hub_id(char *name);
int __parse_options(void);
hub_settings_t *__get_hub_settings(char *name);

inline void __print_hub_settings(hub_settings_t *h) 
{
	camera_settings_t *p_cam;
	int j,k;

	printf("[hub]\n");
	printf("\tip = %s\n", h->ip_address);
	printf("\tclient_id = %d\n", h->client_id);

	for (k = 0; k < NUM_CAMERAS_PER_HUB; k++) {
		p_cam = &h->camera_settings[k];
		printf("\t\t[camera%d]\n", k);
		printf("\t\texposure_absolute = %d\n", p_cam->exposure_absolute);
		printf("\t\tsaturation = %d\n", p_cam->saturation);

		printf("\t\tspotlight = %d,%d,%d,%d\n", 
				p_cam->spotlight.x0,
				p_cam->spotlight.y0,
				p_cam->spotlight.x1,
				p_cam->spotlight.y1);

		for (j = 0; j < NUM_COLORS_PER_CAMERA; j++) {
			printf("\t\t\t[COLOR = %d,%d,%d,%d,%d,%d]\n",
						p_cam->color_range[j].h_low,
						p_cam->color_range[j].h_high,
						p_cam->color_range[j].s_low,
						p_cam->color_range[j].s_high,
						p_cam->color_range[j].v_low,
						p_cam->color_range[j].v_high);
		}
			
	}
}


hub_settings_t *__get_client_settings(int client_id);


#endif /* __PARSE_H__ */
