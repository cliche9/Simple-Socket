/**
 * 这是一个使用 C++ 实现的 TCP 客户端程序。它的功能是主动连接一个服务器端，然后
 * 实现双端的实时通信，可以选择通信模式：文本/文件/视频...
 *
 * 程序的使用方法是，在终端启动时指定参数，./tcp_client host port，host是服务器端
 * 主机名，port是端口
 */
#include "client.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        cout << "Usage: ./tcp_client host port" << endl;
        return -1;
    }
    cout << "Host: " << argv[1] << endl;
    auto port = atoi(argv[2]);
    cout << "Port: " << port << endl;

    // 创建 socket
    auto socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketId) {
        cout << "Socket creation failed." << endl;
        return -1;
    }

    try {
        // 配置连接地址信息
        auto addressInfo = new sockaddr_in();
        addressInfo->sin_family = AF_INET;
        addressInfo->sin_port = htons(port);
        if (0 == inet_aton(argv[1], &(addressInfo->sin_addr))) { // 如果不是IP地址格式的字符串
            cout << "Host to ip..." << endl;
            auto hostInfo = gethostbyname(argv[1]);
            if (nullptr == hostInfo) {
                delete addressInfo;
                throw "gethostbyname() fail!";
            }
            cout << "h_length: " << hostInfo->h_length << endl;
            if (hostInfo->h_length <= 0) {
                delete addressInfo;
                throw "No address info";
            }
            addressInfo->sin_family = hostInfo->h_addrtype; // 有可能是IPv6

            auto ipAddress = new char[100];
            // 取第一个IP地址
            if (nullptr == inet_ntop(hostInfo->h_addrtype, hostInfo->h_addr, ipAddress, 100)) {
                delete addressInfo;
                delete[] ipAddress;
                throw "inet_ntop() fail";
            }
            cout << "IP address: " << ipAddress << endl;
            if (0 == inet_aton(ipAddress, &(addressInfo->sin_addr))) {
                delete addressInfo;
                delete[] ipAddress;
                throw "IP address error";
            }
            delete[] ipAddress;
        }
        cout << "Setup address success." << endl;

        // 连接
        cout << "Connect..." << endl;
        // 连接socket
        clientConnection client(socketId, addressInfo, sizeof(sockaddr_in));
        if (client.start()) {
            // 发送和接收消息
            thread clientThread(&clientConnection::runThread, ref(client));
            clientThread.join();
        } else
            // 发送视频
            client.sendVideo();
        // 连接结束
        delete addressInfo;
    } catch (const char *error) {
        cout << "Error, " << error << endl;
    }

    return 0;
}
