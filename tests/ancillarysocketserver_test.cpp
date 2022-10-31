#include <gtest/gtest.h>
#include <ancillarytcpsocket.h>
#include <tcpserver.h>

TEST(AncillarySocketServerTests, pingPongTest)
{
    TcpServer server;
    server.onNewConnection([&server]
    {
        AncillaryTcpSocket *client = new AncillaryTcpSocket();
        client->setSocketDescriptor(server.nextPendingSocketDescriptor());

        client->onData([client] (const char *data, size_t size)
        {
            ASSERT_EQ(client->fds().size(), 2);

            client->write("Pong: " + std::string(data, size));
        });

        client->onDisconnected([client]
        {
            delete client;
        });
    });
    server.listen("localhost:/tmp/AncillarySocketServerTests", 0);

    AncillaryTcpSocket client;
    client.onConnected([&client]
    {
        std::vector<int> fds = {1, 2};
        client.sendWithFds("Ping", fds);
    });

    client.onData([&client] (const char *data, size_t size)
    {
        ASSERT_STREQ("Pong: Ping", data);
        ASSERT_EQ(strlen(data), size);
        client.disconnectFromHost();
    });

    client.connectToHost("localhost:/tmp/AncillarySocketServerTests", 0);
    ASSERT_EQ(client.waitForConnected(), true);
    ASSERT_EQ(client.waitForDisconnected(), true);
}
