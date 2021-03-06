//
//  UDPSocket.cpp
//  interface
//
//  Created by Stephen Birarda on 1/28/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#include <arpa/inet.h>
#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#ifdef _WIN32
#include "Syssocket.h"
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif

#include <QtCore/QDebug>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QHostAddress>

#include "Logging.h"
#include "UDPSocket.h"

sockaddr_in destSockaddr, senderAddress;

bool socketMatch(const sockaddr* first, const sockaddr* second) {
    if (first != NULL && second != NULL) {
        // utility function that indicates if two sockets are equivalent
        
        // currently only compares two IPv4 addresses
        // expandable to IPv6 by adding else if for AF_INET6
        
        if (first->sa_family != second->sa_family) {
            // not the same family, can't be equal
            return false;
        } else if (first->sa_family == AF_INET) {
            const sockaddr_in *firstIn = (const sockaddr_in *) first;
            const sockaddr_in *secondIn = (const sockaddr_in *) second;
            
            return firstIn->sin_addr.s_addr == secondIn->sin_addr.s_addr
                && firstIn->sin_port == secondIn->sin_port;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

int packSocket(unsigned char* packStore, in_addr_t inAddress, in_port_t networkOrderPort) {
    packStore[0] = inAddress >> 24;
    packStore[1] = inAddress >> 16;
    packStore[2] = inAddress >> 8;
    packStore[3] = inAddress;
    
    packStore[4] = networkOrderPort >> 8;
    packStore[5] = networkOrderPort;
    
    return 6; // could be dynamically more if we need IPv6
}

int packSocket(unsigned char* packStore, sockaddr* socketToPack) {
    return packSocket(packStore, ((sockaddr_in*) socketToPack)->sin_addr.s_addr, ((sockaddr_in*) socketToPack)->sin_port);
}

int unpackSocket(const unsigned char* packedData, sockaddr* unpackDestSocket) {
    sockaddr_in* destinationSocket = (sockaddr_in*) unpackDestSocket;
    destinationSocket->sin_family = AF_INET;
    destinationSocket->sin_addr.s_addr = (packedData[0] << 24) + (packedData[1] << 16) + (packedData[2] << 8) + packedData[3];
    destinationSocket->sin_port = (packedData[4] << 8) + packedData[5];
    return 6; // this could be more if we ever need IPv6
}

void copySocketToEmptySocketPointer(sockaddr** destination, const sockaddr* source) {
    // create a new sockaddr or sockaddr_in depending on what type of address this is
    if (source->sa_family == AF_INET) {
        *destination = (sockaddr*) new sockaddr_in;
        memcpy(*destination, source, sizeof(sockaddr_in));
    } else {
        *destination = (sockaddr*) new sockaddr_in6;
        memcpy(*destination, source, sizeof(sockaddr_in6));
    }
}

int getLocalAddress() {
    
    static int localAddress = 0;
    
    if (localAddress == 0) {
        foreach(const QNetworkInterface &interface, QNetworkInterface::allInterfaces()) {
            if (interface.flags() & QNetworkInterface::IsUp
                && interface.flags() & QNetworkInterface::IsRunning
                && interface.flags() & ~QNetworkInterface::IsLoopBack) {
                // we've decided that this is the active NIC
                // enumerate it's addresses to grab the IPv4 address
                foreach(const QNetworkAddressEntry &entry, interface.addressEntries()) {
                    // make sure it's an IPv4 address that isn't the loopback
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback()) {
                        qDebug("Node's local address is %s\n", entry.ip().toString().toLocal8Bit().constData());
                        
                        // set our localAddress and break out
                        localAddress = htonl(entry.ip().toIPv4Address());
                        break;
                    }
                }
            }
            
            if (localAddress != 0) {
                break;
            }
        }
    }
    
    // return the looked up local address
    return localAddress;
}

unsigned short loadBufferWithSocketInfo(char* addressBuffer, sockaddr* socket) {
    if (socket != NULL) {
        char* copyBuffer = inet_ntoa(((sockaddr_in*) socket)->sin_addr);
        memcpy(addressBuffer, copyBuffer, strlen(copyBuffer));
        return htons(((sockaddr_in*) socket)->sin_port);
    } else {
        const char* unknownAddress = "Unknown";
        memcpy(addressBuffer, unknownAddress, strlen(unknownAddress));
        return 0;
    }
}

sockaddr_in socketForHostnameAndHostOrderPort(const char* hostname, unsigned short port) {
    struct hostent* pHostInfo;
    sockaddr_in newSocket = {};

    if ((pHostInfo = gethostbyname(hostname))) {
        memcpy(&newSocket.sin_addr, pHostInfo->h_addr_list[0], pHostInfo->h_length);
    }
    
    if (port != 0) {
        newSocket.sin_port = htons(port);
    }
    
    return newSocket;
}

UDPSocket::UDPSocket(unsigned short int listeningPort) :
    _listeningPort(listeningPort),
    blocking(true)
{
    init();
    // create the socket
    handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if (handle <= 0) {
        qDebug("Failed to create socket.\n");
        return;
    }
    
    destSockaddr.sin_family = AF_INET;
    
    // bind the socket to the passed listeningPort
    sockaddr_in bind_address;
    bind_address.sin_family = AF_INET;
    bind_address.sin_addr.s_addr = INADDR_ANY;
    bind_address.sin_port = htons((uint16_t) _listeningPort);
    
    if (bind(handle, (const sockaddr*) &bind_address, sizeof(sockaddr_in)) < 0) {
        qDebug("Failed to bind socket to port %hu.\n", _listeningPort);
        return;
    }
    
    // if we requested an ephemeral port, get the actual port
    if (listeningPort == 0) {
        socklen_t addressLength = sizeof(sockaddr_in);
        getsockname(handle, (sockaddr*) &bind_address, &addressLength);
        _listeningPort = ntohs(bind_address.sin_port);
    }
    
    const int DEFAULT_BLOCKING_SOCKET_TIMEOUT_USECS = 0.5 * 1000000;
    setBlockingReceiveTimeoutInUsecs(DEFAULT_BLOCKING_SOCKET_TIMEOUT_USECS);
    
    qDebug("Created UDP Socket listening on %hd\n", _listeningPort);
}

UDPSocket::~UDPSocket() {
#ifdef _WIN32
    closesocket(handle);
#else
    close(handle);
#endif
}

bool UDPSocket::init() { 
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD( 2, 2 );
 
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        return false;
    }
 
    /* Confirm that the WinSock DLL supports 2.2.*/
    /* Note that if the DLL supports versions later    */
    /* than 2.2 in addition to 2.2, it will still return */
    /* 2.2 in wVersion since that is the version we      */
    /* requested.                                        */
 
    if ( LOBYTE( wsaData.wVersion ) != 2 ||
            HIBYTE( wsaData.wVersion ) != 2 ) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        WSACleanup();
        return false; 
    }
#endif
    return true;
}

