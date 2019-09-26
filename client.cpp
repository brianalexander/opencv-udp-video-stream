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
#define WIDTH
#define HEIGHT

// how to compile: g++ -ggdb client.cpp -o client `pkg-config --cflags --libs opencv`
// Ex usage: [filename] input-device server-ip port frames(0-30) jpg-quality(0-100) || 5 arguments
// Ex usage: ./client 0 10.67.111.236 12345 30 90
int main(int argc, char *argv[])
{
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
    unsigned char FPS;
    sscanf(argv[4], "%hhu", &FPS);

    // get fifth argument - JPG QUALITY
    unsigned char JPG_QUALITY;
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
    vidCap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    vidCap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    // std::cout << vidCap.get(cv::CAP_PROP_FRAME_WIDTH) << std::endl;

    std::vector<uchar> encoded;
    cv::Mat frame;

    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(JPG_QUALITY);

    int numPacks;

    if (!vidCap.isOpened())
    {
        exit(EXIT_FAILURE);
    }

    // Creating socket file descriptor
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(serverIP);
    socklen_t servAddrLen = sizeof(servaddr);
    // servaddr.sin_addr.s_addr = INADDR_ANY;

    // main loop for broadcasting frames
    while (true)
    {
        // get a video frame from the camera
        vidCap >> frame;

        // if it's empty go back to the start of the loop
        if (frame.empty())
            continue;

        // use opencv to encode and compress the frame as a jpg
        cv::imencode(".jpg", frame, encoded, compression_params);

        // get the number of packets that need to be sent of the line
        numPacks = ceil(encoded.size() / PACK_SIZE);

        // send initial int that says how many more packets need to be read
        sendto(sockfd, &numPacks, sizeof(int), 0, (const struct sockaddr *)&servaddr,
               sizeof(servaddr));
        printf("Sending %i bytes\n", numPacks * PACK_SIZE);

        // send remaining packets
        for (int i = 0; i < numPacks; i++)
        {
            sendto(sockfd, &encoded[i * PACK_SIZE], PACK_SIZE,
                   0, (const struct sockaddr *)&servaddr,
                   sizeof(servaddr));
        }

        // sleep for an appropriate amount of time to send out the desired FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
    }

    close(sockfd);
    return 0;
}

// char testData[LEN_TEST_DATA];
// memset(testData, 'B', sizeof(testData) - 1);
// testData[LEN_TEST_DATA - 1] = '\0';

// int numPacks = ceil(sizeof(testData) / (PACK_SIZE * 1.0));

// // send remaining bytes
// for (int i = 0; i < numPacks; i++)
// {
//     sendto(sockfd, &testData[i * PACK_SIZE], PACK_SIZE,
//            0, (const struct sockaddr *)&servaddr,
//            sizeof(servaddr));
// }
// std::this_thread::sleep_for(std::chrono::milliseconds(33));

// while (true)
// {
//     vidCap >> frame;
//     if (frame.empty())
//         break;
//     cv::imshow("webcam", frame);
//     if (cv::waitKey(33) == 27) // stop capturing by pressing ESC
//         break;
// }
// vidCap.release();
// cv::destroyWindow("webcam");