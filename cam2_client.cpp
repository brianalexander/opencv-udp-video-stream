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

#define PORT 12346
#define PACK_SIZE 4096
#define LEN_TEST_DATA 30000

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;

    cv::VideoCapture cap0(2);
    cv::Mat frame;
    std::vector<uchar> encoded;

    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(80);

    int numPacks;

    if (!cap0.isOpened())
    {
        exit(EXIT_FAILURE);
    }

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    int n, len;
    printf("%i\n", numPacks);

    while (true)
    {
        cap0 >> frame;
        if (frame.empty())
            continue;

        cv::imencode(".jpg", frame, encoded, compression_params);
        numPacks = ceil(encoded.size() / PACK_SIZE);

        // send initial int that says how many more bytes to read
        sendto(sockfd, &numPacks, sizeof(int), 0, (const struct sockaddr *)&servaddr,
               sizeof(servaddr));
        printf("Sending %i bytes\n", numPacks * PACK_SIZE);

        // send remaining bytes
        for (int i = 0; i < numPacks; i++)
        {
            sendto(sockfd, &encoded[i * PACK_SIZE], PACK_SIZE,
                   0, (const struct sockaddr *)&servaddr,
                   sizeof(servaddr));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
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
//     cap0 >> frame;
//     if (frame.empty())
//         break;
//     cv::imshow("webcam", frame);
//     if (cv::waitKey(33) == 27) // stop capturing by pressing ESC
//         break;
// }
// cap0.release();
// cv::destroyWindow("webcam");