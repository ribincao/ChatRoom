#include <iostream>
#include "Server.h"

Server::Server()
{
    //  设置服务 IP 和 PORT
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    fd = 0;
    epfd = 0;
}
void Server::Init()
{
    //  服务初始化
    std::cout << "Init Server, Loading ... " << std::endl;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { 
        std::cerr << "socket" << std::endl;
        exit(-1);
    }

    //  设置端口复用
    int optval = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if(bind(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        std::cerr << "bind" << std::endl;
        exit(-1);
    }

    if(listen(fd, 5) < 0) {
        std::cerr << "listen" << std::endl;
        exit(-1);
    }

    cout << "Chat Room Create Success." << endl;

    //  创建 epollfd
    epfd = epoll_create(EPOLL_SIZE);
    if(epfd < 0) {
        std::cerr << "epoll" << std::endl;
        exit(-1);
    }
    //  把监听 socket 添加到 epollfd 中
    addfd(epfd, fd, ET);
}

void Server::Close()
{
    //  关闭文件描述符
    close(fd);
    //  关闭 epoll 描述符
    close(epfd);
}

void Server::Run()
{
    //  启动服务
    static struct epoll_event events[EPOLL_SIZE];
    Init();
    Msg msg;
    while(true)
    {
        //  监听描述符事件, 返回活跃事件的个数
        int events_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if(events_cnt < 0) {
            std::cerr << "epoll failure." << std::endl;
            exit(0);
        }
        // std::cout << "epoll events count: " << events_cnt << std::endl;

        //  遍历活跃事件
        for(int i = 0;i < events_cnt;i++)
        {
            //  获取文件描述符
            int sockfd = events[i].data.fd;
            if(sockfd == fd)
            {
                //  如果活跃事件描述符为监听 socket 的描述符就说明有客户建立连接
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(struct sockaddr_in);
                int clientfd = accept(fd, (struct sockaddr*)&client_address, &client_addrlen);

                std::cout << "Client connection from: "
                          << inet_ntoa(client_address.sin_addr) << ":"
                          << ntohs(client_address.sin_port) << ", Client fd = "
                          << clientfd << std::endl;

                //  把新建立连接的 socket 描述符添加到 epllfd 中
                addfd(epfd, clientfd, ET);

                //  将新建立连接的 socket 描述符添加到客户端链表里方便统计连接数
                client_list.push_back(clientfd);
                std::cout << "Add new friend #" << clientfd << " to epoll" << std::endl;
                std::cout << "Now there are " << client_list.size() << " friends in the chat room." << std::endl;

                //  发送欢迎消息
                char message[BUF_SIZE];
                sprintf(msg.content, WELCOME, clientfd);
                bzero(message, BUF_SIZE);
                memcpy(message, &msg, sizeof(msg));
                int ret = send(clientfd, message, sizeof(message), 0);
                if(ret < 0) {
                    std::cerr << "send" << std::endl;
                    Close();
                    exit(0);
                }
            } else {
                //  如果活跃事件描述符不是监听 socket 的描述符就说明聊天室有人发消息
                int ret = SendBroadcastMessage(sockfd);
                if(ret < 0) {
                    std::cerr << "Broadcast" << std::endl;
                    Close();
                    exit(-1);
                }
            }
        }
    }
    Close();
}

int Server::SendBroadcastMessage(int clientfd)
{
    //  发送广播消息
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];

    //  初始化一个消息体
    Msg msg;
    bzero(recv_buf, BUF_SIZE);
    std::cout << "Read From Client #" << clientfd << std::endl;
    //  把客户发来的数据存储到接收缓冲区 recv_buf
    int len = recv(clientfd, recv_buf, BUF_SIZE, 0);
    
    //  把接收缓冲区 recv_buf 的数据拷贝至消息体(结构化)
    memset(&msg, 0, sizeof(msg));
    memcpy(&msg, recv_buf, sizeof(msg));

    //  读取发送者的信息判断是否私聊信息
    msg.from = clientfd;
    if(msg.content[0] == '@' && isdigit(msg.content[1])) {
        //  私聊信息
        msg.type = 1;
        msg.to = msg.content[1] - '0';
        memcpy(msg.content, msg.content + 2, sizeof(msg.content));
    } else {
        //  群聊信息
        msg.type = 0; 
    }

    if(len == 0) {
        //  读取到数据为空则关闭客户端连接
        close(clientfd);
        client_list.remove(clientfd);
        std::cout << "Client #" << clientfd
                  << " leave the chat room, now there are "
                  << client_list.size()
                  << " friends in the chat room."
                  << std::endl;
    } else {
        //  判断聊天室是否只有一个人, 如果只有一个人就发出警告
        if(client_list.size() == 1) {
            memcpy(&msg.content, SINGLE, sizeof(msg.content));
            bzero(send_buf, BUF_SIZE);
            memcpy(send_buf, &msg, sizeof(msg));
            send(clientfd, send_buf, sizeof(send_buf), 0);
        }
        //  定义消息格式
        char format_message[BUF_SIZE];
        if(msg.type == 0) {
            //  发送群聊消息
            sprintf(format_message, MESSAGE, clientfd, msg.content);
            memcpy(msg.content, format_message, BUF_SIZE);
            list<int>::iterator it;
            for(it = client_list.begin(); it != client_list.end();++it) {
                //  遍历聊天室的用户, 把消息发给其他人
                if(*it != clientfd) {
                    bzero(send_buf, BUF_SIZE);
                    memcpy(send_buf, &msg, sizeof(msg));
                    if(send(*it, send_buf, sizeof(send_buf), 0) < 0) {
                        return -1;
                    }
                }
            }
        } else if (msg.type == 1) {
            //  发送私聊消息, 默认接收消息的用户不在线
            bool type_offline = true;
            sprintf(format_message, PRIVATE_MESSAGE, clientfd, msg.content);
            memcpy(msg.content, format_message, BUF_SIZE);
            // 遍历客户端列表依次发送消息，需要判断不要给来源客户端发
            list<int>::iterator it;
            for(it = client_list.begin(); it != client_list.end(); ++it) {
               if(*it == msg.to){
                    //   私聊对象在线
                    type_offline = false;
                    bzero(send_buf, BUF_SIZE);
                    memcpy(send_buf,&msg,sizeof(msg));
                    if(send(*it,send_buf, sizeof(send_buf), 0) < 0 ) {
                        return -1;
                    }
               }
            }
            if(type_offline){
                //   私聊对象离线, 发送错误消息给发送者
                sprintf(format_message, PRIVATE_ERROR, msg.to);
                memcpy(msg.content, format_message, BUF_SIZE);
                bzero(send_buf, BUF_SIZE);
                memcpy(send_buf, &msg, sizeof(msg));
                if(send(msg.from, send_buf, sizeof(send_buf), 0)<0)
                    return -1;
            }
        }
    }
    return len;
}
