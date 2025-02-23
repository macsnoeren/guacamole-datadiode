#!/usr/bin/env python3

__author__      = 'Radoslaw Matusiak'
__copyright__   = 'Copyright (c) 2016 Radoslaw Matusiak'
__license__     = 'MIT'
__version__     = '0.1'


"""
TCP/UDP proxy.
"""

import argparse
import signal
import logging
import select
import socket


FORMAT = '%(asctime)-15s %(levelname)-10s %(message)s'
logging.basicConfig(format=FORMAT)
LOGGER = logging.getLogger()

LOCAL_DATA_HANDLER = lambda x:x
REMOTE_DATA_HANDLER = lambda x:x

BUFFER_SIZE = 2 ** 10  # 1024. Keep buffer size as power of 2.

# Variables for the protocol processor
# OPCODE,ARG1,ARG2,ARG3,...;
# LENGTH.VALUE
# FSM: 0(LENGTH), 1(DOT), 2(VALUE), 3(END)
fsm_state=0
length_value_string=""
length_value=0
processed_data=""
processing_opcode=True
opcode=""

def udp_proxy(src, dst):
    """Run UDP proxy.
    
    Arguments:
    src -- Source IP address and port string. I.e.: '127.0.0.1:8000'
    dst -- Destination IP address and port. I.e.: '127.0.0.1:8888'
    """
    LOGGER.debug('Starting UDP proxy...')
    LOGGER.debug('Src: {}'.format(src))
    LOGGER.debug('Dst: {}'.format(dst))
    
    proxy_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    proxy_socket.bind(ip_to_tuple(src))
    
    client_address = None
    server_address = ip_to_tuple(dst)
    
    LOGGER.debug('Looping proxy (press Ctrl-Break to stop)...')
    while True:
        data, address = proxy_socket.recvfrom(BUFFER_SIZE)
        
        if client_address == None:
            client_address = address

        if address == client_address:
            data = LOCAL_DATA_HANDLER(data)
            proxy_socket.sendto(data, server_address)
        elif address == server_address:
            data = REMOTE_DATA_HANDLER(data)
            proxy_socket.sendto(data, client_address)
            client_address = None
        else:
            LOGGER.warning('Unknown address: {}'.format(str(address)))
# end-of-function udp_proxy    
    
    
def tcp_proxy(src, dst):
    """Run TCP proxy.
    
    Arguments:
    src -- Source IP address and port string. I.e.: '127.0.0.1:8000'
    dst -- Destination IP address and port. I.e.: '127.0.0.1:8888'
    """
    LOGGER.debug('Starting TCP proxy...')
    LOGGER.debug('Src: {}'.format(src))
    LOGGER.debug('Dst: {}'.format(dst))
    
    sockets = []
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(ip_to_tuple(src))
    s.listen(1)

    s_src, _ = s.accept()

    s_dst = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s_dst.connect(ip_to_tuple(dst)) 
    
    sockets.append(s_src)
    sockets.append(s_dst)
    
    while True:
        s_read, _, _ = select.select(sockets, [], [])
        
        for s in s_read:
            data = s.recv(BUFFER_SIZE)
        
            if s == s_src:
                d = LOCAL_DATA_HANDLER(data)
                if len(str(data)) > 3:
                    #print("SRC: (" + str(len(str(data))) + ") " + str(data))
                    process_data(data.decode("utf-8"))
                s_dst.sendall(d)
            elif s == s_dst:
                d = REMOTE_DATA_HANDLER(data)
                if len(str(data)) > 3:
                    process_data(data.decode("utf-8"))
                    #print("DST: (" + str(len(str(data))) + ") " + str(data))
                s_src.sendall(d)
# end-of-function tcp_proxy    

def process_data(data):
    for b in data:
        process_byte(b)

# Validate protocol
# todo: based on opcode data length could be validated
# todo: the opcodes retrieved can be checked whether it is belonging to the real opcode list of guacamole
def process_byte(b):
    global fsm_state
    global length_value_string
    global length_value
    global processed_data
    global processing_opcode
    global opcode

    processed_data=processed_data+b
    
    if fsm_state == 0: # get the length
        processed_data=""
        if b >= '0' and b <='9':
            length_value_string=b
            fsm_state = 1
        else:
            print("ERROR(0) '"  + processed_data + "'")
            fsm_state = 0

    elif fsm_state == 1: # read the length until dot .
        if b >= '0' and b <='9':
            length_value_string=length_value_string+b
        elif b == '.':
            if length_value_string.isnumeric():
                length_value=int(length_value_string)
                print("LENGTH: '" + length_value_string + "'")
                fsm_state = 2
            else:
                print("ERROR(1.1) '"  + processed_data + "'")
                processed_data=""
                fsm_state = 0
        else:
            print("ERROR(1)")
            fsm_state = 0
        
    elif fsm_state == 2: # read the value until counter is zero
        if length_value == 0:
            if b == ',' or b == ';': # ready and start with length again
                if processing_opcode:
                    print("OPCODE: '" + opcode + "'")
                processing_opcode = b == ';'
                opcode=""
                #print("CORRECT!")
                fsm_state = 0
            else:
                print("ERROR(2.1) '"  + processed_data + "'")
                processed_data=""
                fsm_state = 0
        else:
            #if b != ',' and b != '.' and b != ';':
            length_value=length_value-1
            if processing_opcode:
                opcode=opcode+b
            #else:
            #    print("ERROR(2) '"  + processed_data + "'")
            #    processed_data=""
            #    fsm_state = 0
    else:
        print("ERROR UNKNOWN STATE")
        fsm_state=0
        
def ip_to_tuple(ip):
    """Parse IP string and return (ip, port) tuple.
    
    Arguments:
    ip -- IP address:port string. I.e.: '127.0.0.1:8000'.
    """
    ip, port = ip.split(':')
    return (ip, int(port))
# end-of-function ip_to_tuple


def main():
    """Main method."""
    parser = argparse.ArgumentParser(description='TCP/UPD proxy.')
    
    # TCP UPD groups
    proto_group = parser.add_mutually_exclusive_group(required=True)
    proto_group.add_argument('--tcp', action='store_true', help='TCP proxy')
    proto_group.add_argument('--udp', action='store_true', help='UDP proxy')
    
    parser.add_argument('-s', '--src', required=True, help='Source IP and port, i.e.: 127.0.0.1:8000')
    parser.add_argument('-d', '--dst', required=True, help='Destination IP and port, i.e.: 127.0.0.1:8888')
    
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument('-q', '--quiet', action='store_true', help='Be quiet')
    output_group.add_argument('-v', '--verbose', action='store_true', help='Be loud')
    
    args = parser.parse_args()
    
    if args.quiet:
        LOGGER.setLevel(logging.CRITICAL)
    if args.verbose:
        LOGGER.setLevel(logging.NOTSET)
    
    if args.udp:
        udp_proxy(args.src, args.dst)
    elif args.tcp:
        tcp_proxy(args.src, args.dst)
# end-of-function main    


if __name__ == '__main__':
    main()
