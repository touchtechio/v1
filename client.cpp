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
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/stitching/stitcher.hpp"

#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

#include "common.h"

int do_visual = 0;
int do_debug = 0;
int do_show_fps = 0;
int do_show_markers = 0;
int do_show_thresh = 0;
int do_show_spot_selector = 0;


//#define PRX printf
#define PRX(x...) 


#define VISUAL_WIDTH 1280
#define VISUAL_HEIGHT 720

int arg_vis_scale = 2;
int client_id = -1;
char server_ip[100];
int server_port;

int start_dev = 0; // Starting v4l2 device

using namespace cv;
using namespace std;


spotlight_t spotlight[4] = {
	{
		.x0 = 0,
		.y0 = 0,
		.x1 = CAMERA_WIDTH-1,
		.y1 = CAMERA_HEIGHT-1,
	},
	{
		.x0 = 0,
		.y0 = 0,
		.x1 = CAMERA_WIDTH-1,
		.y1 = CAMERA_HEIGHT-1,
	},
	{
		.x0 = 0,
		.y0 = 0,
		.x1 = CAMERA_WIDTH-1,
		.y1 = CAMERA_HEIGHT-1,
	},
	{
		.x0 = 0,
		.y0 = 0,
		.x1 = CAMERA_WIDTH-1,
		.y1 = CAMERA_HEIGHT-1,
	},

};

typedef struct capture_info_t {
	VideoCapture *p_cap;
	int camera_id;
	color_range_t *track_color;
	int frame_id;
} capture_info_t;

capture_info_t capture_info[NUM_CAMERAS_PER_HUB];


// Helper macro to execute v4l2 option
#define EXEC_V4L2_OPTION(option) \
	do { \
		if (hub_settings.camera_settings[i].option != -1) { \
			sprintf(cmd_buffer, "v4l2-ctl -d /dev/video%d -c%s=%d", i + start_dev, #option,\
								hub_settings.camera_settings[i].option); \
			PRX("CMD BUFFER = %s\n", cmd_buffer); \
			if (system(cmd_buffer)) { } \
		} \
	} while (0)


#define DEFINE_COLOR(name, high, low) {high, low, 0, 255, 150, 255}

//Note: colors are wrong

