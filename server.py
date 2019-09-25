# socket_echo_server_dgram.py

import socket
import sys
from multiprocessing import Queue
from threading import Thread
import time


# q = Queue()

# sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# # Bind the socket to the port
# server_address = ('localhost', 10000)
# print('starting up on {} port {}'.format(*server_address))
# sock.bind(server_address)


# def socketQueue():
#     while True:
#         data, address = sock.recvfrom(4096)
#         q.put_nowait(data)


# def printQueueSize():
#     while(True):
#         print(q.qsize())
#         # print(q)
#         time.sleep(1)


# def makeStr():
#     packets = []
#     numChunks = 24415
#     while(True):
#         if(q.qsize() >= numChunks):
#             for i in range(numChunks):
#                 print(i)

#                 chunk = q.get()
#                 packets.append(chunk)
#             print(b''.join(packets))


# t1 = Thread(target=socketQueue)
# t1.start()

# # t2 = Thread(target=makeStr)
# # t2.start()

# t3 = Thread(target=printQueueSize)
# t3.start()

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 3000000)

# Bind the socket to the port
server_address = ('localhost', 10000)
print('starting up on {} port {}'.format(*server_address))
sock.bind(server_address)
i = 0
packets = []
while True:
    # print('\nwaiting to receive message')
    data, address = sock.recvfrom(4096)
    i = i + 1
    packets.append(data)
    # print(len(packets))
    if(i == 24415):
        print(len(packets))
        packets.clear()
        i = 0

    # print('received {} bytes from {}'.format(
    #     len(data), address))
    # print(data)

    # if data:
    #     sent = sock.sendto(data, address)
    #     print('sent {} bytes back to {}'.format(
    #         sent, address))
