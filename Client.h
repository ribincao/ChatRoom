#ifndef IM_CLIENT_H
#define IM_CLIENT_H
 
#include "Base.h"
#include <string>
 
class Client
{
public:
    Client();
    void Connect();
    void Close();
    void Run();
private:
    int sock;
    int pid;
    int epfd;
    int pipe_fd[2];
    bool isClientwork;
    Msg msg;
    char send_buf[BUF_SIZE];
    char recv_buf[BUF_SIZE];
    struct sockaddr_in srv_addr;
};
#endif