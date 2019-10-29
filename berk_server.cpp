/*
 * g++ berk_server.cpp -o berkserv -lpthread `pkg-config --cflags --libs opencv`
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h> // for memset
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
// #include <signal.h>

// Data structures
#include <vector>
#include <set>
#include <map>

// Thread management
#include <thread>
#include <chrono>

// OpenCV
#include "opencv2/opencv.hpp"

// JSON
#include "json.hpp"

// Networking
#include "packetdefinitions.hpp"
#include "socketfunctions.hpp"

std::set<int> socketFds;
std::map<int, std::thread> socketToThreadMap;

nlohmann::json configJSON;

void tcpConnectionListener(char const *port);
void videoStreamListener(char const *port);

void sendConfiguration(int socketFd, uint8_t *configurationBuffer, uint16_t numPacks);
void sendHeartbeat(int socketFd, uint8_t *configurationBuffer, uint16_t numPacks);

int main(void)
{
    // Get handle for config file
    std::ifstream i("config.json");
    // Read data into json object
    i >> configJSON;
    // close file stream
    i.close();

    std::thread tcpConnectionListenerThread(tcpConnectionListener,
                                            configJSON["connPort"].get<std::string>().c_str());
    tcpConnectionListenerThread.join();

    return 0;
}

void sendConfiguration(int socketFd, uint8_t *configurationBuffer, uint16_t numPacks)
{
    int result;
    std::cout << "sending configuration..." << std::endl;
    std::cout << sizeof(numPacks) << std::endl;
    result = send(socketFd, &numPacks, sizeof(numPacks), MSG_NOSIGNAL);
    if (result == -1)
    {
        perror("send");
    }

    std::cout << numPacks << std::endl;
    result = send(socketFd, configurationBuffer, numPacks, MSG_NOSIGNAL);
    if (result == -1)
    {
        perror("send");
    }
}

void videoStreamListener(char const *port)
{
    int udpSock;
    int duration = 0;
    unsigned int bytesPerSecond = 0;
    int recv_bytes = 0;
    unsigned int numPacks;
    unsigned int numBytes;
    unsigned int frameBytes;
    const int buflen = 200000;
    unsigned char *buffer = new unsigned char[buflen];

    udpSock = bindUdpSocketFd(port);

    while (true)
    {
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        frameBytes = 0;
        // the do-while below is in charge of finding a packet that is a single int
        // We then get that int, which presents the number of packets needed to get
        // the complete image.
        // If it gets data or misses data, it will continue to throw away messages
        // until it finds another int, which is the start of a new image.

        do
        {
            recv_bytes = recvfrom(udpSock, buffer, buflen, 0, NULL, NULL);
            bytesPerSecond += recv_bytes;

        } while (recv_bytes > sizeof(int));

        // treat tempBuf as an int array and get the first element
        numBytes = ((int *)buffer)[0];
        numPacks = (numBytes / PACK_SIZE) + 1;

        for (int i = 0; i < numPacks + 1; i++)
        {
            recv_bytes += recvfrom(udpSock, &buffer[i * PACK_SIZE], PACK_SIZE, MSG_WAITALL, NULL, NULL);
            frameBytes += recv_bytes;
            bytesPerSecond += recv_bytes;
        }

        // display bytes recieved and reset count to 0
        // printf("bytes recieved : %i\n", frameBytes);

        std::vector<unsigned char> rawData(buffer, buffer + numBytes);
        cv::Mat frame = cv::imdecode(rawData, cv::IMREAD_COLOR);
        if (frame.size().width == 0)
        {
            std::cerr << "decode failure" << std::endl;
            continue;
        }

        // Draws the frame on screen. Will be replaced with UI code
        cv::imshow("recv", frame);
        cv::waitKey(1);

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        duration += std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        if (duration >= 1000)
        {
            std::cout << "bytes per second: " << (double)bytesPerSecond / 1000.0 / 1000.0 << "mB/s" << std::endl;
            duration = 0;
            bytesPerSecond = 0;
        }
    }

    delete buffer;
}

void tcpConnectionListener(char const *port)
{
    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    int recv_bytes;
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;

    ConnectionPacket connPack;

    sockfd = bindTcpSocketFd(port);
    printf("server: waiting for connections...\n");

    for (;;)
    { // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        // Verify their information in the config file
        recv_bytes = recv(new_fd, &connPack, CONN_PACK_SIZE, 0);

        if (recv_bytes == CONN_PACK_SIZE)
        {
            std::cout << connPack.cameraId << " has connected." << std::endl;
            ConfigurationPacket defaultConfigPacket = {
                configJSON["devices"][connPack.cameraId]["device"].get<std::string>(),
                configJSON["devices"][connPack.cameraId]["targetPort"].get<std::string>(),
                configJSON["devices"][connPack.cameraId]["fps"].get<uint8_t>(),
                configJSON["devices"][connPack.cameraId]["quality"].get<uint8_t>(),
                configJSON["devices"][connPack.cameraId]["resolutionX"].get<uint16_t>(),
                configJSON["devices"][connPack.cameraId]["resolutionY"].get<uint16_t>(),
            };

            socketFds.insert(new_fd);
            printf("starting udp listener on %s\n", defaultConfigPacket.targetPort.c_str());
            socketToThreadMap.insert({new_fd, std::thread(videoStreamListener, defaultConfigPacket.targetPort.c_str())});

            uint16_t numPacks;
            uint8_t *serializedConfigPack = ConfigurationPacket::serialize(defaultConfigPacket, numPacks);

            sendConfiguration(new_fd, serializedConfigPack, numPacks);

            delete serializedConfigPack;
        }
    }
}

void sendHeartbeat(int socketFd, uint8_t *configurationBuffer, uint16_t numPacks)
{
    for (;;)
    {
        if (!socketFds.empty())
        {
            for (auto it = std::begin(socketFds); it != std::end(socketFds); ++it)
            {
                printf("Sending messages to %d.\n", *it);
                int result = send(*it, "Hello, world!", 13, MSG_NOSIGNAL); //MSG_NOSIGNAL
                if (result == -1)
                {
                    perror("send");
                    socketFds.erase(socketFds.find(*it));
                }
            }
        }

        for (auto it = std::begin(socketFds); it != std::end(socketFds); ++it)
        {
            printf("%d ", *it);
            printf("\n");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}