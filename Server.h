#ifndef IM_SERVER_H
#define IM_SERVER_H

#include <string>
#include "Base.h"
using namespace std;

class Server
{
public:
    Server();
    void Init();
    void Close();
    void Run();
    int SendBroadcastMessage(int);
private:
    struct sockaddr_in srv_addr;
    int fd;
    int epfd;
    list<int> client_list;
};
#endif