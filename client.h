#ifndef _CLIENT_H_
#define _CLIENT_H_
#include <opencv2/opencv.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> // 主机名解析到IP地址需要
#include <string>
#include <thread>
#include <cstdlib>
#include <random>
#include <iostream>
#include <cstdlib>
#include <fstream>
#define NumberOfPackage 32
#define ImageWidth 640
#define ImageHeight 480
#define ImageBufferSize ImageWidth * ImageHeight * 3 / NumberOfPackage
#define bufferSize 100
#define fileNameSize 64
using namespace std;

/*
 * socketId: 客户端的socket
 * serverAddress: 服务器ip地址、端口信息
 * serverIPAddress: 服务器xxx.xxx.xxx.xxx的ip地址
 * buffer: 收发缓冲区
 * fileName: 收发文件名
 * isFileToSend: 当前是否处于发送文件状态
 * clientLock: 锁, 未使用到
 */
struct videoBuffer {
    char buffer[ImageBufferSize];
    int tag;
};

class clientConnection {
private:
    int socketId;
    socklen_t lengthOfSocketAddress;
    const sockaddr *serverAddress;
    string serverIPAddress;
    char *buffer;
    videoBuffer vBuffer;
    string fileName;
    bool isFileToSend;
    bool isOnVideo;
    mutex clientLock;
public:
    clientConnection(int socketId, sockaddr_in *addressInfo, socklen_t length) {
        this->socketId = socketId;
        lengthOfSocketAddress = length;
        serverAddress = (sockaddr *)addressInfo;
        buffer = new char[bufferSize];
        buffer[0] = '\0';
        isFileToSend = false;
        isOnVideo = false;
        // connect: SYN J
        openSocket();
        serverIPAddress = inet_ntoa(addressInfo->sin_addr);
        cout << "Connect success: " << serverIPAddress << endl;
    }
    
    ~clientConnection() {
        delete[] buffer;
    }

    int start() {
        // 开始函数, 传文件/信息返回1, 传视频返回0
        // 正常发信息
        do {
            cout << "传信息/文件输入\"#start\", 传视频输入\"$video\"\n";
            cin.getline(buffer, bufferSize);
            if (!strcmp(buffer, "exit")) {
                // 退出程序
                closeSocket();
                exit(1);
            }
        }
        while (strcmp(buffer, "$video") && strcmp(buffer, "#start"));
        ssize_t sendLength = strlen(buffer);
        ssize_t realSendLength = send(socketId, buffer, sendLength, 0);
        if (sendLength != realSendLength) {
            perror("Send error\n");
            return -1;
        }
        if (!strcmp(buffer, "$video")) 
            return 0;
        if (!strcmp(buffer, "#start"))
            return 1;
        return 1;
    }

    void runThread() {
        // 启动线程, 同时收/发
        thread clientSendThread(&clientConnection::recvThread, ref(*this));
        thread clientRecvThread(&clientConnection::sendThread, ref(*this));
        clientSendThread.join();
        clientRecvThread.join();
    }
    
    void recvThread() {
        // 接收线程
        while (true) {
            ssize_t recvLength;
            recvLength = recv(socketId, buffer, bufferSize - 1, 0);
            // == 0: socket已经断开
            if (recvLength == 0)
                return;
            // < 0: 发生错误
            if (recvLength < 0) {
                perror("Receive data fail\n");
                return;
            }
            // 正常运行, 输出收到的信息
            buffer[recvLength] = '\0';
            cout << serverIPAddress << ": " << buffer << endl;
            // 发文件时收到接收方传来的确认, "#ack", 传文件
            if (!strcmp(buffer, "#ack") && isFileToSend) {
                sendFile();
                continue;
            }
            // 收文件时收到#开头的信息, 获取文件名, 开始接收文件
            if (buffer[0] == '#') {
                fileName = buffer + 1;
                recvFile();
                continue;
            }
        }
    }

    void sendThread() {
        // 发送线程
        while (true) {
            // 读取输入
            cin.getline(buffer, bufferSize);
            if (!strcmp(buffer, "exit")) {
                // 退出thread
                closeSocket();
                return;
            }
            // 正常发信息
            ssize_t sendLength = strlen(buffer);
            ssize_t realSendLength = send(socketId, buffer, sendLength, 0);
            if (sendLength != realSendLength) {
                perror("Send error\n");
                return;
            }
            // 输入信息为#end, 标识发送文件结束
            if (!strcmp(buffer, "#end") && isFileToSend) {
                cout << "Successfully sent file: " << fileName << " to server: " << serverIPAddress << endl;
                isFileToSend = false;
                continue;
                // 继续运行
            }
            // 输入信息为#+文件名, 文件传输开始, 实际过程位于recvThread
            // 此时recvThread正在运行, recvThread会收到#ack, 然后真正开始文件传输
            if (buffer[0] == '#' && !isFileToSend) {
                // 设置判断, 获取文件名
                isFileToSend = true;
                fileName = buffer + 1;
            }
        }
    }

