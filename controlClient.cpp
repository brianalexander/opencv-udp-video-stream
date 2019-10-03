//C++ Implementation of Socket Sending for the Titan Rover

//import header files
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {

	//to compile this program run: g++ -ggdb -w controlClient.cpp -o controlClient
	//usage: [filename] server-ip port FPS  JPG-quality resolution_x resolution_y
	//Example usage: ./controlClient 10.67.111.236 8080 30 20 450 450

	//get arguments from command line user
	std::string serverIP;
	std::string port;
	std::string FPS;
	std::string JPG_QUALITY;
	std::string resolution_x;
	std::string resolution_y;

	serverIP = argv[1];
	port = argv[2];
	FPS = argv[3];
	JPG_QUALITY = argv[4];
	resolution_x = argv[5];
	resolution_y = argv[6];

	//combine into one string
	std::string messag = FPS + "," + JPG_QUALITY + "," + resolution_x + "," + resolution_y;
	
	//convert the messag into a const char pointer because that is what the socket sending wants it to be
	const char *message = new char[messag.size() + 1];
	message = messag.c_str();

	//set up socket 
	int sockfd;
	struct sockaddr_in servaddr;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("creating socket failed");
		exit(EXIT_FAILURE);
	}

	//convert the string serverIp to a const char
	const char *charServerIP = new char[16];
	charServerIP = serverIP.c_str();

	//server info
	memset(&servaddr, 0, sizeof(servaddr));		
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(12346);
	servaddr.sin_addr.s_addr = inet_addr(charServerIP);
	
	//send the socket
	sendto(sockfd, (const char *)message, strlen(message), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	printf("Sent message\n");
	return 0;
}


