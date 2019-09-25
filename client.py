# socket_echo_client_dgram.py
import socket
import sys
import math

pack_size = 4096

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = ('localhost', 10000)
message = b'a'*100000000
num_packs = math.ceil(len(message) / pack_size)
print(num_packs)

try:

    # Send data
    for i in range(num_packs):
        # print('sending {!r}'.format(message))
        sent = sock.sendto(
            message[i*pack_size: (i+1) * pack_size], server_address)

    # # Receive response
    # print('waiting to receive')
    # data, server = sock.recvfrom(4096)
    # print('received {!r}'.format(data))

finally:
    print('sent')
    #     print('closing socket')
    sock.close()