    void recvFile() {
        // 实现真正的文件接收过程
        // 回传#ack作为确认
        ssize_t sendLength = strlen("#ack");
        ssize_t realSendLength = send(socketId, "#ack", sendLength, 0);
        if (sendLength != realSendLength) {
            perror("Send error\n");
            cout << "Fail to receive file: " << fileName << " from server: " << serverAddress << endl;
            return;
        }
        fileName = "recv" + fileName;
        ofstream outfile(fileName, ios::binary);
        if (!outfile.is_open()) {
            cout << "File: " << fileName << " can not open to write\n";
            return;
        }
        ssize_t recvLength = 0;
        while ((recvLength = recv(socketId, buffer, bufferSize - 1, 0)) > 0) {
            buffer[recvLength] = '\0';
            // 发送方在文件传输结束后发送一个#end, 标志结束, 接收方收到, 退出接收过程
            if (!strcmp(buffer, "#end"))
                break;
            outfile.write(buffer, recvLength);
        }
        cout << "Successfully received file: " << fileName << " from server: " << serverIPAddress << endl; 
    }
    
    void sendFile() {
        // 文件发送过程
        ifstream infile(fileName, ios::binary);
        if (!infile.is_open()) {
            cout << "File: " << fileName << " is not found or can not open to read\n";
            return;
        }
        ssize_t sendLength = 0;
        ssize_t realSendLength = 0;
        // 循环, 直到文件内容全部发送
        while (!infile.eof()) {
            infile.read(buffer, bufferSize - 1);
            sendLength = infile.gcount();
            buffer[sendLength] = '\0';
            realSendLength = send(socketId, buffer, sendLength, 0);
            if (sendLength != realSendLength) {
                perror("Send error\n");
                return;
            }
        }
        cout << "Input \"#end\" to complete.\n";
    }

    void sendVideo() {
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
            
            cv::imshow("client", image);
            cv::waitKey(30);
            transmit(image);
        }
    }

    /*
    void recvVideo() {
        cv::Mat image;
        while (true) {
            if (receive(image)) {
                cv::imshow("client", image);
                cv::waitKey(0);
            }
        }
    }
    */

    bool transmit(cv::Mat image) {
        if (image.empty()) {
            cout << "Empty image\n";
            return false;
        }
        if (image.cols != ImageWidth || image.rows != ImageHeight || image.type() != CV_8UC3) {
            cout << image.cols << ' ' << image.rows << ' ' << image.type() << endl;
            cout << "The image have to satisfy: cols = " << ImageWidth << ", rows = " << ImageHeight << ", type = " << CV_8UC3 << endl;
            return false;
        }

        for(int k = 0; k < NumberOfPackage; k++) {
            int num1 = ImageHeight / NumberOfPackage * k;
            for (int i = 0; i < ImageHeight / NumberOfPackage; i++) {
                int num2 = i * ImageWidth * 3;
                uchar *ucdata = image.ptr<uchar>(i + num1);
                for (int j = 0; j < ImageWidth * 3; j++)
                    vBuffer.buffer[num2 + j] = ucdata[j];
            }
    
            if(k == NumberOfPackage - 1)
                vBuffer.tag =  2;
            else
                vBuffer.tag = 1;
    
            if (send(socketId, (char *)(&vBuffer), sizeof(vBuffer), 0) < 0) {
                cout << "Send image error: " << strerror(errno) << "(errno: " << errno << ")\n";
                return false;
            }
        }
        return true;
    }
    
    /*
    bool receive(cv::Mat &image) {
        int returnTag = false;
        cv::Mat img(ImageHeight, ImageWidth, CV_8UC3, cv::Scalar(0));
        ssize_t needRecvLength = sizeof(videoBuffer);
        ssize_t recvLength = 0;
        int count = 0;
        memset(&vBuffer, 0, sizeof(vBuffer));
    
        for (int i = 0; i < NumberOfPackage; i++) {  
            int pos = 0;              
            while (pos < needRecvLength) {
                recvLength = recv(socketId, (char*)(&vBuffer) + pos, needRecvLength - pos, 0);
                if (recvLength < 0) {
                    cout << "Receive image failed.\n";
                    break;
                }
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
    */

    void closeSocket() {
        if (-1 != socketId && 0 != shutdown(socketId, SHUT_RDWR)) {
            cout << "Shutdown client socket fail" << endl;
            perror("Error info ");
        }
        if (-1 != socketId)
            close(socketId);
    }

    void openSocket() {
        if (0 != connect(socketId, (sockaddr *)serverAddress, lengthOfSocketAddress)) {
            delete serverAddress;
            perror("Connect Error ");
            throw "Connect fail!";
        }
    }
};

#endif