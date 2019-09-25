#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "opencv2/opencv.hpp"

// #include "opencv2/opencv.hpp"

#define PORT 12345
#define PACK_SIZE 4096
#define BUF_LEN 65540

int main(int argc, char *argv[])
{
    int sockfd;
    char *tempBuf = new char[PACK_SIZE];

    struct sockaddr_in servaddr,
        cliaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // fill servaddr & cliaddr with zeros
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    memset(tempBuf, 'C', sizeof(tempBuf));

    servaddr.sin_family = AF_INET; // ipv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // bind the socket with the server address
    int result;
    result = bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (result < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int len;
    int n = 0;
    int numPacks = 8;
    int recvMsgSize;

    // char tempBuf[PACK_SIZE];

    while (true)
    {
        do
        {
            recvMsgSize = recvfrom(sockfd, tempBuf, BUF_LEN, 0, NULL, NULL);
        } while (recvMsgSize > sizeof(int));

        // treat tempBuf as an int array and get the first element
        numPacks = ((int *)tempBuf)[0];

        char *buffer = new char[numPacks * PACK_SIZE];

        for (int i = 0; i < numPacks; i++)
        {
            n += recvfrom(sockfd, tempBuf, PACK_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, (socklen_t *)&len);

            memcpy(&(buffer[i * PACK_SIZE]), tempBuf, PACK_SIZE);
        }
        printf("bytes recieved : %i\n", n);

        cv::Mat rawData = cv::Mat(1, PACK_SIZE * numPacks, CV_8UC1, buffer);
        cv::Mat frame = cv::imdecode(rawData, cv::IMREAD_COLOR);
        if (frame.size().width == 0)
        {
            std::cerr << "decode failure" << std::endl;
            continue;
        }
        cv::imshow("recv", frame);
        free(buffer);
        cv::waitKey(1);

        // printf("strlen(buffer) : %i\n", strlen(buffer));
        // printf("String: %s\n", buffer);
        n = 0;
    }
}

// while (true)
// {
//     for (int i = 0; i < numPacks; i++)
//     {
//         n += recvfrom(sockfd, tempBuf, PACK_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, (socklen_t *)&len);

//         memcpy(&(buffer[i * PACK_SIZE]), tempBuf, PACK_SIZE);
//     }
//     printf("bytes recieved : %i\n", n);
//     buffer[n] = '\0';

//     printf("strlen(buffer) : %i\n", strlen(buffer));
//     // printf("String: %s\n", buffer);
//     n = 0;
// }