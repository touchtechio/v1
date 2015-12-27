
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
#include <sstream>
#include <thread>
#include <mutex>

#include "parse.h"
#include "common.h"

#define OUTPUT_STREAM_SIZE 10000
#define INBOUND_PACKET_SIZE 100
#define FILLED_SIZE 1304
#define MAX_ZONE_AREA 400


const char* oscTarget;
int oscPort;
int visionPort;
osc::OutboundPacketStream *p;
std::mutex streamMutex;

void process_vision(int listenfd)
{
    UdpTransmitSocket oscSocket( IpEndpointName(oscTarget, oscPort) );

    while (1) {
        visual_packet_t vis_packet;
        int rc = __recvfrom(listenfd, &vis_packet, sizeof(visual_packet_t));

        if (rc != 0) {
            printf("client disconnected...\n");
            exit(1);
        }

        if (vis_packet.data.frame_id == 0 || vis_packet.data.frame_id == 1 || vis_packet.data.frame_id == -1) {
            printf("WARN: frame_id came as %d\n", vis_packet.data.frame_id);
        }

	int zone = vis_packet.zone_id;
        char address[80] = "/camera/zone/";
        std::stringstream stream;
        stream << zone;
        strcat (address, stream.str().c_str());
        analyzed_color_t *color_info = vis_packet.data.color_info;

        streamMutex.lock();
        (*p) << osc::BeginMessage( address );

        for (int i = 0; i < 3; i++) {
        //  (*p) << color_info[i].x << color_info[i].y << color_info[i].area;
          (*p) << ((float)color_info[i].area/(float)MAX_ZONE_AREA));
        }

        (*p) << osc::EndMessage;
        std::cout << p->Size() << std::endl;

        if (p->Size() > FILLED_SIZE) {
            (*p) << osc::EndBundle;
            oscSocket.Send(p->Data(), p->Size());
            p->Clear();
            (*p) << osc::BeginBundleImmediate;
        }
        streamMutex.unlock();

    }
}

int main(int argc, char **argv)
{
    int listenfd, port, rc;
    struct sockaddr_in servaddr;

    if (argc != 4) {
        printf("Error... usage: ./visionp <input port> <output address> <output port>\n");
        return -1;
    }

    port = atoi(argv[1]);
    oscTarget = argv[2];
    oscPort = atoi(argv[3]);
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

    char buffer[OUTPUT_STREAM_SIZE];
    osc::OutboundPacketStream q(buffer, OUTPUT_STREAM_SIZE);
    q << osc::BeginBundleImmediate;
    p = &q;

    std::thread t0(process_vision, listenfd);
    std::thread t1(process_vision, listenfd);
    std::thread t2(process_vision, listenfd);
    std::thread t3(process_vision, listenfd);
    std::thread t4(process_vision, listenfd);
    std::thread t5(process_vision, listenfd);
    std::thread t6(process_vision, listenfd);
    std::thread t7(process_vision, listenfd);

    t0.join();
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    t7.join();
    return 0;
}