void UDPSocket::setBlocking(bool blocking) {
    this->blocking = blocking;

#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(handle, FIONBIO, &mode);
#else
    int flags = fcntl(handle, F_GETFL, 0);
    fcntl(handle, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

void UDPSocket::setBlockingReceiveTimeoutInUsecs(int timeoutUsecs) {
    struct timeval tv = {timeoutUsecs / 1000000, timeoutUsecs % 1000000};
    setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
}

//  Receive data on this socket with retrieving address of sender
bool UDPSocket::receive(void* receivedData, ssize_t* receivedBytes) const {
    return receive((sockaddr*) &senderAddress, receivedData, receivedBytes);
}

//  Receive data on this socket with the address of the sender 
bool UDPSocket::receive(sockaddr* recvAddress, void* receivedData, ssize_t* receivedBytes) const {
    
#ifdef _WIN32
    int addressSize = sizeof(*recvAddress);
#else
    socklen_t addressSize = sizeof(*recvAddress);
#endif    
    *receivedBytes = recvfrom(handle, static_cast<char*>(receivedData), MAX_BUFFER_LENGTH_BYTES,
                              0, recvAddress, &addressSize);
    
    return (*receivedBytes > 0);
}

int UDPSocket::send(sockaddr* destAddress, const void* data, size_t byteLength) const {
    if (destAddress) {
        // send data via UDP
        int sent_bytes = sendto(handle, (const char*)data, byteLength,
                                0, (sockaddr *) destAddress, sizeof(sockaddr_in));
        
        if (sent_bytes != byteLength) {
            qDebug("Failed to send packet: %s\n", strerror(errno));
            return false;
        }
        
        return sent_bytes;
    } else {
        qDebug("UDPSocket send called with NULL destination address - Likely a node with no active socket.\n");
        return 0;
    }
    
}

int UDPSocket::send(const char* destAddress, int destPort, const void* data, size_t byteLength) const {
    
    // change address and port on reusable global to passed variables
    destSockaddr.sin_addr.s_addr = inet_addr(destAddress);
    destSockaddr.sin_port = htons((uint16_t)destPort);
    
    return send((sockaddr *)&destSockaddr, data, byteLength);
}
