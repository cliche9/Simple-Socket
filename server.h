#ifndef _SERVER_H_
#define _SERVER_H_
#include <opencv4/opencv2/opencv.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <fstream>
#include <iostream>
#define NumberOfPackage 32
#define ImageWidth 640
#define ImageHeight 480
#define ImageBufferSize ImageWidth * ImageHeight * 3 / NumberOfPackage
#define bufferSize 100
#define numberOfClients 10
using namespace std;

/*
 * socketId: 服务器的socket
 * peerAddress[i]: 第i个客户端的ip地址、端口信息
 * ipAddress[i]: 第i个客户端的xxx.xxx.xxx.xxx的ip地址
 * peerSocket[i]: 第i个客户端的socket描述字
 * numberOfConnection: 当前连接数量, 未实现多连接
 * buffer: 收发缓冲区
 * fileName: 收发文件名
 * isFileToSend: 当前是否处于发送文件状态
 * clientLock: 锁, 未使用到
 */
struct videoBuffer {
    char buffer[ImageBufferSize];
    int tag;
};

class serverConnection {
private:
    int socketId;
    socklen_t *lengthOfSocketAddress;
    sockaddr **peerAddress;
    string *ipAddress;
    int *peerSocket;
    int numberOfConnection;
    char *buffer;
    videoBuffer vBuffer;
    string fileName;
    bool isFileToSend;
    mutex serverLock;
public:
    serverConnection(int socketId) {
        this->socketId = socketId;
        // socket地址共16byte
        lengthOfSocketAddress = new socklen_t(sizeof(sockaddr));
        peerAddress = new sockaddr *[numberOfClients];
        for (int i = 0; i < numberOfClients; i++)
            peerAddress[i] = new sockaddr();
        ipAddress = new string[numberOfClients];
        peerSocket = new int[numberOfClients];
        numberOfConnection = 0;
        buffer = new char[bufferSize];
        isFileToSend = false;
    }
    
    ~serverConnection() {
        for (int i = 0; i < numberOfConnection; i++) {
            if (-1 != peerSocket[i] && 0 != shutdown(peerSocket[i], SHUT_RDWR)) {
                cout << "Shutdown client socket fail" << endl;
                perror("Error info ");
            }
            if (-1 != peerSocket[i]) {
                close(peerSocket[i]);
            }
        }
        for (int i = 0; i < numberOfClients; i++)
            delete peerAddress[i];
        delete[] buffer;
        delete[] peerAddress;
        delete[] ipAddress;
        delete[] peerSocket;
        delete lengthOfSocketAddress;
    }

    inline void accept() {
        // 与accept()队列中的客户端建立连接
        if (-1 == (peerSocket[numberOfConnection] = ::accept(socketId, peerAddress[numberOfConnection], lengthOfSocketAddress))) {
            perror("Connect Error ");
            throw "Connect fail!";
        }
        sockaddr_in *clientAddr = (sockaddr_in *)peerAddress[numberOfConnection];
        ipAddress[numberOfConnection] = inet_ntoa(clientAddr->sin_addr);
        cout << "Connect success: " << ipAddress[numberOfConnection] << endl;
        buffer[0] = '\0';
        ++numberOfConnection;
    }

    bool start(int clientNumber) {
        // 开始函数, 传文件/信息返回1, 传视频返回0
        // 接收信号
        ssize_t recvLength = recv(peerSocket[clientNumber], buffer, bufferSize - 1, 0);
        buffer[recvLength] = '\0';
        cout << ipAddress[clientNumber] << ": " << buffer << endl;
        if (!strcmp(buffer, "$video")) 
            return 0;
        if (!strcmp(buffer, "#start"))
            return 1;
        return 1;
    }

    void runThread(int clientNumber) {
        // 运行线程, 该线程对编号为clientNumber的客户端进行收发操作
        thread serverRecvThread(&serverConnection::sendThread, ref(*this), clientNumber);
        thread serverSendThread(&serverConnection::recvThread, ref(*this), clientNumber);
        serverSendThread.join();
        serverRecvThread.join();
    }

    void recvThread(int clientNumber) {
        // 对第clientNumber号客户端进行接收
        while (true) {
            // 接收信息
            ssize_t recvLength;
            recvLength = recv(peerSocket[clientNumber], buffer, bufferSize - 1, 0);
            // 是否退出
            if (recvLength == 0)
                return;
            if (recvLength < 0) {
                perror("Receive data fail\n");
                return;
            }
            buffer[recvLength] = '\0';
            cout << ipAddress[clientNumber] << ": " << buffer << endl;
            // 收到确认ark, 传文件
            if (!strcmp(buffer, "#ack") && isFileToSend) {
                sendFile(clientNumber);
                continue;
            }
            // 收文件
            if (buffer[0] == '#') {
                fileName = buffer + 1;
                recvFile(clientNumber);
                continue;
            }
        }
    }

    void sendThread(int clientNumber) {
        // 对客户端进行发送
        while (true) {
            cin.getline(buffer, bufferSize);
            // 直接退出
            if (!strcmp(buffer, "exit"))
                return;
            // 正常传信息
            ssize_t sendLength = strlen(buffer);
            ssize_t realSendLength = send(peerSocket[clientNumber], buffer, sendLength, 0);
            if (sendLength != realSendLength) {
                perror("Send error\n");
                return;
            }
            // 结束传文件
            if (!strcmp(buffer, "#end") && isFileToSend) {
                cout << "Successfully sent file: " << fileName << " to client: " << ipAddress[clientNumber] << endl;
                isFileToSend = false;
                continue;
            }
            // 开启传文件
            if (buffer[0] == '#' && !isFileToSend) {
                isFileToSend = true;
                fileName = buffer + 1;
            }
        }
    }

