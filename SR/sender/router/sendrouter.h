#ifndef SENDROUTER_H
#define SENDROUTER_H
#include <vector>    
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <set>
#include "../reliableudpsegment.h"

struct SendPacket
{
    int seqno;
    int __fd;
    ReliableUDPSegment __buf;
    size_t __n;
    int __flags;
    __CONST_SOCKADDR_ARG __addr;
    socklen_t __addr_len;
};
void SendToRouter(int __fd, ReliableUDPSegment __buf, size_t __n,int __flags, __CONST_SOCKADDR_ARG __addr,socklen_t __addr_len);
void* SendThread(void*);
#endif