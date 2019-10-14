/**
 * g++ server.cpp -o server -lpthread `pkg-config --cflags --libs opencv`
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <asio.hpp>
#include <vector>
#include <thread>
#include "json.hpp"

#include "opencv2/opencv.hpp"
#include "packetdefinitions.hpp"

struct Camera
{
    std::string cameraId;
    asio::ip::udp::endpoint endpoint;
};

//
// function declarations
//
void sendConfiguration(Camera &camera, unsigned char *configurationBuffer, unsigned short numPacks);
void connectionListener();
void videoStreamListener(unsigned short port);

//
// Globals
//
std::map<std::string, Camera> cameras;
std::map<std::string, std::thread> videoStreamListeners;

nlohmann::json configJSON;

// how to compile: g++ -ggdb server.cpp -o server `pkg-config --cflags --libs opencv`
int main(int argc, char *argv[])
{
    {
        std::ifstream i("config.json");
        i >> configJSON;
        i.close();
    }

    std::thread connListenerThread(connectionListener);
    connListenerThread.join();
}

/**
 * Listen for new cameras
 * 
 * */
void connectionListener()
{
    using asio::ip::udp;

    asio::io_context io_context;

    ConnectionPacket connPack;

    // Create an ip::udp::socket object to receive requests on UDP port 12345.
    udp::socket socket(io_context, udp::endpoint(udp::v4(), configJSON["connPort"]));

    for (;;)
    {
        udp::endpoint remote_endpoint;
        asio::error_code error;

        // get the incoming camera data
        // structure: (char[25])
        socket.receive_from(asio::mutable_buffer(&connPack, sizeof(connPack)), remote_endpoint);
        std::cout << "message received" << std::endl
                  << std::flush;

        // get CameraId
        std::string cameraId(connPack.cameraId);

        // add camera to list of connected cameras
        cameras[cameraId] = {cameraId, remote_endpoint};

        // create Camera Configuration Packet
        ConfigurationPacket defaultConfigPacket = {
            configJSON["devices"][cameraId]["device"].get<std::string>(),
            configJSON["devices"][cameraId]["fps"].get<unsigned char>(),
            configJSON["devices"][cameraId]["quality"].get<unsigned char>(),
            configJSON["devices"][cameraId]["resolutionX"].get<unsigned short>(),
            configJSON["devices"][cameraId]["resolutionY"].get<unsigned short>(),
            configJSON["devices"][cameraId]["targetPort"].get<unsigned short>()};

        // start a thread to listen for the new client
        // save the thread in a map indexed by the cameraId for later access
        videoStreamListeners[cameraId] = std::thread(videoStreamListener, defaultConfigPacket.targetPort);

        // serialize Configuration Packet
        unsigned short numPacks;
        unsigned char *seralizedConfPack = ConfigurationPacket::serialize(defaultConfigPacket, numPacks);

        // respond to the camera with it's starting configuration
        sendConfiguration(cameras[cameraId], seralizedConfPack, numPacks);

        // clean up seralized data
        delete seralizedConfPack;
    }
}

/**
 * Send configurations to known cameras
 * NOT A THREAD, SINGLE FUNCTION THAT SENDS A SINGLE PACKET
 * */
void sendConfiguration(Camera &camera, unsigned char *configurationBuffer, unsigned short numPacks)
{
    using asio::ip::udp;

    asio::io_context io_context;

    udp::endpoint remote_endpoint = camera.endpoint;

    // Create and open socket
    udp::socket socket(io_context);
    socket.open(udp::v4());

    socket.send_to(asio::buffer(&numPacks, sizeof(numPacks)), remote_endpoint);
    socket.send_to(asio::buffer(configurationBuffer, numPacks), remote_endpoint);

    socket.close();
}

/**
 * ONE COPY FOR EACH VIDEO STREAM
 * */
void videoStreamListener(unsigned short port)
{
    int duration = 0;
    unsigned int bytesPerSecond = 0;
    int recv_bytes = 0;
    unsigned int numPacks;
    unsigned int numBytes;
    unsigned int frameBytes;
    unsigned char *buffer = new unsigned char[200000];

    // Create an ip::udp::socket object to receive requests on the target port.
    using asio::ip::udp;
    asio::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), port));

    asio::socket_base::receive_buffer_size option;
    socket.get_option(option);
    std::cout << option.value() << std::endl;

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
            recv_bytes = socket.receive(asio::buffer(buffer, PACK_SIZE));
            bytesPerSecond += recv_bytes;

        } while (recv_bytes > sizeof(int));

        // treat tempBuf as an int array and get the first element
        numBytes = ((int *)buffer)[0];
        numPacks = (numBytes / PACK_SIZE) + 1;

        for (int i = 0; i < numPacks + 1; i++)
        {
            recv_bytes += socket.receive(asio::buffer(&buffer[i * PACK_SIZE], numPacks * PACK_SIZE));
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
// cv::Mat rawData = cv::Mat(1, numBytes, CV_8UC1, buffer);
// char *tempBuf = new char[PACK_SIZE];
// printf("received %d bytes\n", recv_bytes);
// int extraBytes = numBytes - (numPacks * PACK_SIZE);
// printf("Frame size %i bytes\n", numBytes);
// create a byte array that is the size of the incomming image

// recv_bytes = socket.receive(asio::buffer(&buffer[i * PACK_SIZE], extraBytes));
// n += recv_bytes;
// memset(buffer, 0, 1000 * PACK_SIZE);
// delete buffer;
