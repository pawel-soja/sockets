#pragma once

#include <string>
#include <memory>
#include <functional>

class TcpSocket;
class TcpServerPrivate;;
class TcpServer
{
    public:
        TcpServer();
        virtual ~TcpServer();

    public:
        bool listen(const std::string &address, unsigned short port);

    public:
        void onNewConnection(const std::function<void()> &callback);

    public:
        TcpSocket *nextPendingConnection();
        ptrdiff_t nextPendingSocketDescriptor();

    protected:
        std::unique_ptr<TcpServerPrivate> d_ptr;
};
