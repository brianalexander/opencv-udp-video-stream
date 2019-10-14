// typedef unsigned char byte;
#include <vector>
#include <string>
#include <iostream>

#define PACK_SIZE 4096

class ConfigurationPacket
{
public:
    std::string device;
    unsigned char fps;
    unsigned char quality;
    unsigned short resolutionX;
    unsigned short resolutionY;
    unsigned short targetPort;

    static unsigned char *serialize(ConfigurationPacket &packet, unsigned short &numPacks);
    static ConfigurationPacket deserialize(unsigned char *buffer);
};

ConfigurationPacket ConfigurationPacket::deserialize(unsigned char *buffer)
{
    ConfigurationPacket newPacket;
    for (; *buffer != '\0'; ++buffer)
    {
        newPacket.device.push_back(*buffer);
    }

    ++buffer;
    newPacket.fps = *buffer;

    ++buffer;
    newPacket.quality = *buffer;

    ++buffer;
    newPacket.resolutionX = (buffer[0] << 8 | buffer[1]);

    buffer += 2;
    newPacket.resolutionY = (buffer[0] << 8 | buffer[1]);

    buffer += 2;
    newPacket.targetPort = (buffer[0] << 8 | buffer[1]);

    return newPacket;
};

unsigned char *ConfigurationPacket::serialize(ConfigurationPacket &packet, unsigned short &numPacks)
{
    std::vector<unsigned char> byteBuffer;

    // add device string to buffer
    byteBuffer.insert(byteBuffer.begin(), packet.device.begin(), packet.device.end());
    byteBuffer.push_back('\0');

    byteBuffer.push_back(packet.fps);
    byteBuffer.push_back(packet.quality);

    unsigned char *byteResX = static_cast<unsigned char *>(static_cast<void *>(&packet.resolutionX));
    byteBuffer.push_back(byteResX[1]);
    byteBuffer.push_back(byteResX[0]);

    unsigned char *byteResY = static_cast<unsigned char *>(static_cast<void *>(&packet.resolutionY));
    byteBuffer.push_back(byteResY[1]);
    byteBuffer.push_back(byteResY[0]);

    unsigned char *byteTargPort = static_cast<unsigned char *>(static_cast<void *>(&packet.targetPort));
    byteBuffer.push_back(byteTargPort[1]);
    byteBuffer.push_back(byteTargPort[0]);

    numPacks = byteBuffer.size();

    unsigned char *serializedData = (unsigned char *)malloc(sizeof(unsigned char) * byteBuffer.size());
    std::copy(byteBuffer.begin(), byteBuffer.end(), serializedData);

    return serializedData;
};

struct ConnectionPacket
{
    char cameraId[26];
};