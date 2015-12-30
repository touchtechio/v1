.PHONY: all client server vis_server fake map api_app options_server calibrator

all: map fake server vis_server client api_app options_server calibrator

client: 
	g++ client.cpp -o client -O3 -I/usr/include/opencv -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videostab -lrt -lpthread -lm -ldl

server:
	g++ server.cpp parse.cpp -o server -O3  -lrt -lpthread -lm 

vis_server:
	g++ vis_server.cpp -o vis_server -O3 -I/usr/include/opencv -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videostab -lrt -lpthread -lm -ldl

fake:
	g++ fake_client.cpp -o fake_client -O3 -lpthread -lm
	g++ fake_visual_sender.cpp -o fake_visual_sender -O3 -lpthread 

map:
	g++ map_server.cpp -o map_server -O3 -I/usr/include/opencv -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videostab -lrt -lpthread -lm -ldl

api_app:
	g++ api_app.cpp -o api_app -O3 -lrt -lpthread -lm -ldl

options_server:
	g++ options_server.cpp parse.cpp -o options_server -g -lpthread

calibrator:
	g++ calibrator.cpp -o calibrator -O3 -I/usr/include/opencv -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_objdetect -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_video -lopencv_videostab -lrt -lpthread -lm -ldl

clean:
	rm -f client server vis_server fake_client api_app fake_visual_sender map_server options_server calibrator
