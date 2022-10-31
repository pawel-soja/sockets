/*
    Copyright (C) 2022 by Pawel Soja <kernel32.pl@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "tcpsocket.h"
#include "select.h"

#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cstdio>

#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

class TcpSocketPrivate
{
    public:
        TcpSocketPrivate(TcpSocket *parent);
        virtual ~TcpSocketPrivate() = default;

    public: // platform dependent
        bool createSocket(int domain = AF_INET);
        void closeSocket();
        int recvSocket(void *dst, size_t size);
        int sendSocket(const void *src, size_t size);
        bool setNonblockSocket();

    public: // low level helpers
        bool connectSocket(const std::string &hostName, unsigned short port);
        bool waitForConnectedSockets();
        bool processSocket();
        bool bindSocket(const std::string &address, unsigned short port);

    public: // TcpSocketPrivate API
        ssize_t write(const void *data, size_t size);

        void connectToHost(const std::string &hostName, unsigned short port);
        void aboutToClose();

        bool bind(const std::string &hostname, unsigned short port);
        bool bind(SocketFileDescriptor fd);

    public:
        enum ErrorType
        {
            ErrorTypeSystem,
            ErrorTypeInternal
        };
        void setSocketError(TcpSocket::SocketError error, ErrorType errorType = ErrorTypeSystem,
                            const std::string &errorString = "");
        void setSocketState(TcpSocket::SocketState state);

    public:
        TcpSocket *parent;
        SocketFileDescriptor socketFd = SocketInvalid;
        Select select;
        int timeout{30000};

        std::atomic<bool> isAboutToClose{false};

        std::thread::id threadId;
        mutable std::mutex socketStateMutex;
        mutable std::condition_variable socketStateChanged;

        std::atomic<TcpSocket::SocketState> socketState{TcpSocket::UnconnectedState};
        TcpSocket::SocketError socketError;
        std::string errorString;

        // events
        std::function<void()> onConnected;
        std::function<void()> onDisconnected;
        std::function<void(const char *, size_t)> onData;
        std::function<void(TcpSocket::SocketError)> onErrorOccurred;
};
