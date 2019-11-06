/*
 * client.c -- a stream socket client demo
 */

#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <random>
#include <cmath>

#include "opencv2/opencv.hpp"
#include "packetdefinitions.hpp"
#include "socketfunctions.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAXIMUM_BACKOFF 32000

// function declarations
void tcpListener(int tcpSockFd);
void videoStreamWriter(int udpSockFd);
bool heartBeatReciever(int tcpSockFd);
// global variables
ConfigurationPacket currentConfig;

// vars for thread synchronization
std::mutex m;
std::condition_variable condVar;
bool haveNewConfig = false;

void init(int argc, char *argv[]) {
    ConnectionPacket connPack;
    memset(connPack.cameraId, '\0', sizeof(connPack.cameraId));
    strcpy(connPack.cameraId, argv[1]);

    for (;;)
    {
        if (-1 == (tcpSockFd = connectTcpSocketFd(argv[2], argv[3])))
        {
            if (waitTime > MAXIMUM_BACKOFF)
            {
                waitTime = MAXIMUM_BACKOFF + dist(rl24);
            }
            else
            {
                waitTime = static_cast<int>(std::pow(2, connectionAttemptCount)) * 1000 + dist(rl24);
            }

            // waitTime = (waitTime < MAXIMUM_BACKOFF ? waitTime : MAXIMUM_BACKOFF) + dist(rl24);
            std::cout << waitTime << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
            ++connectionAttemptCount;
        }
        else
        {
            // start configuration listener
            std::thread(tcpListener, tcpSockFd).detach();

            std::this_thread::sleep_for(std::chrono::milliseconds(3000));

            // Send user ID
            send(tcpSockFd, &connPack, sizeof(connPack), 0);

            // Block until we receive configuration
            {
                // Wait until we lose our connection
                std::unique_lock<std::mutex> lk(m);
                condVar.wait(lk, [] { return haveNewConfig; });

                // release lock on mutex
                lk.release();
            }

            std::cout << "target port: " << currentConfig.targetPort << std::endl;

            // start thread after configuration received
            int udpSockFd = connectUdpSocketFd(argv[2], currentConfig.targetPort.c_str());
            std::cout << "udpSockFd: " << udpSockFd << std::endl;
            std::thread(videoStreamWriter, udpSockFd, argc, *argv[]).detach();
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc != 4)
    {
        fprintf(stderr, "usage: ./client cameraId hostname port\n");
        exit(1);
    }

    //setup random number generator for exponential backoff
    std::random_device rd;
    std::ranlux24 rl24(rd());
    std::uniform_int_distribution<int> dist(1, 1000);
    int tcpSockFd;
    int connectionAttemptCount = 0;
    int waitTime = 0;

    init(argc, *argv[]);

    // std::this_thread::sleep_for(std::chrono::seconds(1));

    
}

void tcpListener(int tcpSockFd)
{

    int recv_bytes;
    const int buflen = 2000000;
    uint8_t buf[buflen];

    for (;;) // infinite loop
    {
        std::cout << "waiting for new configuration" << std::endl;
        // get the incoming configuration data
        do
        {
            recv_bytes = recv(tcpSockFd, buf, PACK_SIZE, 0);
            std::cout << "received " << recv_bytes << " bytes" << std::endl;
        } while (recv_bytes != 2);

        // get the number of packets to be expected.
        unsigned short numPacks = ((unsigned short *)buf)[0];

        // get the configuration data
        std::cout << "waiting for config bytes" << std::endl;
        recv_bytes = recv(tcpSockFd, buf, PACK_SIZE, MSG_NOSIGNAL);

        // verify we got all the packets
        if (recv_bytes == numPacks)
        {
            // make sure we are in the correct state to accept a new configuration
            if (haveNewConfig == false)
            {
                currentConfig = ConfigurationPacket::deserialize(buf);
                std::cout << "deserialize " << currentConfig.targetPort << std::endl;
                printf("new configuration: device: %s port: %s FPS:%hhu QUAL:%hhu X:%hu Y:%hu\n",
                       currentConfig.device.c_str(),
                       currentConfig.targetPort.c_str(),
                       currentConfig.fps,
                       currentConfig.quality,
                       currentConfig.resolutionX,
                       currentConfig.resolutionY);

                // after losing our connection, change ready to true
                {
                    std::lock_guard<std::mutex> lk(m);
                    haveNewConfig = true;
                }
                condVar.notify_one();
            }
        }
    }
    std::cout << "exiting" << std::endl;
}

bool heartBeatReciever(int tcpSockFd)
{
    int recv_bytes;
    const int buflen = 2000000;
    uint8_t buf[buflen];

    while (true) // infinite loop
    {
        // get the incoming configuration data
        do
        {
            recv_bytes = recv(tcpSockFd, buf, PACK_SIZE, 0);
            std::cout << "received " << recv_bytes << " bytes" << std::endl;
        } while (recv_bytes != 2);

        // get the number of packets to be expected.
        unsigned short numPacks = ((unsigned short *)buf)[0];

        // verify we got all the packets
        if (recv_bytes == numPacks)
        {
            return true; // just continue
        } 
    } 
    return false; // error handling here as server maybe crashed
}

void videoStreamWriter(int udpSockFd, int argc, char *argv[])
{
    cv::VideoCapture vidCap;
    cv::Mat frame;
    std::vector<int> compression_params;
    std::vector<uchar> encoded;

    unsigned int numPacks;
    unsigned int numBytes;
    int bytesPerFrame;

    std::cout << "begin streaming" << std::endl;

    while (true)
    {

        std::cout << "mainloop" << std::endl;

        if (std::isdigit(currentConfig.device[0]))
        {
            vidCap.open(currentConfig.device[0] - '0');
        }
        else
        {
            vidCap.open(currentConfig.device);
        }

        if (!vidCap.isOpened())
        {
            std::cout << "failed to open device" << std::endl;
            exit(EXIT_FAILURE);
        }

        vidCap.set(cv::CAP_PROP_FRAME_WIDTH, currentConfig.resolutionX);
        vidCap.set(cv::CAP_PROP_FRAME_HEIGHT, currentConfig.resolutionY);

        compression_params.clear();
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(currentConfig.quality);

        // reset haveNewConfig to false
        haveNewConfig = false;

        // main loop for broadcasting frames
        while (heartBeatReciever()) //add tcpSockfd as input for heartbeatReceiver()
        {
            bytesPerFrame = 0;
            // std::cout << "streamingloop" << std::endl;
            // get a video frame from the camera
            vidCap >> frame;

            // if it's empty go back to the start of the loop
            if (frame.empty())
                continue;

            // change formatting from BRG to RGB when opening images outside of openCV
            cv::cvtColor(frame, frame, CV_BGR2RGB);

            // use opencv to encode and compress the frame as a jpg
            cv::imencode(".jpg", frame, encoded, compression_params);

            // get the number of packets that need to be sent of the line
            numBytes = encoded.size();
            numPacks = (numBytes / PACK_SIZE) + 1;

            // send initial int that says how many more packets need to be read
            send(udpSockFd, &numBytes, sizeof(numBytes), MSG_NOSIGNAL);
            // udpSocket.send_to(asio::buffer(&numBytes, sizeof(numBytes)), remote_endpoint);

            for (int i = 0; i < numPacks; i++)
            {
                // bytesPerFrame += udpSocket.send_to(asio::buffer(&encoded[i * PACK_SIZE], PACK_SIZE), remote_endpoint);
                bytesPerFrame = send(udpSockFd, &encoded[i * PACK_SIZE], PACK_SIZE, MSG_NOSIGNAL);
            }

            // display bytes recieved and reset count to 0
            printf("bytes sent : %i\n", bytesPerFrame);

            // at the end...
            if (haveNewConfig)
            {
                std::cout << "new config available" << std::endl;
                break;
            }

            // sleep for an appropriate amount of time to send out the desired FPS
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / currentConfig.fps));
            heartBeatReciever();
        }
        std::terminate() //kills threads (may shutdown entire program???)
        init(argc, *argv[]); //restarts main
    }
}

