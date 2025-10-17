#include "tcp_server.h"

int main()
{
    int val;
    vuprs::TcpServer server("./server_config.json");
    server.start();

    std::cin >> val;

    return 0;
}