# ---------------------------------------------------------------------------
# Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
# Distributed under the MIT/X11 software license, see the accompanying
# file license.txt or http://www.opensource.org/licenses/mit-license.php.
# ---------------------------------------------------------------------------

import os
import sys
import zmq
import pprint

import logging
logger = logging.getLogger("python_client")
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
formatter = logging.Formatter("%(asctime)s - %(name)s - %(message)s")
ch.setFormatter(formatter)
logger.addHandler(ch)

if __name__ == "__main__":
    logger.info("starting")
    
    PROTOCOL = "tcp"
    HOSTNAME = "127.0.0.1"
    PORTS = [5556, 5557]
    FILTER = ""
    
    connections = ["%s://%s:%s" % (PROTOCOL, HOSTNAME, port) for port in PORTS]
    logger.debug("Collecting updates from: %s" % (pprint.pformat(connections), ))
    
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    map(socket.connect, connections)
    socket.setsockopt(zmq.SUBSCRIBE, FILTER)
    
    while 1:
        incoming = socket.recv()
        logger.debug("Update: '%s'" % (incoming, ))
        