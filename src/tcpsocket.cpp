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
#include "tcpsocket.h"
#include "tcpsocket_p.h"

#include "socketaddress.h"

#include <errno.h>
#include <chrono>
#include <thread>
#include <algorithm>

// TcpSocketPrivate
TcpSocketPrivate::TcpSocketPrivate(TcpSocket *parent)
    : parent(parent)
{ }

TcpSocket::TcpSocket(std::shared_ptr<TcpSocketPrivate> &&d)
    : d_ptr(std::move(d))
{ }

ssize_t TcpSocketPrivate::write(const void *data, size_t size)
{
    int ret;
    do
    {
        std::unique_lock<std::mutex> locker(socketStateMutex);
        if (socketState != TcpSocket::ConnectedState && socketState != TcpSocket::BoundState)
        {
            return 0;
        }
        ret = TcpSocketPrivate::sendSocket(data, size);
    }
    while (ret == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

    if (ret < 0)
    {
        aboutToClose();
    }

    return std::max(ret, 0);
}

bool TcpSocketPrivate::connectSocket(const std::string &address, unsigned short port)
{
    // create socket handle
    if (!createSocket(SocketAddress::isUnix(address) ? AF_UNIX : AF_INET))
    {
        setSocketError(TcpSocket::SocketResourceError);
        return false;
    }

    // set non blocking mode
    if (!setNonblockSocket())
    {
        setSocketError(TcpSocket::UnknownSocketError);
        return false;
    }

    // get socket address
    auto sockAddr = SocketAddress(address, port);

    if (!sockAddr.isValid())
    {
        setSocketError(TcpSocket::HostNotFoundError);
        return false;
    }

    // connect to host
    if (::connect(socketFd, &sockAddr, sockAddr.size()) < 0 && errno != EINPROGRESS)
    {
        setSocketError(TcpSocket::UnknownSocketError);
        return false;
    }

    return true;
}

bool TcpSocketPrivate::bindSocket(const std::string &address, unsigned short port)
{
    // create socket handle
    if (!createSocket(SocketAddress::isUnix(address) ? AF_UNIX : AF_INET))
    {
        setSocketError(TcpSocket::SocketResourceError);
        return false;
    }

    // set non blocking mode
    if (!setNonblockSocket())
    {
        setSocketError(TcpSocket::UnknownSocketError);
        return false;
    }

    // get socket address
    SocketAddress sockAddr(address, port);

    if (!sockAddr.isValid())
    {
        setSocketError(TcpSocket::HostNotFoundError);
        return false;
    }

#ifdef __linux__
    sockAddr.unlink();
#endif

    // bind socket
    if (::bind(socketFd, &sockAddr, sockAddr.size()) < 0 && errno != EINPROGRESS)
    {
        setSocketError(TcpSocket::UnknownSocketError);
        return false;
    }

    if (::listen(socketFd, 256) < 0)
    {
        setSocketError(TcpSocket::UnknownSocketError);
        return false;
    }

    return true;
}

bool TcpSocketPrivate::waitForConnectedSockets()
{
    // wait for connect
    select.clear();
    select.setReadWriteExceptionEvent(socketFd);
    select.select(timeout);

    if (select.isTimeout())
    {
        setSocketError(TcpSocket::SocketTimeoutError);
        return false;
    }

    if (select.isWakedUp())
    {
        return false;
    }

    return TcpSocketPrivate::sendSocket("", 0) == 0; // final test, -1 if not connected
}

bool TcpSocketPrivate::processSocket()
{
    select.clear();
    select.setReadEvent(socketFd);
#ifndef _WIN32
    select.setTimeout(10 * 1000); // we can wake up
#else
    select.setTimeout(100);
#endif

    select.select();

    // timeout, continue...
    if (select.isTimeout())
    {
        return true;
    }

    // manual wakeup, maybe isAboutToClose, exit
    if (select.isWakedUp())
    {
        return true;
    }

    // nothing to do
    if (!select.isReadEvent(socketFd))
    {
        return true;
    }

    // call virtual method
    parent->readyRead();

    return true;
}

class Finally
{
        typedef std::function<void()> F;
        F f;
    public:
        Finally(const F &f) : f(f) { }
        ~Finally()
        {
            if (f) f();
        }
};

void TcpSocketPrivate::connectToHost(const std::string &hostName, unsigned short port)
{
    if (socketState != TcpSocket::UnconnectedState)
    {
        setSocketError(TcpSocket::OperationError);
        return;
    }

    setSocketState(TcpSocket::HostLookupState);

    std::thread([this, hostName, port]
    {
        std::shared_ptr<TcpSocketPrivate> priv = this->parent->d_ptr;
        this->threadId = std::this_thread::get_id();
        Finally finally([this]
        {
            closeSocket();
            setSocketState(TcpSocket::UnconnectedState);
        });

        // lookup and connect
        if (!connectSocket(hostName, port))
        {
            // see error in connectSocket
            return;
        }
        // wait for connected
        setSocketState(TcpSocket::ConnectingState);
        if (!waitForConnectedSockets())
        {
            setSocketError(TcpSocket::SocketError::HostNotFoundError);
            return;
        }

        setSocketState(TcpSocket::ConnectedState);
        parent->connected();
        while (isAboutToClose == false && processSocket());

        if (errno != 0 && errno != EINPROGRESS)
        {
            setSocketError(TcpSocket::ConnectionRefusedError);
        }

        setSocketState(TcpSocket::SocketState::ClosingState);
        parent->disconnected();
    }).detach();
}

bool TcpSocketPrivate::bind(const std::string &hostName, unsigned short port)
{
    if (socketState != TcpSocket::UnconnectedState)
    {
        setSocketError(TcpSocket::OperationError);
        return false;
    }

    setSocketState(TcpSocket::HostLookupState);

    // lookup and connect
    if (!bindSocket(hostName, port))
    {
        // see error in connectSocket
        closeSocket();
        setSocketState(TcpSocket::UnconnectedState);
        return false;
    }
    setSocketState(TcpSocket::BoundState);

    std::thread([this, hostName, port]
    {
        std::shared_ptr<TcpSocketPrivate> priv = this->parent->d_ptr;
        this->threadId = std::this_thread::get_id();
        Finally finally([this]
        {
            closeSocket();
            setSocketState(TcpSocket::UnconnectedState);
        });

        parent->connected();
        while (isAboutToClose == false && processSocket());

        if (errno != 0 && errno != EINPROGRESS)
        {
            setSocketError(TcpSocket::ConnectionRefusedError);
        }

        setSocketState(TcpSocket::SocketState::ClosingState);
        parent->disconnected();
    }).detach();

    return true;
}

bool TcpSocketPrivate::bind(SocketFileDescriptor fd)
{
    if (socketState != TcpSocket::UnconnectedState)
    {
        setSocketError(TcpSocket::OperationError);
        return false;
    }

    socketFd = fd;
    setSocketState(TcpSocket::BoundState);

    std::thread([this, fd] ()
    {
        std::shared_ptr<TcpSocketPrivate> priv = this->parent->d_ptr;
        this->threadId = std::this_thread::get_id();
        Finally finally([this]
        {
            closeSocket();
            setSocketState(TcpSocket::UnconnectedState);
        });

        setSocketState(TcpSocket::BoundState);
        parent->connected();
        while (isAboutToClose == false && processSocket());

        if (errno != 0 && errno != EINPROGRESS)
        {
            setSocketError(TcpSocket::ConnectionRefusedError);
        }

        setSocketState(TcpSocket::SocketState::ClosingState);
        parent->disconnected();
    }).detach();

    return true;
}

void TcpSocketPrivate::aboutToClose()
{
    std::unique_lock<std::mutex> locker(socketStateMutex);
    if (socketState == TcpSocket::UnconnectedState || isAboutToClose.exchange(true) == true)
    {
        return;
    }
    select.wakeUp();
}

void TcpSocketPrivate::closeSocket()
{
    if (socketFd == SocketInvalid)
        return;

#ifdef _WIN32
    ::closesocket(socketFd);
#else
    ::close(socketFd);
#endif
    socketFd = SocketInvalid;
}

void TcpSocketPrivate::setSocketError(TcpSocket::SocketError error, ErrorType errorType, const std::string &errorString)
{
    if (errorType == ErrorTypeSystem && errorString == "")
    {
#ifdef _WIN32
        LPTSTR s = nullptr;
        auto code = WSAGetLastError();
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&s, 0, nullptr
        );

#ifdef UNICODE
        std::wstring ws(s);
        this->errorString = std::string(ws.begin(), ws.end());
#else
        this->errorString = s;
#endif
        LocalFree(s);

        this->errorString += " (" + std::to_string(code) + ")";

#else
        this->errorString = strerror(errno);
        this->errorString += " (" + std::to_string(errno) + ")";
#endif
    }
    else
    {
        this->errorString = errorString;
    }
    socketError = error;
    parent->errorOccurred(error);
}