// if (!haveNewConfig)
// {
// }
// else // configuration received. start stream & block until we need to try and reconnect
// {
//     // reset connection attemps
//     connectionAttemptCount = 0;

//     {
//         // Wait until we lose our connection
//         std::unique_lock<std::mutex> lk(m);
//         condVar.wait(lk, [] { return lostConnection; });

//         // release lock on mutex
//         lk.release();
//     }
// }

// int main(int argc, char *argv[])
// {
//     int sockfd, numbytes;
//     char buf[MAXDATASIZE];
//     struct addrinfo hints, *servinfo, *p;
//     int rv;
//     char s[INET6_ADDRSTRLEN];

//     if (argc != 2)
//     {
//         fprintf(stderr, "usage: client hostname\n");
//         exit(1);
//     }

//     memset(&hints, 0, sizeof hints);
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;

//     if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
//     {
//         fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
//         return 1;
//     }

//     // loop through all the results and connect to the first we can
//     for (p = servinfo; p != NULL; p = p->ai_next)
//     {
//         if ((sockfd = socket(p->ai_family, p->ai_socktype,
//                              p->ai_protocol)) == -1)
//         {
//             perror("client: socket");
//             continue;
//         }

//         if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
//         {
//             close(sockfd);
//             perror("client: connect");
//             continue;
//         }

//         break;
//     }

//     if (p == NULL)
//     {
//         fprintf(stderr, "client: failed to connect\n");
//         return 2;
//     }

//     inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
//               s, sizeof s);
//     printf("client: connecting to %s\n", s);

//     freeaddrinfo(servinfo); // all done with this structure
//     for (;;)
//     {
//         if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
//         {
//             perror("recv");
//             exit(1);
//         }

//         buf[numbytes] = '\0';

//         printf("client: received '%s'\n", buf);
//     }

//     close(sockfd);

//     return 0;
// }