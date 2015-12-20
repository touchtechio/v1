
/* (c) Intel Corporation 2015, All Rights Reserved */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "lib/osc/OscOutboundPacketStream.h"
#include "lib/ip/UdpSocket.h"

#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "parse.h"
#include "common.h"

#define OUTPUT_STREAM_SIZE 10000
#define INBOUND_PACKET_SIZE 100
#define FILLED_SIZE 1304

const char* oscTarget;
int oscPort;
int visionPort;

int main(int argc, char **argv) 
{
	int listenfd, connfd, rc, port;
	struct sockaddr_in servaddr;

	if (argc != 2) {
		printf("Error... usage: ./visionp <port>\n");
		return -1;
	}

	port = atoi(argv[1]);
	printf("Initializing visual parser... on port %d\n",port);

	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	int option = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&option, sizeof(int));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	rc = bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

	if (rc == -1) {
		perror("bind error: ");
		return -1;
	}
	
	printf("Accepting connections on visualization...\n");
	visual_packet_t vis_packet;
	connfd = listenfd;

    char buffer[OUTPUT_STREAM_SIZE];
    osc::OutboundPacketStream p(buffer, OUTPUT_STREAM_SIZE);
    q << osc::BeginBundleImmediate;

    UdpTransmitSocket oscSocket( IpEndpointName(oscTarget, oscPort) );

	while (1) {
		rc = __recvfrom(connfd, &vis_packet, sizeof(visual_packet_t));

		if (rc != 0) {
			printf("client disconnectd...\n");
			exit(1);
		}

		if (vis_packet.data.frame_id == 0 || vis_packet.data.frame_id == 1 || vis_packet.data.frame_id == -1) {
			printf("WARN: frame_id came as %d\n", vis_packet.data.frame_id);
		}

        char address[80] = "/camera/zone/";
        stream << vis_packet.zone_id;
        strcat (address, stream.str().c_str());
        analyzed_color_t *color_info = vis_packet.data.color_info;
        p << osc::BeginMessage( address );
        
        for (int i = 0; i < 7; i++) {
        	p << color_info[i].x << color_info[i].y << color_info[i].area;
        }

        p << osc::EndMessage;
        std::cout << p.Size() << std::endl;
		
		if (p.Size > FILLED_SIZE) {
			p << osc::EndBundle;
            int size = p->Size();
            oscSocket.Send(p->Data(), p->Size());
            p->Clear();
            p << osc::BeginBundleImmediate;
		}
	}

	return 0;
}