void TcpSocketPrivate::setSocketState(TcpSocket::SocketState state)
{
    std::unique_lock<std::mutex> locker(socketStateMutex);
    if (socketState.exchange(state) == state)
        return;
    socketStateChanged.notify_all();
}

// TcpSocket
TcpSocket::TcpSocket()
    : d_ptr(new TcpSocketPrivate(this))
{ }

TcpSocket::~TcpSocket()
{
    disconnectFromHost();
    waitForDisconnected();
}

void TcpSocket::setConnectionTimeout(int timeout)
{
    d_ptr->timeout = timeout;
}

void TcpSocket::connectToHost(const std::string &hostName, unsigned short port)
{
    d_ptr->connectToHost(hostName, port);
}

void TcpSocket::disconnectFromHost()
{
    d_ptr->aboutToClose();
}

bool TcpSocket::bind(const std::string &address, unsigned short port)
{
    return d_ptr->bind(address, port);
}

int TcpSocket::write(const char *data, int size)
{
    return d_ptr->write(data, size);
}

int TcpSocket::write(const std::string &data)
{
    return write(data.data(), data.size());
}

static std::string sSocketErrorToString(TcpSocket::SocketError error)
{
    switch (error)
    {
        case TcpSocket::ConnectionRefusedError:
            return "ConnectionRefusedError";
        case TcpSocket::RemoteHostClosedError:
            return "RemoteHostClosedError";
        case TcpSocket::HostNotFoundError:
            return "HostNotFoundError";
        case TcpSocket::SocketAccessError:
            return "SocketAccessError";
        case TcpSocket::SocketResourceError:
            return "SocketResourceError";
        case TcpSocket::SocketTimeoutError:
            return "SocketTimeoutError";
        case TcpSocket::DatagramTooLargeError:
            return "DatagramTooLargeError";
        case TcpSocket::NetworkError:
            return "NetworkError";
        case TcpSocket::AddressInUseError:
            return "AddressInUseError";
        case TcpSocket::SocketAddressNotAvailableError:
            return "SocketAddressNotAvailableError";
        case TcpSocket::UnsupportedSocketOperationError:
            return "UnsupportedSocketOperationError";
        case TcpSocket::UnfinishedSocketOperationError:
            return "UnfinishedSocketOperationError";
        case TcpSocket::ProxyAuthenticationRequiredError:
            return "ProxyAuthenticationRequiredError";
        case TcpSocket::SslHandshakeFailedError:
            return "SslHandshakeFailedError";
        case TcpSocket::ProxyConnectionRefusedError:
            return "ProxyConnectionRefusedError";
        case TcpSocket::ProxyConnectionClosedError:
            return "ProxyConnectionClosedError";
        case TcpSocket::ProxyConnectionTimeoutError:
            return "ProxyConnectionTimeoutError";
        case TcpSocket::ProxyNotFoundError:
            return "ProxyNotFoundError";
        case TcpSocket::ProxyProtocolError:
            return "ProxyProtocolError";
        case TcpSocket::OperationError:
            return "OperationError";
        case TcpSocket::SslInternalError:
            return "SslInternalError";
        case TcpSocket::SslInvalidUserDataError:
            return "SslInvalidUserDataError";
        case TcpSocket::TemporaryError:
            return "TemporaryError";
        case TcpSocket::UnknownSocketError:
            return "UnknownSocketError";
    }
    return "UnknownSocketError";
}

