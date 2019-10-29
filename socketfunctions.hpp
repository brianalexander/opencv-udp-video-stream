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

#define BACKLOG 128 // how many pending connections queue will hold
#define PACK_SIZE 4096

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int bindUdpSocketFd(char const *port = nullptr)
{
    int udpSocketFd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((udpSocketFd = socket(p->ai_family, p->ai_socktype,
                                  p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (setsockopt(udpSocketFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(udpSocketFd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(udpSocketFd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return -1;
    }

    return udpSocketFd;
}

int connectUdpSocketFd(char const *host, char const *port)
{
    int udpSocketFd;
    struct addrinfo hints, *servinfo, *p;
    int yes = 1;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((udpSocketFd = socket(p->ai_family, p->ai_socktype,
                                  p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (setsockopt(udpSocketFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (connect(udpSocketFd, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(udpSocketFd);
            perror("listener: failed connect");
            return -1;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return -1;
    }

    return udpSocketFd;
}

int bindTcpSocketFd(char const *port = nullptr)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int tcpSocketFd;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    hints.ai_protocol = IPPROTO_TCP;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((tcpSocketFd = socket(p->ai_family, p->ai_socktype,
                                  p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(tcpSocketFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(tcpSocketFd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(tcpSocketFd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    if (listen(tcpSocketFd, BACKLOG) == -1)
    {
        perror("listen");
        return -1;
    }

    return tcpSocketFd;
}

int connectTcpSocketFd(char const *host = nullptr, char const *port = nullptr)
{
    struct addrinfo hints, *server_list, *server_candidate;
    int rv;
    int tcpSocketFd;

    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if ((rv = getaddrinfo(host, port, &hints, &server_list)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (server_candidate = server_list; server_candidate != NULL; server_candidate = server_candidate->ai_next)
    {
        if ((tcpSocketFd = socket(server_candidate->ai_family, server_candidate->ai_socktype,
                                  server_candidate->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(tcpSocketFd, server_candidate->ai_addr, server_candidate->ai_addrlen) == -1)
        {
            close(tcpSocketFd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (server_candidate == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(server_candidate->ai_family, get_in_addr((struct sockaddr *)server_candidate->ai_addr),
              s, sizeof s);
    printf("tcp client: connecting to %s\n", s);

    freeaddrinfo(server_list); // all done with this structure

    return tcpSocketFd;
}