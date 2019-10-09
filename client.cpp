#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <iostream>

#include <chrono>
#include <thread>

#include "opencv2/opencv.hpp"

#define PACK_SIZE 4096
#define LISTENER_PORT 12346

// Global configurations shared between threads
unsigned char FPS;
unsigned char JPG_QUALITY;
unsigned short resolutionX;
unsigned short resolutionY;
unsigned char updateStreamConfigurationFlag = 0;

void videoStream(cv::VideoCapture &vidCap, int sockfd, sockaddr_in &servaddr, socklen_t servAddrLen);

// how to compile: g++ -ggdb client.cpp -o client `pkg-config --cflags --libs opencv` -pthread
// Ex usage: [filename] input-device server-ip port frames(0-30) jpg-quality(0-100) || 5 arguments
// Ex usage: ./client 0 10.67.111.236 12345 30 90
int main(int argc, char *argv[])
{
    //
    // BEGIN SETTING UP CONFIGURATION LISTENER
    //
    int configListenerSockFD;
    int result;

    struct sockaddr_in configListenerAddr;
    socklen_t configListenerAddrLen = sizeof(configListenerAddr);
    configListenerSockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (configListenerSockFD < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&configListenerAddr, 0, configListenerAddrLen);

    configListenerAddr.sin_family = AF_INET;
    configListenerAddr.sin_addr.s_addr = INADDR_ANY;
    configListenerAddr.sin_port = htons(LISTENER_PORT);

    result = bind(configListenerSockFD, (const struct sockaddr *)&configListenerAddr, configListenerAddrLen);
    if (result < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //
    // BEGIN SETTING UP VIDEO STREAM THREAD
    //
    resolutionX = 640;
    resolutionY = 480;

    // get first argument - INPUT DEVICE
    unsigned char cameraCode;
    std::string cameraIP;
    unsigned char useCameraCode = 0;
    if (strlen(argv[1]) == 1)
    {
        sscanf(argv[1], "%hhu", &cameraCode);
        useCameraCode = 1;
    }
    else
    {
        cameraIP = argv[1];
    }

    // get second argument - SERVER IP
    char serverIP[16];
    sscanf(argv[2], "%s", serverIP);

    // get third argument - SERVER PORT
    int port;
    sscanf(argv[3], "%d", &port);

    // get fourth argument - FRAMES PER SECOND
    sscanf(argv[4], "%hhu", &FPS);

    // get fifth argument - JPG QUALITY
    sscanf(argv[5], "%hhu", &JPG_QUALITY);

    cv::VideoCapture vidCap;

    if (useCameraCode)
    {
        vidCap.open(cameraCode);
    }
    else
    {
        vidCap.open(cameraIP);
    }

    if (!vidCap.isOpened())
    {
        exit(EXIT_FAILURE);
    }

    // Creating socket file descriptor
    int videoStreamSockFD;
    struct sockaddr_in videoStreamAddr;
    socklen_t videoStreamAddrLen = sizeof(videoStreamAddr);

    videoStreamSockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (videoStreamSockFD < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&videoStreamAddr, 0, videoStreamAddrLen);

    // Filling server information
    videoStreamAddr.sin_family = AF_INET;
    videoStreamAddr.sin_port = htons(port);
    videoStreamAddr.sin_addr.s_addr = inet_addr(serverIP);

    // servaddr.sin_addr.s_addr = INADDR_ANY;

    //
    // START VIDEO STREAM THREAD
    //
    std::thread streamingThread(videoStream, std::ref(vidCap), videoStreamSockFD, std::ref(videoStreamAddr), videoStreamAddrLen);

    //
    // START LISTENING FOR RECONFIGURATION MESSAGES
    //
    // 6 bytes of incomming data
    // change this to TCP
    int recvMsgSize;
    unsigned char bufSize = 6;
    char *buffer = new char[bufSize];

    while (true)
    {
        recvMsgSize = recvfrom(configListenerSockFD, buffer, bufSize, MSG_WAITALL, NULL, NULL);
        if (recvMsgSize == 6 && updateStreamConfigurationFlag == 0)
        {
            FPS = buffer[0];
            JPG_QUALITY = buffer[1];
            resolutionX = (buffer[2] << 8 | buffer[3]);
            resolutionY = (buffer[4] << 8 | buffer[5]);
            printf("new configuration: FPS:%hhu QUAL:%hhu X:%hu Y:%hu\n", FPS, JPG_QUALITY, resolutionX, resolutionY);
            updateStreamConfigurationFlag = 1;
        }
    }

    close(videoStreamSockFD);
    close(configListenerSockFD);
    return 0;
}

void videoStream(cv::VideoCapture &vidCap, int sockfd, sockaddr_in &servaddr, socklen_t servAddrLen)
{
    std::vector<uchar> encoded;
    cv::Mat frame;
    std::vector<int> compression_params;
    int numPacks;

    while (true)
    {
        vidCap.set(cv::CAP_PROP_FRAME_WIDTH, resolutionX);
        vidCap.set(cv::CAP_PROP_FRAME_HEIGHT, resolutionY);

        compression_params.clear();
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(JPG_QUALITY);
        updateStreamConfigurationFlag = 0;
        // main loop for broadcasting frames
        while (true)
        {
            // get a video frame from the camera
            vidCap >> frame;

            // if it's empty go back to the start of the loop
            if (frame.empty())
                continue;

            cv::cvtColor(frame, frame, CV_BGR2RGB);
            // use opencv to encode and compress the frame as a jpg
            cv::imencode(".jpg", frame, encoded, compression_params);

            // get the number of packets that need to be sent of the line
            numPacks = ceil(encoded.size() / PACK_SIZE);

            // send initial int that says how many more packets need to be read
            sendto(sockfd, &numPacks, sizeof(int), 0, (const struct sockaddr *)&servaddr,
                   servAddrLen);
            printf("Sending %i bytes\n", numPacks * PACK_SIZE);

            // send remaining packets
            for (int i = 0; i < numPacks; i++)
            {
                sendto(sockfd, &encoded[i * PACK_SIZE], PACK_SIZE,
                       0, (const struct sockaddr *)&servaddr,
                       servAddrLen);
            }
            // at the end...
            if (updateStreamConfigurationFlag)
                break;

            // sleep for an appropriate amount of time to send out the desired FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
        }
    }
}