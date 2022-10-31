#include <gtest/gtest.h>
#include <tcpsocket.h>
#include <tcpserver.h>

TEST(TcpSocketServerTests, pingPongTest)
{
    TcpServer server;
    server.onNewConnection([&server]
    {
        auto client = server.nextPendingConnection();
        client->onData([client] (const char *data, size_t size)
        {
            client->write("Pong: " + std::string(data, size));
        });

        client->onDisconnected([client]
        {
            delete client;
        });
    });
    server.listen("localhost", 1234);

    TcpSocket client;
    client.onConnected([&client]
    {
        client.write("Ping");
    });

    client.onData([&client] (const char *data, size_t size)
    {
        ASSERT_STREQ("Pong: Ping", std::string(data, size).c_str());
        client.disconnectFromHost();
    });

    client.connectToHost("localhost", 1234);
    ASSERT_EQ(client.waitForConnected(), true);
    ASSERT_EQ(client.waitForDisconnected(), true);
}
