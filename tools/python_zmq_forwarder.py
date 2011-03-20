# ---------------------------------------------------------------------------
# Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
# Distributed under the MIT/X11 software license, see the accompanying
# file license.txt or http://www.opensource.org/licenses/mit-license.php.
# ---------------------------------------------------------------------------

import os
import sys
import zmq
import zmq.devices

import logging
logger = logging.getLogger("python_zmq_forwarder")
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
formatter = logging.Formatter("%(asctime)s - %(name)s - %(message)s")
ch.setFormatter(formatter)
logger.addHandler(ch)

if __name__ == "__main__":
    logger.info("starting")
    
    PROTOCOL = "tcp"
    HOSTNAME = "10.0.0.10"
    IN_PORT = 5556
    OUT_PORT = 5557
    
    in_connection = "%s://%s:%s" % (PROTOCOL, HOSTNAME, IN_PORT)
    out_connection = "%s://%s:%s" % (PROTOCOL, HOSTNAME, OUT_PORT)
    logger.info("Forwarding updates from '%s' to '%s'" % (in_connection, out_connection))
    
    context = zmq.Context()
    in_socket = context.socket(zmq.SUB)
    in_socket.setsockopt(zmq.SUBSCRIBE, "")
    in_socket.bind(in_connection)
    in_socket.connect(in_connection)
    
    out_socket = context.socket(zmq.PUB)
    out_socket.bind(out_connection)
    out_socket.connect(out_connection)        
    
    try:
        logger.debug("Starting forwarder...")        
        device = zmq.device(zmq.FORWARDER, in_socket, out_socket)
    except:
        logger.exception("Forwarder is stopping.")
        