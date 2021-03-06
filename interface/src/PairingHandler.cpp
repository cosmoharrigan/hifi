//
//  PairingHandler.cpp
//  hifi
//
//  Created by Stephen Birarda on 5/13/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

#include <NodeList.h>

#include "PairingHandler.h"

const char PAIRING_SERVER_HOSTNAME[] = "pairing.highfidelity.io";
const int PAIRING_SERVER_PORT = 7247;

PairingHandler* PairingHandler::getInstance() {
    static PairingHandler* instance = NULL;
    
    if (!instance) {
        instance = new PairingHandler();
    }
    
    return instance;
}

void PairingHandler::sendPairRequest() {    
    // grab the node socket from the NodeList singleton
    UDPSocket *nodeSocket = NodeList::getInstance()->getNodeSocket();
    
    // prepare the pairing request packet
    
    // use the getLocalAddress helper to get this client's listening address
    int localAddress = getLocalAddress();
    
    char pairPacket[24] = {};
    sprintf(pairPacket, "Find %d.%d.%d.%d:%hu",
            localAddress & 0xFF,
            (localAddress >> 8) & 0xFF,
            (localAddress >> 16) & 0xFF,
            (localAddress >> 24) & 0xFF,
            NodeList::getInstance()->getSocketListenPort());
    
    qDebug("Sending pair packet: %s\n", pairPacket);
    
    sockaddr_in pairingServerSocket;
    
    pairingServerSocket.sin_family = AF_INET;
    
    // lookup the pairing server IP by the hostname
    struct hostent* hostInfo = gethostbyname(PAIRING_SERVER_HOSTNAME);
    memcpy(&pairingServerSocket.sin_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
    pairingServerSocket.sin_port = htons(PAIRING_SERVER_PORT);
    
    // send the pair request to the pairing server
    nodeSocket->send((sockaddr*) &pairingServerSocket, pairPacket, strlen(pairPacket));
}
