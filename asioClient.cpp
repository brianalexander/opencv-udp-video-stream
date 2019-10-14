/**
 * g++ asioClient.cpp -o client -lpthread `pkg-config --cflags --libs opencv`
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <asio.hpp>

#include "opencv2/opencv.hpp"
#include "packetdefinitions.hpp"

// function declarations
void configurationListener();
void videoStreamWriter();

// global variables
ConfigurationPacket currentConfig;
unsigned short configurationPort;

// vars for thread synchronization
std::mutex m;
std::condition_variable condVar;
std::string data;
bool haveNewConfig = false;

// Create an ip::udp::socket object to receive requests on a port assigned by the computer.
using asio::ip::udp;
asio::io_context io_context;
asio::ip::udp::socket udpSocket(io_context, udp::v4());

int main(int argc, char *argv[])
{
    // start configuration listener
    std::thread configListenerThread(configurationListener);

    // start video stream in paused state
    std::thread vidStreamThread(videoStreamWriter);
    asio::socket_base::send_buffer_size option;
    udpSocket.get_option(option);
    std::cout << option.value() << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ConnectionPacket connPack = {"camera1"};

    // Create endpoint
    udp::endpoint remote_endpoint = udp::endpoint(
        asio::ip::address::from_string("127.0.0.1"),
        12345);

    // Send message
    udpSocket.send_to(asio::buffer(&connPack, sizeof(connPack)), remote_endpoint);

    // wait for threads to finish before quitting
    configListenerThread.join();
    vidStreamThread.join();
}

void configurationListener()
{
    unsigned short recv_bytes;
    unsigned char buf[100];

    for (;;) // infinite loop
    {
        asio::ip::udp::endpoint remote_endpoint;
        asio::error_code error;

        // get the incoming configuration data
        // structure: quality[1], fps[1], resolutionX[2], resolutionY[2]
        do
        {
            recv_bytes = udpSocket.receive_from(asio::buffer(buf, 100), remote_endpoint);
        } while (recv_bytes > 2);

        // get the number of packets to be expected.
        unsigned short numPacks = ((unsigned short *)buf)[0];

        // get the configuration data
        recv_bytes = udpSocket.receive_from(asio::buffer(buf, 100), remote_endpoint);

        // verify we got all the packets
        if (recv_bytes == numPacks)
        {
            // make sure we are in the correct state to accept a new configuration
            if (haveNewConfig == false)
            {
                currentConfig = ConfigurationPacket::deserialize(buf);
                printf("new configuration: device: %s FPS:%hhu QUAL:%hhu X:%hu Y:%hu\n",
                       currentConfig.device.c_str(),
                       currentConfig.fps,
                       currentConfig.quality,
                       currentConfig.resolutionX,
                       currentConfig.resolutionY);

                // after receiving configuration change ready to true
                {
                    std::lock_guard<std::mutex> lk(m);
                    haveNewConfig = true;
                }
                condVar.notify_one();
            }
        }
    }
}

void videoStreamWriter()
{
    cv::VideoCapture vidCap;
    cv::Mat frame;
    std::vector<int> compression_params;
    std::vector<uchar> encoded;

    unsigned int numPacks;
    unsigned int numBytes;
    int bytesPerFrame;

    // Wait until we receive a configuration from the server
    std::unique_lock<std::mutex> lk(m);
    condVar.wait(lk, [] { return haveNewConfig; });

    // release lock on mutex
    lk.release();

    std::cout << "begin streaming" << std::endl;

    while (true)
    {
        std::cout << "mainloop" << std::endl;
        udp::endpoint remote_endpoint = udp::endpoint(
            asio::ip::address::from_string("127.0.0.1"),
            currentConfig.targetPort);

        udpSocket.connect(remote_endpoint);

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
        while (true)
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
            udpSocket.send(asio::buffer(&numBytes, sizeof(numBytes)));

            for (int i = 0; i < numPacks; i++)
            {
                bytesPerFrame += udpSocket.send(asio::buffer(&encoded[i * PACK_SIZE], PACK_SIZE));
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
        }
    }
}

// unsigned char *encodedArr = new unsigned char[encoded.size()];

// std::copy(encoded.begin(), encoded.end(), encodedArr);
// int extraBytes = numBytes - (numPacks * PACK_SIZE);
// std::cout << numBytes << "  " << numPacks << std::endl;

//display bytes recieved and reset count to 0
// printf("bytes recieved : %i\n", n);

// printf("JPG size bytes %i\n", numBytes);
// printf("Sending %i bytes\n", numPacks * PACK_SIZE);

// unsigned char *localBuffer = new unsigned char[numBytes];

// n += udpSocket.send(asio::buffer(&encoded[i * PACK_SIZE], extraBytes));

// memcpy(&localBuffer[i * PACK_SIZE], &encodedArr[i * PACK_SIZE], PACK_SIZE);

// memcpy(&localBuffer[i * PACK_SIZE], &encodedArr[i * PACK_SIZE], extraBytes);

// cv::Mat rawData = cv::Mat(1, numBytes, CV_8UC1, buffer);
// std::vector<unsigned char> rawData(localBuffer, localBuffer + numBytes);
// cv::Mat newFrame = cv::imdecode(rawData, cv::IMREAD_COLOR);
// if (newFrame.size().width == 0)
// {
//     std::cerr << "decode failure" << std::endl;
//     continue;
// }

// release memory back to the heap
// delete buffer;

// Draws the frame on screen. Will be replaced with UI code
// cv::imshow("recv", newFrame);
// cv::waitKey(1);