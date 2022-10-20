/*******************************************************************************
 Copyright (C) 2010  Bryan Godbolt godbolt ( a t ) ualberta.ca
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 ****************************************************************************/
/*
 This program sends some data to qgroundcontrol using the mavlink protocol.  The sent packets
 cause qgroundcontrol to respond with heartbeats.  Any settings or custom commands sent from
 qgroundcontrol are printed by this program along with the heartbeats.
 
 
 I compiled this program successfully on Ubuntu 10.04 with the following command
 
 gcc -I ../../pixhawk/mavlink/include -o udp-server udp-server-test.c
 
 the rt library is needed for the clock_gettime on linux
 */
/* These headers are for QNX, but should all be standard on unix/linux */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#if (defined __QNX__) | (defined __QNXNTO__)
/* QNX specific headers */
#include <unix.h>
#else
/* Linux / MacOS POSIX timer headers */
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdbool.h> /* required for the definition of bool in C99 */
#endif

/* This assumes you have the mavlink headers on your include path
 or in the same folder as this source file */
#include <mavlink.h>

#define BUFFER_LENGTH 2041 // minimum buffer size that can be used with qnx (I don't know why)

//////////////////////////////////// Variables  //////////////////////////////
//	main함수에 있던 변수들을 모두 밖으로 뺌. 다른 함수에서 그냥 사용할 수 있도록 하기 위해.

float position[6] = {10,20,30,100,150,10};//x,y,z,vx,vy,vz
const uint16_t voltages[]={10,11,12,13,14,15,16,17,18,19};// arccoma2022.10.04
int battery_remaining = 100;

mavlink_message_t msg;
mavlink_status_t status;
uint16_t len;
uint8_t buf[BUFFER_LENGTH];
int bytes_sent;

int sock;
struct sockaddr_in gcAddr; 
struct sockaddr_in locAddr;
//struct sockaddr_in fromAddr;
ssize_t recsize;
socklen_t fromlen = sizeof(gcAddr);

int i = 0;
//int success = 0;
unsigned int temp = 0;

//////////////////////////////////// Functions  //////////////////////////////
uint64_t microsSinceEpoch();
void send_mavlink_data_to_qgc(int);
void recv_mavlink_data_from_qgc(int);

int main(int argc, char* argv[])
{
	printf("%s", "communicate with QGC by mavlink protocol.\n");
	char help[] = "--help";
	char target_ip[100];

	// Check if --help flag was used
	if ((argc == 2) && (strcmp(argv[1], help) == 0))
    {
		printf("\n");
		printf("\tUsage:\n\n");
		printf("\t");
		printf("%s", argv[0]);
		printf(" <ip address of QGroundControl>\n");
		printf("\tDefault for localhost: udp-server 127.0.0.1\n\n");
		exit(EXIT_FAILURE);
    }
	
	// Change the target ip if parameter was given
	strcpy(target_ip, "127.0.0.1");
	if (argc == 2) {
		strcpy(target_ip, argv[1]);
    }
	
	memset(&locAddr, 0, sizeof(locAddr));
	locAddr.sin_family = AF_INET;
	locAddr.sin_addr.s_addr = INADDR_ANY;
	locAddr.sin_port = htons(14551);
	
	sock =  socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	/* Bind the socket to port 14551 - necessary to receive packets from qgroundcontrol */ 
	if (-1 == bind(sock,(struct sockaddr *)&locAddr, sizeof(struct sockaddr)))
    {
		perror("error bind failed");
		close(sock);
		exit(EXIT_FAILURE);
    } 
	
	/* Attempt to make it non blocking */
#if (defined __QNX__) | (defined __QNXNTO__)
	if (fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0)
#else
	if (fcntl(sock, F_SETFL, O_NONBLOCK | O_ASYNC) < 0)
#endif

    {
		fprintf(stderr, "error setting nonblocking: %s\n", strerror(errno));
		close(sock);
		exit(EXIT_FAILURE);
    }
	
	memset(&gcAddr, 0, sizeof(gcAddr));
	gcAddr.sin_family = AF_INET;
	gcAddr.sin_addr.s_addr = inet_addr(target_ip);
	gcAddr.sin_port = htons(14550);

	//msg.msgid = MAVLINK_MSG_ID_BATTERY_STATUS;
	memset(buf, 0, BUFFER_LENGTH);
	for (;;) {
		send_mavlink_data_to_qgc(sock); // only send hearbeat package
		memset(buf, 0, BUFFER_LENGTH);
		recv_mavlink_data_from_qgc(sock);
		memset(buf, 0, BUFFER_LENGTH);
		sleep(1); // Sleep one second
    }
}//main