TcpSocket::SocketError TcpSocket::error() const
{
    return d_ptr->socketError;
}

std::string TcpSocket::errorString()
{
    return sSocketErrorToString(d_ptr->socketError) + ": " + d_ptr->errorString;
}

void TcpSocket::onConnected(const std::function<void()> &callback)
{
    d_ptr->onConnected = callback;
}

void TcpSocket::onDisconnected(const std::function<void()> &callback)
{
    d_ptr->onDisconnected = callback;
}

void TcpSocket::onData(const std::function<void(const char *, size_t)> &callback)
{
    d_ptr->onData = callback;
}

void TcpSocket::onErrorOccurred(const std::function<void(SocketError)> &callback)
{
    d_ptr->onErrorOccurred = callback;
}

bool TcpSocket::waitForConnected(int timeout) const
{
    if (d_ptr->threadId == std::this_thread::get_id())
    {
        d_ptr->setSocketError(TcpSocket::SocketError::OperationError);
        return false;
    }

    std::unique_lock<std::mutex> locker(d_ptr->socketStateMutex);
    d_ptr->socketStateChanged.wait_for(locker, std::chrono::milliseconds(timeout), [this]
    {
        return d_ptr->socketState == TcpSocket::ConnectedState || d_ptr->socketState == TcpSocket::UnconnectedState;
    });
    return d_ptr->socketState == TcpSocket::ConnectedState;
}

