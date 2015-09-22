//
// Copyright (c) 2015 Singular Inversions Inc. (facegen.com)
// Use, modification and distribution is subject to the MIT License,
// see accompanying file LICENSE.txt or facegen.com/base_library_license.txt
//
// Authors:     Andrew Beatty
// Created:     Nov 1, 2011
//

#include "stdafx.h"
#include "FgTcp.hpp"
#include "FgThrowWindows.hpp"
#include "FgStdString.hpp"
#include "FgDiagnostics.hpp"
#include "FgOut.hpp"

// Tell compiler to link to these libs:
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

struct  WinsockDll
{
    WSADATA     wsaData;

    WinsockDll() {
        // Initialize Winsock DLL version 2.2:
        int         itmp = WSAStartup(MAKEWORD(2,2),&wsaData);
        if(itmp != 0)
            FGASSERT_FALSE1(fgToString(itmp));
    }

    ~WinsockDll() {
        // Every successful call to WSAStartup must be matched by a WSACleanup()
        // (windows uses a counter):
        WSACleanup();
    }
};

static
void
initWinsock()
{
    static  WinsockDll  wsd;
}

bool
fgTcpClient(
    const string &      hostname,
    uint16              port,
    const string &      data,
    bool                getResponse,
    string &            response)
{
    initWinsock();

    SOCKET              socketHandle;
    struct addrinfo *   addressInfo;
    socketHandle = INVALID_SOCKET;
    addressInfo = NULL;
    struct addrinfo     hints;
    int                 itmp;

    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port.
    itmp = getaddrinfo(
        hostname.c_str(),           // Hostname or IP #
        fgToString(port).c_str(),   // Service name or port #
        &hints,
        &addressInfo);              // RETURNED
    if (itmp != 0)
        return false;               // Server does not appear to exist

    struct addrinfo *ptr = NULL;
    // Attempt to connect to an IP address until one succeeds. It's possible
    // for more than 1 to be returned if there are more than 1 namespace service
    // providers (for instance local hosts file vs. DNS server), according to
    // MS docs:
    for(ptr=addressInfo; ptr != NULL ;ptr=ptr->ai_next) {
        socketHandle =
            socket(
                ptr->ai_family,
                ptr->ai_socktype, 
                ptr->ai_protocol);
        if (socketHandle == INVALID_SOCKET) {
            // Couldn't use scope guard for freeaddrinfo due to compile issues:
            freeaddrinfo(addressInfo);
            FGASSERT_FALSE1(fgToString(WSAGetLastError()));
        }
        // Try to connect to the server
        itmp = connect(
            socketHandle,
            ptr->ai_addr,
            (int)ptr->ai_addrlen);
        if (itmp == 0)
            break;
        else {
            closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
        }
    }
    freeaddrinfo(addressInfo);
    if (socketHandle == INVALID_SOCKET)
        return false;               // Unable to connect, server not listening ?

    // 'send' will block for buffering since we've created a blocking socket, so the
    // value returned is always either the data size or an error:
    itmp = send(socketHandle,data.data(),int(data.size()),0);
    FGASSERT1(itmp != SOCKET_ERROR,fgToString(WSAGetLastError()));

    // close socket for sending to cause server's recv/read to return a zero
    // size data packet if server is waiting for more (ie to flush the stream).
    itmp = shutdown(socketHandle,SD_SEND);
    FGASSERT1(itmp != SOCKET_ERROR,fgToString(WSAGetLastError()));

    if (getResponse) {
        response.clear();
        do {
            char    buff[1024];
            // If server doesn't respond and closes connection we'll immediately get
            // a value of zero here and nothing will be placed in buff. Otherwise
            // we'll continue to receive data until server closes connection causing
            // the zero message:
            itmp = recv(socketHandle,buff,sizeof(buff),0);
            FGASSERT1(itmp != SOCKET_ERROR,fgToString(WSAGetLastError()));
            if (itmp > 0)
                response += string(buff,itmp);
        }
        while (itmp > 0);
        FGASSERT(itmp == 0);
    }

    // Couldn't use scope guard due to compile issues:
    closesocket(socketHandle);
    return true;
}

