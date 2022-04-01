/**
 * 这是一个使用 C++ 实现的 TCP 服务端程序。它的功能是监听指定端口，把客户端发送的
 * 内容输出到标准输出，然后返回一个应答消息。后续循环监听客户端的消息，但是不
 * 回复。
 *
 * 程序的使用方法是，在终端启动时指定参数，./tcp_server port，port是服务器端口。
 */
#include "server.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << "Usage: ./tcp_server port" << endl;
        return -1;
    }
    auto port = atoi(argv[1]);
    cout << "Port: " << port << endl;

    // 创建 socket
    auto socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == socketId) {
        cout << "Socket creation failed." << endl;
        return -1;
    }

    try {
        // 配置监听地址信息
        auto addressInfo = new sockaddr_in();
        addressInfo->sin_family = AF_INET;
		// 使用htons和htonl将主机字节顺序转变为网络字节顺序, 改为big-endian
        addressInfo->sin_port = htons(port);
		// 监听所有网络发送过来的请求
        addressInfo->sin_addr.s_addr = htonl(INADDR_ANY);
		// 将地址(ipv4 + port)绑定到socket
        if (0 != ::bind(socketId, (sockaddr *)addressInfo, sizeof(sockaddr))) {
            delete addressInfo;
            perror("Bind error ");
            throw "Server start fail.";
        }
        cout << "Bind success" << endl;
        if (0 != listen(socketId, SOMAXCONN)) {
            delete addressInfo;
            perror("Fail, can't listen ");
            throw "Server start fail.";
        }

        serverConnection server(socketId);
        int connects = 0;
        while (true) {
			// client连接server
            // accept: SYN K, Ark J+1
            cout << "Listening..." << endl;
            server.accept();
            // 连接socket
            if (server.start(connects)) {
                // 发送和接收消息
                thread serverThread(&serverConnection::runThread, ref(server), connects);
                serverThread.join();
            } else
                // 接收视频
                server.recvVideo(connects);
            connects++;
        }
        delete addressInfo;
    } catch (const char *error) {
        cout << "Error, " << error << endl;
    }
    
    return 0;
}