// Initial colors we will track
color_range_t init_track_colors[] = {
	DEFINE_COLOR("C0", 0, 255),
	DEFINE_COLOR("C1", 0, 255),
	DEFINE_COLOR("C2", 0, 255),
	DEFINE_COLOR("C3", 20,50),
	DEFINE_COLOR("C4", 60,87),
	DEFINE_COLOR("C5", 50,55),
	DEFINE_COLOR("C6", 120,140),
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


volatile int do_exit = 0;
static void sig_handler(int val) {
	if (val == SIGTERM || val == SIGINT) {
		do_exit = 1;
		printf("---Client [id=%d] exit signal received---\n", client_id);
	}
}

static void signals_init(void) {
	struct sigaction action;

	action.sa_handler = sig_handler;
	action.sa_flags = 0;
	sigemptyset (&action.sa_mask);
	sigaction (SIGINT, &action, NULL);
	sigaction (SIGTERM, &action, NULL);
	sigaction (SIGHUP, &action, NULL);
	sigaction (SIGPIPE, &action, NULL);
}



Mat full_image(CAMERA_HEIGHT*2, CAMERA_WIDTH*2, CV_8UC3);
Mat full_hsv(CAMERA_HEIGHT*2, CAMERA_WIDTH*2, CV_8UC3);
Mat full_image_copy(CAMERA_HEIGHT*2, CAMERA_WIDTH*2, CV_8UC3);
Mat full_hsv_copy(CAMERA_HEIGHT*2, CAMERA_WIDTH*2, CV_8UC3);

Mat *full_thresh[10];

struct timeval tv1, tv2;
int number_of_frames = 0;

void __setup_cap(VideoCapture *p_cap) {
	pthread_mutex_lock(&mutex);
	p_cap->set(CV_CAP_PROP_FRAME_WIDTH,CAMERA_WIDTH);
	p_cap->set(CV_CAP_PROP_FRAME_HEIGHT,CAMERA_HEIGHT);
	pthread_mutex_unlock(&mutex);
//	p_cap->set(CV_CAP_PROP_FOURCC ,CV_FOURCC('M', 'J', 'P', 'G') );
}


int sockfd;

pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

int dbg_spot_x0[2] = {0,0};
int dbg_spot_y0[2] = {0,0};
int dbg_spot_x1[2] = { CAMERA_WIDTH-1, CAMERA_WIDTH-1 };
int dbg_spot_y1[2] = { CAMERA_HEIGHT-1, CAMERA_HEIGHT-1 };

#define DBG_SPOT_WIN0 "spotlight selection camera 0"
#define DBG_SPOT_WIN1 "spotlight selection camera 1"

void __create_debug_spotlight_win(const char *winname, int id) {
	namedWindow(winname, CV_WINDOW_AUTOSIZE);
	cvCreateTrackbar("X0", winname, &dbg_spot_x0[id], CAMERA_WIDTH-1);
	cvCreateTrackbar("Y0", winname, &dbg_spot_y0[id], CAMERA_HEIGHT-1);
	cvCreateTrackbar("X1", winname, &dbg_spot_x1[id], CAMERA_WIDTH-1);
	cvCreateTrackbar("Y1", winname, &dbg_spot_y1[id], CAMERA_HEIGHT-1);
}

void __create_debug_color_win(const char *win_name, color_range_t *range)
{
	namedWindow(win_name, CV_WINDOW_AUTOSIZE);
	cvCreateTrackbar("h high", win_name, &range->h_high, 179);
	cvCreateTrackbar("h low", win_name, &range->h_low, 179);
	cvCreateTrackbar("s high", win_name, &range->s_high, 255);
	cvCreateTrackbar("s low", win_name, &range->s_low, 255);
	cvCreateTrackbar("v high", win_name, &range->v_high, 255);
	cvCreateTrackbar("v low", win_name, &range->v_low, 255);
}


static color_range_t red_color = {0,179,150,255,90,255};
static color_range_t green_color = {0,0,0,0,0,0};
static color_range_t blue_color = {0,0,0,0,0,0};

void __create_debug_windows(void) {
	if (do_debug) {
		__create_debug_color_win("thresh0 win (red)", &red_color);
		__create_debug_color_win("thresh1 win (green)", &green_color);
		__create_debug_color_win("thresh2 win (blue)", &blue_color);
	}
}


void __send_analyzed_info(analyzed_info_t *info) {
	int rc;
	int i;
	int cam_id;

	client_packet_t packet;

	pthread_mutex_lock(&send_mutex);

	cam_id = info->camera_id;
	packet.header.signature = CLIENT_HEADER_SIGNATURE;

	GET_TIMESTAMP(&info->timestamp_sent);

	for (i=0; i<7; i++) {
		info->color_info[i].h_low = capture_info[cam_id].track_color[i].h_low;
		info->color_info[i].s_low = capture_info[cam_id].track_color[i].s_low;
		info->color_info[i].v_low = capture_info[cam_id].track_color[i].v_low;

		info->color_info[i].h_high = capture_info[cam_id].track_color[i].h_high;
		info->color_info[i].s_high = capture_info[cam_id].track_color[i].s_high;
		info->color_info[i].v_high = capture_info[cam_id].track_color[i].v_high;
	}

	memcpy(&packet.data, info, sizeof(analyzed_info_t));
	packet.checksum = __calc_checksum(info);

	rc = send(sockfd, &packet, sizeof(client_packet_t), MSG_DONTWAIT);

	if (rc == -1) {
		PRX("Failed to send data for camera id %d\n", info->camera_id);
		perror("Error... : ");
	}

	pthread_mutex_unlock(&send_mutex);
}

static void __print_color_range(color_range_t *c) {
	PRX("%d-%d, %d-%d, %d-%d\n", c->h_low, c->h_high, c->s_low, c->s_high, c->v_low, c->v_high);
}

// Single function for reading data from the server
void *client_read_function(void *data)
{

	struct sockaddr_in server_addr;
	hub_settings_t hub_settings;
	int rc;
	int i;
	pthread_t  write_thread;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	int one = 1;
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

	PRX("Sending client id...\n");
	rc = send(sockfd, &client_id, sizeof(client_id), 0);
	if (rc < 0) {
		perror("Failed to send test message\n");
		return NULL;
	}

	sleep(1);
	PRX("Finished sleeping!\n");

	while (1) {
		
		__recvfrom(sockfd, &hub_settings, sizeof(hub_settings_t));

		for (i=0; i<NUM_CAMERAS_PER_HUB; i++) {
			char cmd_buffer[255];
			memset(cmd_buffer, 0x0, 255);

			// Apply options
			EXEC_V4L2_OPTION(exposure_auto);
			EXEC_V4L2_OPTION(exposure_absolute);

			EXEC_V4L2_OPTION(focus_auto);
			EXEC_V4L2_OPTION(focus_absolute);

			EXEC_V4L2_OPTION(white_balance_temperature_auto);
			EXEC_V4L2_OPTION(white_balance_temperature);

			EXEC_V4L2_OPTION(zoom_absolute);

			EXEC_V4L2_OPTION(brightness);
			EXEC_V4L2_OPTION(contrast);
			EXEC_V4L2_OPTION(saturation);

			// Apply spotlight
			{
				spotlight_t *spot;
				spot = &hub_settings.camera_settings[i].spotlight;

				if (spot->x0 >= 0 && spot->x0 < CAMERA_WIDTH &&
					spot->x1 >= 0 && spot->x1 < CAMERA_WIDTH &&
					spot->y0 >= 0 && spot->y0 < CAMERA_HEIGHT &&
					spot->y1 >= 0 && spot->y1 < CAMERA_HEIGHT) {
					spotlight[i] = *spot;
					PRX("setting spot[%d]: %d,%d,%d,%d\n",i, spot->x0, spot->y0, spot->x1, spot->y1);
				} else {
					PRX("Skipping spot[%d]: %d,%d,%d,%d\n",i, spot->x0, spot->y0, spot->x1, spot->y1);
				}


			}

			// Apply color range changes
			{
				int j;

				for (j=0; j<NUM_COLORS_PER_CAMERA; j++) {
				//	if (hub_settings.camera_settings[i].color_range[j].h_low != -1) 
					{

						capture_info[i].track_color[j] = hub_settings.camera_settings[i].color_range[j];
//						__print_color_range(&hub_settings.camera_settings[i].color_range[j]);
					}
				}

				// debug
			}
		}
	}

}


typedef struct draw_colors_t {
	int r,g,b;
} draw_colors_t;


draw_colors_t colors_for_x[] = {
	{255,0,0},
	{0,255,0},
	{0,0,255},
};

// Main function for each cameras thread
void *camera_main(void *data)
{
	int camera_id;
	char win_name[] = "camera0";

	int num_frames= 0;
	Mat frame;
	Mat flip_frame;
	Mat thresh[10];
	int x,y,w,h;
	int i;
	int counter = 0;
	spotlight_t *spot = 0;
	int required_area;
	double fps = 0.0;

	Moments motion_moments;
	capture_info_t *cap_info;
	VideoCapture *p_cap;

	cap_info = (capture_info_t *) data;

	camera_id = cap_info->camera_id;
	p_cap = cap_info->p_cap;

	PRX("Thread for camera %d started\n", camera_id);

	Mat *rgb_portion;
	Mat *hsv_portion;
	Mat *thresh_portion[10];
	Mat prev_frame;

	switch (camera_id) {
	case 0:
		x = 0; y = 0; w = CAMERA_WIDTH; h = CAMERA_HEIGHT;
		break;
	case 2:
		x = 0; y = CAMERA_HEIGHT; w = CAMERA_WIDTH; h = CAMERA_HEIGHT;
		break;
	case 1:
		x = CAMERA_WIDTH; y = 0; w = CAMERA_WIDTH; h = CAMERA_HEIGHT;
		break;
	case 3:
		x = CAMERA_WIDTH; y = CAMERA_HEIGHT; w = CAMERA_WIDTH; h = CAMERA_HEIGHT;
		break;
	}


	if (do_visual) {
		rgb_portion = new Mat(full_image, Rect(x,y,w,h));
	}

	hsv_portion = new Mat(full_hsv, Rect(x,y,w,h));

	spot = &spotlight[camera_id];

	for (i=0; i<NUM_COLORS_PER_CAMERA; i++) {
		thresh_portion[i] = new Mat(*(full_thresh[i]), Rect(x,y,w,h));
	}
	
	win_name[6] = '0' + camera_id;

	__setup_cap(p_cap);

	int low_h, low_s, low_v, high_h, high_s, high_v;
	int area, pos_x, pos_y;
	analyzed_info_t analyzed_info;
	char camera_label[100];

	sprintf(camera_label, "camera_%d", camera_id);

	int spot_x0, spot_x1, spot_y0, spot_y1;
	int real_cam_width;
	int real_cam_height;
	
	while (do_exit == 0) {

		PRX("[%d] >>> before capture\n", camera_id);

#ifdef DO_FLIP
		(*(p_cap)) >> flip_frame;

		flip(flip_frame, frame, 1);
#else 
		(*(p_cap)) >> frame;
#endif

		GET_TIMESTAMP(&analyzed_info.timestamp_gotframe);
		PRX("[%d] <<< after capture\n", camera_id);

		if (frame.empty()) {
			PRX("HAD empty frame on capture for camera %d\n", camera_id);
			continue;
		}

		if (do_show_spot_selector) {
			spot->x0 = dbg_spot_x0[camera_id];
			spot->y0 = dbg_spot_y0[camera_id];
			spot->x1 = dbg_spot_x1[camera_id];
			spot->y1 = dbg_spot_y1[camera_id];
		}

		spot_x0 = spot->x0;
		spot_x1 = spot->x1;
		spot_y0 = spot->y0;
		spot_y1 = spot->y1;

		real_cam_width = (spot_x1 - spot_x0 + 1);
		real_cam_height = (spot_y1 - spot_y0 + 1);

		analyzed_info.camera_id = -1;
		for (i = 0; i < 7; i++) {
			analyzed_info.color_info[i].x = -1;
			analyzed_info.color_info[i].y = -1;
			analyzed_info.color_info[i].area = -1;
		}

		analyzed_info.frame_id++;

		pthread_mutex_lock(&mutex);
	
		if (do_visual) {	
			frame.copyTo(*rgb_portion);
		}

		cvtColor(frame, *hsv_portion, COLOR_BGR2HSV);

		if (do_visual) {
			char text[30];
			putText(*rgb_portion, camera_label, Point(50, 50), FONT_HERSHEY_PLAIN, 2, Scalar(0,255,0), 3, 8);

			if (do_show_fps) {
				sprintf(text, "FPS: %.2f", fps);
				putText(*rgb_portion, text, Point(50, 80), FONT_HERSHEY_PLAIN, 2, Scalar(255,255,255), 2,8);
			}
		}
		
		if (do_visual && (do_show_markers || do_show_spot_selector) )
		{
			rectangle(*rgb_portion, 
						Point(spot_x0, spot_y0),
						Point(spot_x1, spot_y1),
						Scalar(255,255,255),
						1, 8, 0);
		}
		
		pthread_mutex_unlock(&mutex);

		
		if (do_debug) {
			cap_info->track_color[0] = red_color;
			cap_info->track_color[1] = green_color;
			cap_info->track_color[2] = blue_color;
		}

		
		Mat *thresh_spot;
		memset(analyzed_info.map, 0x0, sizeof(analyzed_info.map));
		
		for (i = 0; i< NUM_COLORS_PER_CAMERA; i++) {
			low_h = cap_info->track_color[i].h_low;
			low_s = cap_info->track_color[i].s_low;
			low_v = cap_info->track_color[i].v_low;

			high_h = cap_info->track_color[i].h_high;
			high_s = cap_info->track_color[i].s_high;
			high_v = cap_info->track_color[i].v_high;

			//*(thresh_portion[i]) = Mat::zeros(Size(h,w), CV_8UC1);

			//printf("%d,%d,%d - %d,%d,%d\n", low_h, low_s, low_v, high_h, high_s, high_v);
			inRange(*hsv_portion,
					Scalar(low_h,low_s,low_v), 
					Scalar(high_h, high_s, high_v),
					*(thresh_portion[i]));

		
			thresh_spot = new Mat(*thresh_portion[i], Rect(spot_x0, spot_y0, real_cam_width, real_cam_height));
			motion_moments = moments(*thresh_spot, false);
	
		//	memset(&motion_moments, 0x0, sizeof(motion_moments));
			area = motion_moments.m00 / 255.0;
			
			pos_x = 0; pos_y = 0;
	

		{
			int x,y;
			
			int step_x = (real_cam_width)  / NUM_HORIZ_CELLS;
			int step_y = (real_cam_height) / NUM_VERT_CELLS;
			Mat *cell_mat;

			Moments cell_moments;
			int cell_area;
			int pixel_area;

			int rounded_width;
			int rounded_height;
			int cell_id = 0;

			//if (camera_id == 0) printf("STEPX: %d STEPY: %d, REAL_W: %d REAL_H: %d\n", step_x, step_y, real_cam_width, real_cam_height);

			if (REQUIRED_AREA == 0) {				
				required_area = ((float)step_x * (float)step_y * (CELL_THRESH_PERCENT)/100.0); // % of the area
			} else {
				required_area = REQUIRED_AREA;
			}

			rounded_width = ((real_cam_width) / step_x ) * step_x;
			rounded_height = ((real_cam_height) / step_y) * step_y;

			for (y = 0; y < rounded_height; y += step_y) {
				for (x = 0; x < rounded_width; x += step_x) {

					if (cell_id < NUM_HORIZ_CELLS * NUM_VERT_CELLS) {

						Rect my_rect;

						my_rect = Rect(x,y, step_x, step_y);

						//if (camera_id == 0) printf("%d, %d, %d, %d\n", x,y, step_x, step_y);
						cell_mat = new Mat(*thresh_spot, my_rect);
	
						cell_moments = moments(*cell_mat, false);
						cell_area = cell_moments.m00;

						pixel_area = cell_area / 255;
				
						if (pixel_area >= required_area) {

							analyzed_info.map[cell_id] |= (1<<i); // SET BIT-field specifying color was present

							// Visual stuff
							if (do_visual && do_show_markers) {
								Point p0 = Point(my_rect.tl().x + spot_x0, my_rect.tl().y + spot_y0);
								Point p1 = Point(my_rect.br().x + spot_x0, my_rect.br().y + spot_y0);
								rectangle(*rgb_portion, p0, p1, Scalar(255,255,255),1,8,0);

							}
						
						}
						delete cell_mat;
					}

					cell_id++;
				}
			}
		}


			if (area > required_area) {
				Mat thresh_cpy;

				pos_x = ((int)  (motion_moments.m10 / motion_moments.m00) + spot_x0);
				pos_y = (int)  (motion_moments.m01 / motion_moments.m00) + spot_y0;
			
				area = (100.0 * area) / (real_cam_width * real_cam_height);
			} else {
				pos_x = -1;
				pos_y = -1;
				area = -1;
			}
	
			delete thresh_spot;

			analyzed_info.camera_id = camera_id;
			
			analyzed_info.color_info[i].x = pos_x;
			analyzed_info.color_info[i].y = pos_y;
			analyzed_info.color_info[i].area = area;
			
			

			// For Debug
			if (do_visual && do_show_markers && pos_x != -1) {
				DRAW_X(*rgb_portion, pos_x, pos_y, Scalar(colors_for_x[i].b, colors_for_x[i].g, colors_for_x[i].r), 2);
			}

		}



	//	__dbg_print_analyzed_data(&analyzed_info);

		// Send data to the server
		__send_analyzed_info(&analyzed_info);


		num_frames++;

		// Debug FPS display
		if (do_show_fps && num_frames % 10 == 1) {
			unsigned long delta;

			gettimeofday(&tv2, 0);

			delta = (tv2.tv_sec - tv1.tv_sec)* 1000000 + (tv2.tv_usec - tv1.tv_usec);

			fps = (num_frames * 1.0) / (delta * 1.0 / 1000000.0);

	//		printf("FPS[camera=%d; frame_id=%d] = %f\n", camera_id, analyzed_info.frame_id, fps);
		}

	}

	PRX("--------------- EXIT %d ----------------- \n", camera_id);

	if (do_visual) {
		delete rgb_portion;
	}
	delete hsv_portion;

	for (i = 0; i < NUM_COLORS_PER_CAMERA; i++) {
		delete thresh_portion[i];
	}

	pthread_mutex_lock(&mutex);
	p_cap->release();
	pthread_mutex_unlock(&mutex);
}




int main(int argc, char **argv) {

	int have_transform = 0;
	int i;
	int have_data;
	int low_h, low_s, low_v;
	int high_h, high_s, high_v;
	int num_frames = 0;
	FILE *cfg_file;
	int rc;


	pthread_t client_thread;
	pthread_t capture_thread[NUM_CAMERAS_PER_HUB];

	if (argc >= 3) {
	} else {
		printf("Usage: ./client <server_ip> <server_port> [-v|-d|-f|-m|-t|-s|--dev=start|--vsize=WxH]\n");
		printf("\t-v : display visuals\n");
		printf("\t-f : print fps\n");
		printf("\t-d : debug info; setting -d sets -v as well\n");
		printf("\t-m : show markers\n");
		printf("\t-t : show threshold images\n");
		printf("\t-s : show spotlight selector\n");
		printf("\t--dev=<number> : first v4l2 device starts at index <number>\n");
		printf("\t--vsize=WxH : specify width/height of the visual output\n");
		
		return -1;
	}

	if (argc >= 4) {
		for (i=3; i< argc; i++) {
			if (strcmp(argv[i], "-d") == 0) {
				do_debug =1;
			} else if (strcmp(argv[i], "-v") == 0) {
				do_visual = 1;
			} else if (strcmp(argv[i], "-f") == 0) {
				do_show_fps = 1;
			} else if (strcmp(argv[i], "-m") == 0) {
				do_show_markers = 1;
			} else if (strcmp(argv[i], "-t") == 0) {
				do_show_thresh = 1;
			} else if (strcmp(argv[i], "-s") == 0) {
				do_show_spot_selector = 1;
			} else if (strncmp(argv[i], "--dev=", 6) == 0) {
				char *p = argv[i];
				p += 6;
				start_dev = atoi(p);
			} else if (strncmp(argv[i], "--vscale=",9) == 0) {
				sscanf(argv[i], "--vscale=%d", &arg_vis_scale);
				printf("Scale = %d\n", arg_vis_scale);
			} else {
				printf("Unknown option %s\n", argv[i]);
				return -1;
			}
		}
	}

	// Set visual for debug as well
	if (do_debug == 1) {
		do_visual = 1;
		do_show_fps = 1;
		do_show_markers = 1;
		do_show_thresh = 1;
	}

	cfg_file = fopen("/etc/camera_client.cfg", "r");
	
	
	if (cfg_file == NULL) {
		perror("Failed to open config file /etc/camera_client.cfg ");
		return -1;
	}
	
	rc = fscanf(cfg_file, "client_id = %d\n", &client_id);

	if (rc < 0) {
		perror("Failed to read from file... ");
		return -1;
	}

	fclose(cfg_file);
	
	if (client_id == -1) {
		PRX("ERROR: failed to retrieve client_id from config file\n");
		return -1;
	}

	PRX("Initializing client with id = %d\n", client_id);
	
	__create_debug_windows();

	if (do_show_spot_selector) {
		__create_debug_spotlight_win(DBG_SPOT_WIN0,0);
		__create_debug_spotlight_win(DBG_SPOT_WIN1,1);
	}

	strcpy(server_ip, argv[1]);
	server_port = atoi(argv[2]);

	PRX("Attempting to connect to %s:%d\n", server_ip, server_port);
	signals_init();

	gettimeofday(&tv1, 0);



	for (i = 0; i < 10; i++) {
		full_thresh[i] = new Mat(CAMERA_HEIGHT*2, CAMERA_WIDTH*2, CV_8UC1);
	}


	for (i = 0; i < NUM_CAMERAS_PER_HUB; i++) {
		capture_info[i].camera_id = i;
		capture_info[i].p_cap = new VideoCapture();

		while (1) {
			capture_info[i].p_cap->open(start_dev + i); 

			if (capture_info[i].p_cap->isOpened()) {
				break;
			}
			usleep(25000);
			PRX("Failed to open camera %d... retrying...\n", i);

			if (do_exit == 1) {
				return -1;
			}
		}

		capture_info[i].track_color = (color_range_t*)malloc(sizeof(init_track_colors));

		memcpy(capture_info[i].track_color, init_track_colors, sizeof(init_track_colors));
	}

	pthread_create(&client_thread, 0, client_read_function, 0);

	for (i = 0 ; i < NUM_CAMERAS_PER_HUB; i++) {
		capture_info[i].frame_id = 0;
		pthread_create(&capture_thread[i], 0, camera_main, (void*)&capture_info[i]);
	}

	while (do_exit == 0) {
		pthread_mutex_lock(&mutex);

		full_image.copyTo(full_image_copy);
		//full_hsv.copyTo(full_hsv_copy);

		pthread_mutex_unlock(&mutex);

		const char *names[] = {
			"red", "green", "blue",
		};

		if (do_visual && do_show_thresh) {
			for (i = 0; i< NUM_COLORS_PER_CAMERA; i++) {
				Mat down_thresh;
				Mat thresh_submat;
				char winn[100];

				down_thresh = Mat::zeros(Size(VISUAL_WIDTH/arg_vis_scale, VISUAL_HEIGHT/arg_vis_scale), CV_8UC1);
				thresh_submat = Mat(*full_thresh[i], Rect(0,0, VISUAL_WIDTH*2, VISUAL_HEIGHT));
				resize(thresh_submat,  down_thresh, Size(VISUAL_WIDTH*2/arg_vis_scale, VISUAL_HEIGHT/arg_vis_scale), 0, 0, INTER_LINEAR);

				sprintf(winn, "thresh%d win (%s)", i, names[i % 3]);
				imshow(winn, down_thresh);

			}
		}



		if (do_visual) {
			Mat downscaled;
			Mat __2cam_submat;

			__2cam_submat = Mat(full_image_copy, Rect(0,0,VISUAL_WIDTH*2, VISUAL_HEIGHT));

			downscaled = Mat::zeros(Size(VISUAL_WIDTH*2/arg_vis_scale, VISUAL_HEIGHT/arg_vis_scale), CV_8UC3);
			resize(__2cam_submat, downscaled, Size(VISUAL_WIDTH*2/arg_vis_scale, VISUAL_HEIGHT/arg_vis_scale), 0,0, INTER_LINEAR);
			imshow("Donwscaled", downscaled);
			//imshow("__2cam", __2cam_submat);
		//	imshow("Full", full_image_copy);
		}

		num_frames++;

		if (num_frames % 100 == 1) {
			unsigned long delta;

			gettimeofday(&tv2, 0);

			delta = (tv2.tv_sec - tv1.tv_sec)* 1000000 + (tv2.tv_usec - tv1.tv_usec);

			PRX("FPS_MAIN = %f\n", (num_frames * 1.0) / (delta * 1.0 / 1000000.0));
		}
	
		waitKey(20);
	}

	PRX("Do exit triggered\n");

	for (i = 0; i < NUM_CAMERAS_PER_HUB; i++) {
		pthread_join(capture_thread[i], NULL);
	}

	PRX("Exiting...\n");
	
	return 0;
}
