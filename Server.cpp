#include <iostream>
#include "Server.h"

Server::Server()
{
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    fd = 0;
    epfd = 0;
}
void Server::Init()
{
    std::cout << "Init Server, Loading ... " << std::endl;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) { 
        std::cerr << "socket" << std::endl;
        exit(-1);
    }

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

    epfd = epoll_create(EPOLL_SIZE);
    if(epfd < 0) {
        std::cerr << "epoll" << std::endl;
        exit(-1);
    }
    addfd(epfd, fd, ET);
}

void Server::Close()
{
    close(fd);
    close(epfd);
}

void Server::Run()
{
    static struct epoll_event events[EPOLL_SIZE];
    Init();
    Msg msg;
    while(true)
    {
        int events_cnt = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if(events_cnt < 0) {
            std::cerr << "epoll failure." << std::endl;
            exit(0);
        }
        // std::cout << "epoll events count: " << events_cnt << std::endl;

        for(int i = 0;i < events_cnt;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == fd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(struct sockaddr_in);
                int clientfd = accept(fd, (struct sockaddr*)&client_address, &client_addrlen);

                std::cout << "Client connection from: "
                          << inet_ntoa(client_address.sin_addr) << ":"
                          << ntohs(client_address.sin_port) << ", Client fd = "
                          << clientfd << std::endl;

                addfd(epfd, clientfd, ET);

                client_list.push_back(clientfd);
                std::cout << "Add new friend #" << clientfd << " to epoll" << std::endl;
                std::cout << "Now there are " << client_list.size() << " friends in the chat room." << std::endl;

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
    char recv_buf[BUF_SIZE];
    char send_buf[BUF_SIZE];

    Msg msg;
    bzero(recv_buf, BUF_SIZE);
    std::cout << "Read From Client #" << clientfd << std::endl;
    int len = recv(clientfd, recv_buf, BUF_SIZE, 0);
    
    memset(&msg, 0, sizeof(msg));
    memcpy(&msg, recv_buf, sizeof(msg));

    msg.from = clientfd;
    if(msg.content[0] == '@' && isdigit(msg.content[1])) {
        msg.type = true;
        msg.to = msg.content[1] - '0';
        memcpy(msg.content, msg.content + 2, sizeof(msg.content));
    } else {
        msg.type = false; 
    }

    if(len == 0) {
        close(clientfd);
        client_list.remove(clientfd);
        std::cout << "Client #" << clientfd
                  << " leave the chat room, now there are "
                  << client_list.size()
                  << " friends in the chat room."
                  << std::endl;
    } else {
        if(client_list.size() == 1) {
            memcpy(&msg.content, CAUTION, sizeof(msg.content));
            bzero(send_buf, BUF_SIZE);
            memcpy(send_buf, &msg, sizeof(msg));
            send(clientfd, send_buf, sizeof(send_buf), 0);
        }
        char format_message[BUF_SIZE];
        if(msg.type == 0) {
            sprintf(format_message, MESSAGE, clientfd, msg.content);
            memcpy(msg.content, format_message, BUF_SIZE);
            list<int>::iterator it;
            for(it = client_list.begin(); it != client_list.end();++it) {
                if(*it != clientfd) {
                    bzero(send_buf, BUF_SIZE);
                    memcpy(send_buf, &msg, sizeof(msg));
                    if(send(*it, send_buf, sizeof(send_buf), 0) < 0) {
                        return -1;
                    }
                }
            }
        } else if (msg.type == 1) {
            bool type_offline=true;
            sprintf(format_message, PRIVATE_MESSAGE, clientfd, msg.content);
            memcpy(msg.content, format_message, BUF_SIZE);
            // 遍历客户端列表依次发送消息，需要判断不要给来源客户端发
            list<int>::iterator it;
            for(it = client_list.begin(); it != client_list.end(); ++it) {
               if(*it == msg.to){
                    type_offline=false;
                    //把发送的结构体转换为字符串
                    bzero(send_buf, BUF_SIZE);
                    memcpy(send_buf,&msg,sizeof(msg));
                    if(send(*it,send_buf, sizeof(send_buf), 0) < 0 ) {
                        return -1;
                    }
               }
            }
            if(type_offline){
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
