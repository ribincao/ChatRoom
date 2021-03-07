#include <iostream>
#include "Client.h"

 
Client::Client(){
    //  设置服务 IP 和 PORT
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
 
    //  创建 epollfd
    epfd = epoll_create(EPOLL_SIZE);
    
    if(epfd < 0) {
        std::cerr << "epoll" << std::endl;
        exit(-1); 
    }

    //  把 socketfd 添加至 epollfd
    addfd(epfd, sock, true);
    //  把父进程的读管道描述符添加至 epollfd
    addfd(epfd, pipe_fd[0], true);
 
}

void Client::Close() {
 
    if(pid){
        //  关闭父进程的读管道描述符
        close(pipe_fd[0]);
        //  关闭套接字描述符
        close(sock);
    }else{
        //  关闭子进程的写管道描述符
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
        //  子进程负责往管道写数据, 关闭子进程的管道读描述符
        close(pipe_fd[0]); 
        std::cout << "Please input '@exit' to exit the chat room" << std::endl;
        std::cout << "@ + ClientID to private chat. " << std::endl;
        //  用户在线的情况
        while(isClientwork){
            //  读取标准输入写到消息体里
            memset(msg.content, 0, sizeof(msg.content));
            fgets(msg.content, BUF_SIZE, stdin);
            //  判断用户是否要离开群聊室
            if(strncasecmp(msg.content, EXIT, strlen(EXIT)) == 0){
                //  用户离开聊天室
                isClientwork = 0;
            }
            else {
                //  将消息拷贝到发送缓冲区
                memset(send_buf, 0, BUF_SIZE);
                memcpy(send_buf, &msg, sizeof(msg));
                if(write(pipe_fd[1], send_buf, sizeof(send_buf)) < 0 ) { 
                    std::cerr << "fork error" << std::endl;
                    exit(-1);
                }
            }
        }
    } else { 
        //  父进程负责读服务端返回的数据, 关闭父进程的管道写描述符
        close(pipe_fd[1]);
        while(isClientwork) {
            //  返回触发事件的描述符个数
            int epoll_events_count = epoll_wait(epfd, events, 2, -1);
            //  遍历事件描述符
            for(int i = 0; i < epoll_events_count ; ++i)
            {
                memset(recv_buf, 0, sizeof(recv_buf));
                //  如果是连接套接字返回的数据就拷贝到接收缓冲区
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
                    //  如果是其他描述符事件, 这里是子进程的写管道描述符事件, 就将子进程写入的数据发送给服务端
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