bool TcpSocket::waitForDisconnected(int timeout) const
{
    if (d_ptr->threadId == std::this_thread::get_id())
    {
        d_ptr->setSocketError(TcpSocket::SocketError::OperationError);
        return false;
    }

    std::unique_lock<std::mutex> locker(d_ptr->socketStateMutex);
    return d_ptr->socketStateChanged.wait_for(locker, std::chrono::milliseconds(timeout), [this]
    {
        return d_ptr->socketState == TcpSocket::UnconnectedState || d_ptr->socketState == TcpSocket::ClosingState;
    });
}

ptrdiff_t TcpSocket::socketDescriptor() const
{
    return d_ptr->socketFd;
}

bool TcpSocket::setSocketDescriptor(ptrdiff_t socketDescriptor)
{
    return d_ptr->bind(socketDescriptor);
}

void TcpSocket::connected()
{
    emitConnected();
}

void TcpSocket::disconnected()
{
    emitDisconnected();
}

void TcpSocket::readyRead()
{
    char data[65536];
    ssize_t size = d_ptr->recvSocket(data, sizeof(data));

    if (size <= 0)
    {
        disconnectFromHost();
        return;
    }

    emitData(data, size);
}

void TcpSocket::errorOccurred(SocketError error)
{
    emitErrorOccurred(error);
}

void TcpSocket::emitConnected()
{
    if (d_ptr->onConnected)
        d_ptr->onConnected();
}

void TcpSocket::emitDisconnected()
{
    if (d_ptr->onDisconnected)
        d_ptr->onDisconnected();
}

void TcpSocket::emitData(const char *data, size_t size)
{
    if (d_ptr->onData)
        d_ptr->onData(data, size);
}

void TcpSocket::emitErrorOccurred(SocketError error)
{
    if (d_ptr->onErrorOccurred)
        d_ptr->onErrorOccurred(error);
}
