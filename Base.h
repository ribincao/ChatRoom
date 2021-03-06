#ifndef IM_BASE_H
#define IM_BASE_H

#include <iostream>
#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4000
#define EPOLL_SIZE 5000
#define BUF_SIZE 0xFFFF
#define WELCOME "Welcome to chat room. Your id is: #%d"
#define MESSAGE "#%d: %s"
#define PRIVATE_MESSAGE "#%d say to you: %s"
#define PRIVATE_ERROR "#%d is not in chat room yet."
#define EXIT "@EXIT"
#define CAUTION "Only one in the chat room."
#define ET true

struct Msg
{
    int type;
    int from;
    int to;
    char content[BUF_SIZE];
};

static void addfd(int epollfd, int fd, bool et_flag)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if(et_flag) ev.events |= EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
    // std::cout << "fd added to epoll" << std::endl;
}
#endif