void recv_mavlink_data_from_qgc(int sock){
	//msg.msgid = 147;//MAVLINK_MSG_ID_BATTERY_STATUS;
	
	recsize = recvfrom(sock, (void *)buf, BUFFER_LENGTH, 0, (struct sockaddr *)&gcAddr, &fromlen);
	if (recsize > 0){
		// Something received - print out all bytes and parse packet
			
		printf("Bytes Received: %d\nDatagram: ", (int)recsize);
		for (i = 0; i < recsize; ++i)
		{
			temp = buf[i];
			printf("%02x ", (unsigned char)temp);
			if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status))
			{
				// Packet received
				
				printf("\nReceived packet: SYS: %d, COMP: %d, LEN: %d, MSG ID: %d\n", msg.sysid, msg.compid, msg.len, msg.msgid);
			}
		}
		printf("\n");
	}		
}

void send_mavlink_data_to_qgc(int sock){

	//Send Heartbeat : I am a Helicopter. My heart is beating.
	mavlink_msg_heartbeat_pack(1, 200, &msg, MAV_TYPE_HELICOPTER, MAV_AUTOPILOT_GENERIC, MAV_MODE_GUIDED_ARMED, 0, MAV_STATE_ACTIVE);
	len = mavlink_msg_to_send_buffer(buf, &msg);
	bytes_sent = sendto(sock, buf, len, 0, (struct sockaddr*)&gcAddr, sizeof(struct sockaddr_in));
	printf("send heartbeat to QGC\n");
	
/*	send another data..
	
	//베터리 상태 전송 // arccoma2022.10.04 
	
	if(0 == battery_remaining){
		battery_remaining=100;	// 
	}
	else if( battery_remaining ){
		battery_remaining-=5;	// full charging
	}
	mavlink_msg_battery_status_pack(1, 200, &msg,0,1,1,77,voltages,0,0,-1,battery_remaining,0,1,0,0,0);
	len = mavlink_msg_to_send_buffer(buf, &msg);
	bytes_sent = sendto(sock, buf, len, 0, (struct sockaddr*)&gcAddr, sizeof(struct sockaddr_in));
	printf("send battery_status to QGC\n");


	// Send Status 
	mavlink_msg_sys_status_pack(1, 200, &msg, 0, 0, 0, 500, 11000, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	len = mavlink_msg_to_send_buffer(buf, &msg);
	bytes_sent = sendto(sock, buf, len, 0, (struct sockaddr*)&gcAddr, sizeof (struct sockaddr_in));
	printf("send system_status to QGC\n");
		
	// Send Local Position 
	mavlink_msg_local_position_ned_pack(1, 200, &msg, microsSinceEpoch(), 
										position[0], position[1], position[2],
										position[3], position[4], position[5]);
	len = mavlink_msg_to_send_buffer(buf, &msg);
	bytes_sent = sendto(sock, buf, len, 0, (struct sockaddr*)&gcAddr, sizeof(struct sockaddr_in));
	printf("send local_position to QGC\n");
		
	// Send attitude 
	mavlink_msg_attitude_pack(1, 200, &msg, microsSinceEpoch(), 1.2, 1.7, 3.14, 0.01, 0.02, 0.03);
	len = mavlink_msg_to_send_buffer(buf, &msg);
	bytes_sent = sendto(sock, buf, len, 0, (struct sockaddr*)&gcAddr, sizeof(struct sockaddr_in));
	printf("send attitude to QGC\n");
*/
}

/* QNX timer version */
#if (defined __QNX__) | (defined __QNXNTO__)
uint64_t microsSinceEpoch()
{
	
	struct timespec time;
	
	uint64_t micros = 0;
	
	clock_gettime(CLOCK_REALTIME, &time);  
	micros = (uint64_t)time.tv_sec * 1000000 + time.tv_nsec/1000;
	
	return micros;
}
#else
uint64_t microsSinceEpoch()
{
	
	struct timeval tv;
	
	uint64_t micros = 0;
	
	gettimeofday(&tv, NULL);  
	micros =  ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
	
	return micros;
}
#endif