    void recvFile(int clientNumber) {
        // 回传ark作为确认
        ssize_t sendLength = strlen("#ack");
        ssize_t realSendLength = send(peerSocket[clientNumber], "#ack", sendLength, 0);
        if (sendLength != realSendLength) {
            perror("Send error\n");
            cout << "Fail to receive file: " << fileName << " from server: " << ipAddress[clientNumber] << endl;
            return;
        }
        // 接收文件名
        fileName = "recv" + fileName;
        ofstream outfile(fileName, ios::binary);
        if (!outfile.is_open()) {
            cout << "File: " << fileName << " can not open to write\n";
            return;
        }
        ssize_t recvLength = 0;
        while ((recvLength = recv(peerSocket[clientNumber], buffer, bufferSize - 1, 0)) > 0) {
            buffer[recvLength] = '\0';
            if (!strcmp(buffer, "#end"))
                break;
            outfile.write(buffer, recvLength);
        }
        cout << "Successfully received file: " << fileName << " from server: " << ipAddress[clientNumber] << endl; 
    }

    void sendFile(int clientNumber) {
        ifstream infile(fileName, ios::binary);
        if (!infile.is_open()) {
            cout << "File: " << fileName << " is not found or can not open to read\n";
            return;
        }
        ssize_t sendLength = 0;
        ssize_t realSendLength = 0;
        while (!infile.eof()) {
            infile.read(buffer, bufferSize - 1);
            sendLength = infile.gcount();
            buffer[sendLength] = '\0';
            realSendLength = send(peerSocket[clientNumber], buffer, sendLength, 0);
            if (sendLength != realSendLength) {
                perror("Send error\n");
                return;
            }
        }
        cout << "Input \"#end\" to complete.\n";
    }

    /*
    void sendVideo(int clientNumber) {
        cv::VideoCapture capture(0);
        capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480); 
        cv::Mat image;
        while (true) {
            if (!capture.isOpened())
                return;
            capture >> image;
            if (image.empty())
                return;
            transmit(image, clientNumber);
        }
    }
    */

    void recvVideo(int clientNumber) {
        cv::Mat image;
        while (receive(image, clientNumber)) {
            cv::imshow("server", image);
            cv::waitKey(30);
        }
        // cv::destroyAllWindows(); -- 似乎无法关闭窗口
    }

    /*
    bool transmit(cv::Mat image, int clientNumber) {
        if (image.empty()) {
            cout << "Empty image\n";
            return false;
        }
        if (image.cols != ImageWidth || image.rows != ImageHeight || image.type() != CV_8UC3) {
            cout << "The image have to satisfy: cols = " << ImageWidth << ", rows = " << ImageHeight << ", type = " << CV_8UC3 << endl;
            return false;
        }
        // ? 似乎是区分传送方式
        for (int k = 0; k < NumberOfPackage; k++) {
            int num1 = ImageHeight / NumberOfPackage * k;
            for (int i = 0; i < ImageHeight / NumberOfPackage; i++) {
                int num2 = i * ImageHeight * 3;
                uchar *ucdata = image.ptr<uchar>(i + num1);
                for (int j = 0; j < ImageHeight * 3; j++)
                    vBuffer.buffer[num2 + j] = ucdata[j];
            }
    
            if (k == NumberOfPackage - 1)
                vBuffer.tag =  2;
            else
                vBuffer.tag = 1;
    
            if (send(peerSocket[clientNumber], (char *)(&vBuffer), sizeof(vBuffer), 0) < 0) {
                cout << "Send image error: " << strerror(errno) << "(errno: " << errno << ")\n";
                return false;
            }
        }
        return true;
    }
    */

    bool receive(cv::Mat &image, int clientNumber) {
        int returnTag = false;
        cv::Mat img(ImageHeight, ImageWidth, CV_8UC3, cv::Scalar(0));
        ssize_t needRecvLength = sizeof(videoBuffer);
        ssize_t recvLength = 0;
        int count = 0;
        memset(&vBuffer, 0, sizeof(vBuffer));
    
        for (int i = 0; i < NumberOfPackage; i++) {
            int pos = 0;                
            while (pos < needRecvLength) {
                recvLength = recv(peerSocket[clientNumber], (char*)(&vBuffer) + pos, needRecvLength - pos, 0);
                if (recvLength < 0) {
                    cout << "Receive image failed.\n";
                    break;
                }
                if (recvLength == 0)
                    return false;
                pos += recvLength;
            }
    
            count = count + vBuffer.tag;
    
            int num1 = ImageHeight / NumberOfPackage * i;
            for (int j = 0; j < ImageHeight / NumberOfPackage; j++) {
                int num2 = j * ImageWidth * 3;
                uchar* ucdata = img.ptr<uchar>(j + num1);
                for (int k = 0; k < ImageWidth * 3; k++)
                    ucdata[k] = vBuffer.buffer[num2 + k];
            }
    
            if (vBuffer.tag == 2) {
                if (count == NumberOfPackage + 1) {
                    image = img;
                    returnTag = true;
                    count = 0;
                } else {
                    count = 0;
                    i = 0;
                }
            }
        }
        return returnTag;
    }
};

#endif