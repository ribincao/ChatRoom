#include <iostream>
#include "Client.h"

 
Client::Client(){
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    
    sock = 0;
    pid = 0;
    isClientwork = true;
    epfd = 0;
}

void Client::Connect() {
    std::cout << "Connect Server: " << SERVER_IP << " : " << SERVER_PORT << std::endl;
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        std::cerr << "socket" << std::endl;
        exit(-1); 
    }

    if(connect(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        std::cerr << "connect" << std::endl;
        exit(-1);
    }

    if(pipe(pipe_fd) < 0) {
        std::cerr << "pipe" << std::endl;
        exit(-1);
    }
 
    epfd = epoll_create(EPOLL_SIZE);
    
    if(epfd < 0) {
        std::cerr << "epoll" << std::endl;
        exit(-1); 
    }

    addfd(epfd, sock, true);
    addfd(epfd, pipe_fd[0], true);
 
}

void Client::Close() {
 
    if(pid){
        close(pipe_fd[0]);
        close(sock);
    }else{
        close(pipe_fd[1]);
    }
}

void Client::Run() {
 
    static struct epoll_event events[2];
    Connect();
    pid = fork();
    if(pid < 0) {
        std::cerr << "fork" << std::endl;
        close(sock);
        exit(-1);
    } else if(pid == 0) {
        close(pipe_fd[0]); 
        std::cout << "Please input '@exit' to exit the chat room" << std::endl;
        std::cout << "@ + ClientID to private chat. " << std::endl;
        while(isClientwork){
            memset(msg.content, 0, sizeof(msg.content));
            fgets(msg.content, BUF_SIZE, stdin);
            if(strncasecmp(msg.content, EXIT, strlen(EXIT)) == 0){
                isClientwork = 0;
            }
            else {
                memset(send_buf, 0, BUF_SIZE);
                memcpy(send_buf, &msg, sizeof(msg));
                if(write(pipe_fd[1], send_buf, sizeof(send_buf)) < 0 ) { 
                    std::cerr << "fork error" << std::endl;
                    exit(-1);
                }
            }
        }
    } else { 
        close(pipe_fd[1]); 
        while(isClientwork) {
            int epoll_events_count = epoll_wait(epfd, events, 2, -1 );
            for(int i = 0; i < epoll_events_count ; ++i)
            {
                memset(recv_buf, 0, sizeof(recv_buf));
                if(events[i].data.fd == sock)
                {
                    int ret = recv(sock, recv_buf, BUF_SIZE, 0);
                    memset(&msg, 0, sizeof(msg));
                    memcpy(&msg, recv_buf, sizeof(msg));
                    std::cout << msg.content << std::endl;
                    if(ret == 0) {
                        std::cout << "Server closed connection: " << std::endl;
                        close(sock);
                        isClientwork = 0;
                    } else {
                        std::cout << msg.content << std::endl;
                    }
                }
                else { 
                    int ret = read(events[i].data.fd, recv_buf, BUF_SIZE);
                    if(ret == 0)
                        isClientwork = 0;
                    else {
                        send(sock, recv_buf, sizeof(recv_buf), 0);
                    }
                }
            }
        }
    }
    Close();
}