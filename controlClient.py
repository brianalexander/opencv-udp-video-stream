import socket
import struct

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the port
server_address = ('localhost', 12346)

# fps quality resolution-x resolution-y
fps = 30
quality = 100
resolution_x = 320
resolution_y = 240

message = struct.pack("! B B H H", fps, quality, resolution_x, resolution_y)

sock.sendto(message, server_address)
