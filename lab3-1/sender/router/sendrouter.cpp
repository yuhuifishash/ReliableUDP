#include "sendrouter.h"
#include "../reliableudpsegment.h"
#include <iostream>
extern int UDPSender;
extern sockaddr_in addrReceiver;
extern pthread_mutex_t send_mutex;

static int LossCount = 0;
static int now_count = 0;
static int DelayTime = 100;//ms
std::map<SendPacket*,int> SendQueue;
SendPacket* p;
void SendToReceiver()
{
    std::set<SendPacket*> erase_packet;
    pthread_mutex_lock(&send_mutex);
    for(auto &[sp,t]:SendQueue){
        t -= 10;
        if(t <= 0){
            erase_packet.insert(sp);
            sendto(sp->__fd,(char*)&(sp->__buf),sp->__n,sp->__flags,sp->__addr,sp->__addr_len);
        }
    }
    for(auto sp:erase_packet){
        SendQueue.erase(sp);
        delete sp;
    }
    pthread_mutex_unlock(&send_mutex);
}

void* SendThread(void*)
{
    while(true){
        usleep(10000);
        SendToReceiver();
    }
}

void SendToRouter(int __fd, ReliableUDPSegment __buf, size_t __n,int __flags, __CONST_SOCKADDR_ARG __addr,socklen_t __addr_len)
{
    ++now_count;
    if(now_count == LossCount){//drop this packet
        now_count = 0;
        return;
    }
    SendPacket* s = new SendPacket;
    s->__fd = __fd;
    s->__buf = __buf;
    s->__n = __n;
    s->__flags = __flags;
    s->__addr = __addr;
    s->__addr_len = __addr_len;

    //sendto(__fd,__buf,__n,__flags,__addr,__addr_len);
    pthread_mutex_lock(&send_mutex);
    SendQueue[s] = DelayTime;
    pthread_mutex_unlock(&send_mutex);
}