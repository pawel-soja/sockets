#include "tcpserver.h"
#include "tcpsocket.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <select.h>
#include <cstring>

class TcpServerPrivate: public TcpSocket
{
    public:
        TcpServerPrivate(TcpServer *parent);

    public:
        void readyRead() override;

    public:
        TcpServer *parent;

        std::function<void()> onNewConnection;
};

// TcpServerPrivate

TcpServerPrivate::TcpServerPrivate(TcpServer *parent)
    : parent(parent)
{ }

void TcpServerPrivate::readyRead()
{
    if (onNewConnection)
        onNewConnection();
    else
        delete parent->nextPendingConnection();
}

// TcpServer

TcpServer::TcpServer()
    : d_ptr(new TcpServerPrivate(this))
{ }

TcpServer::~TcpServer()
{ }

bool TcpServer::listen(const std::string &address, unsigned short port)
{
    return d_ptr->bind(address, port);
}

void TcpServer::onNewConnection(const std::function<void()> &callback)
{
    d_ptr->onNewConnection = callback;
}

ptrdiff_t TcpServer::nextPendingSocketDescriptor()
{
    auto serverFd = d_ptr->socketDescriptor();
    struct sockaddr_in sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    socklen_t len = sizeof(sockaddr);

    auto clientFd = ::accept(serverFd, (struct sockaddr *)&sockaddr, &len);
    if (clientFd < 0)
    {
        perror("accept");
        return SocketInvalid;
    }

    return clientFd;
}

TcpSocket *TcpServer::nextPendingConnection()
{
    TcpSocket *client = new TcpSocket();

    client->setSocketDescriptor(nextPendingSocketDescriptor());

    return client;
}