void
fgTcpServer(
    uint16      port,
    bool        respond,
    bool(*handler)(const string & ipAddr,const string & dataIn,string & response),
    size_t      maxRecvBytes)
{
    initWinsock();
    SOCKET      sockListen = INVALID_SOCKET;

    // Set up the listening socket:
    struct addrinfo     hints;
    ZeroMemory(&hints,sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // A reliable, 2-way, stream-based connection (requires TCP)
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo     *addrInfoPtr = NULL;
    int itmp = getaddrinfo(NULL,fgToString(port).c_str(),&hints,&addrInfoPtr);
    FGASSERT1(itmp == 0,fgToString(itmp));
    sockListen = socket(addrInfoPtr->ai_family,addrInfoPtr->ai_socktype,addrInfoPtr->ai_protocol);
    if (sockListen == INVALID_SOCKET) {
        freeaddrinfo(addrInfoPtr);
        FGASSERT_FALSE1(fgToString(WSAGetLastError()));
    }
    itmp = bind(sockListen,addrInfoPtr->ai_addr,(int)addrInfoPtr->ai_addrlen);
    if (itmp == SOCKET_ERROR) {
        closesocket(sockListen);
        freeaddrinfo(addrInfoPtr);
        FGASSERT_FALSE1(fgToString(WSAGetLastError()));
    }
    itmp = listen(sockListen, SOMAXCONN);
    if (itmp == SOCKET_ERROR) {
        closesocket(sockListen);
        freeaddrinfo(addrInfoPtr);
        FGASSERT_FALSE1(fgToString(WSAGetLastError()));
    }
    freeaddrinfo(addrInfoPtr);

    // Receive messages and respond until finished:
    SOCKET      sockClient;
    bool        handlerRetval = true;
    do {
        fgout << fgnl << "> " << std::flush;
        sockaddr_in     sa;
        sa.sin_family = AF_INET;
        socklen_t       sz = sizeof(sa);
        sockClient = accept(sockListen,(sockaddr*)(&sa),&sz);
        if (sockClient == INVALID_SOCKET) {
            closesocket(sockListen);
            FGASSERT_FALSE1(fgToString(WSAGetLastError()));
        }
        char * clientStringPtr = inet_ntoa(sa.sin_addr);
            FGASSERT(clientStringPtr != NULL);
        string     ipAddr = string(clientStringPtr);
        //fgout << "receiving from " << ipAddr << " ... " << std::flush;
        string     dataBuff;
        int itmp = 0;
        do {
            char    recvbuf[1024];
            // recv() will return when either it has filled the buffer, copied over everthing
            // from the socket input buffer (only if non-empty), or when the the read connection
            // is closed by the client. Otherwise it will block (ie if input buffer empty):
            itmp = recv(sockClient,recvbuf,sizeof(recvbuf),0);
            fgout << "." << std::flush;
            if (itmp > 0)
                dataBuff += string(recvbuf,itmp);
        }
        while ((itmp > 0) && (dataBuff.size() <= maxRecvBytes));
        if (itmp != 0) {
            closesocket(sockClient);
            if (itmp < 0)
                fgout << "TCP RECV ERROR: " << itmp;
            else if (itmp > 0)
                fgout << " OVERSIZE MESSAGE IGNORED.";
            fgout << std::flush;
            continue;
        }
        fgout << ": " << std::flush;
        if (!respond)   // Avoid timeout errors on the data socket for long handlers that don't respond:
            closesocket(sockClient);
        string     response;
        try {
            handlerRetval = handler(ipAddr,dataBuff,response);
        }
        catch(FgException const & e) {
            fgout << "Handler exception (FG4 exception): " << e.no_tr_message();
        }
        catch(std::exception const & e) {
            fgout << "Handler exception (std::exception): " << e.what();
        }
        catch(...) {
            fgout << "Handler exception (unknown type)";
        }
        if (respond) {
            if (!response.empty()) {
                int     bytesSent = send(sockClient,response.data(),int(response.size()),0);
                shutdown(sockClient,SD_SEND);
                if (bytesSent != int(response.size()))
                    fgout << "TCP SEND ERROR: " << bytesSent << " (of " << response.size() << ").";
            }
            closesocket(sockClient);
        }
        fgout << std::flush;
    } while (handlerRetval == true);
    closesocket(sockListen);
}

// */
