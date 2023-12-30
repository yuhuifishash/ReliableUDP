#include "reliableudpconnect.h"
#include "reliableudpsegment.h"
#include "bytestream.h"
#include "router/sendrouter.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <map>
#include <chrono>

int RetransTime = 500;

std::map<int64_t,ReliableUDPSegment> WaitingSegmentid;
std::map<ReliableUDPSegment,std::pair<int,int> > WaitingAckSegment;
extern int UDPSender;
extern sockaddr_in addrReceiver;
extern ReliableSenderUDPStatus status_now;
extern ByteStream buf;
extern pthread_mutex_t mutex;

int64_t now_seqno = 0;
int64_t end_seqno = -1;

double avg_delay = 0;
double max_delay = 0;
double min_delay = 1e9;
int cnt = 0;

void* timeevent_thread(void*)
{
    while(true){
        usleep(50000);
        tickevent();
    }
}

void tickevent()
{
    pthread_mutex_lock(&mutex);

    for(auto &[Seg,timer]:WaitingAckSegment){
        timer.second -= 50;
        if(timer.second < 0){
            timer.first *= 2;
            timer.second = timer.first;
            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

            auto Seg_info = Seg;
            printf("timeout,resend %s to Receiver\n",GetSegInfo(Seg_info).c_str());
        }
    }
    pthread_mutex_unlock(&mutex);

}
/*
 * Acker 接收接收端发送的ACK等信息
 * addrSender  发送端的端口号和ip地址等信息
*/
void ReliableUDPConnect(int Acker,sockaddr_in addrSender)
{
    
    pthread_t timeevent_t;
    pthread_create(&timeevent_t,NULL,timeevent_thread,NULL);
    while(true){
        if(status_now == CLOSED){
            Send_CLOSED(Acker,addrSender);
        }
        if(status_now == SYN_SENT){
            Send_SYNSENT(Acker,addrSender);
        }
        if(status_now == ESTABLISHED){
            Send_ESTABLISHED(Acker,addrSender);
        }
        if(status_now == FIN_WAIT){
            Send_FINWAIT(Acker,addrSender);
        }
        if(status_now == TIME_WAIT){
            Send_TIMEWAIT(Acker,addrSender);
        }
        if(status_now == SEND_CLOSED){
            return;
        }
    }
}

void Send_CLOSED(int Acker,sockaddr_in addrSender)
{
    ReliableUDPSegment Seg = CreateEmptySeg(0b10000000,0);//SYN
    Seg.SrcPort = addrSender.sin_port;;
    Seg.DestPort = addrReceiver.sin_port;
    CalculatedCheckSum(Seg);

    SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

    int64_t id = Seg.seqno;

    pthread_mutex_lock(&mutex);
    WaitingSegmentid[id] = Seg;
    WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
    std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
    pthread_mutex_unlock(&mutex);

    status_now = SYN_SENT;

}
void Send_SYNSENT(int Acker,sockaddr_in addrSender)
{
    sockaddr_in addrAckSender;
    socklen_t addrAckSenderlen = sizeof(addrAckSender);
    char recv_buf[1500]={};

    recvfrom(Acker,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrAckSender,&addrAckSenderlen);

    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}
    int64_t id = Seg.seqno;

    pthread_mutex_lock(&mutex);
    ReliableUDPSegment recent_Seg = WaitingSegmentid[id];
    WaitingAckSegment.erase(recent_Seg);
    WaitingSegmentid.erase(id);
    pthread_mutex_unlock(&mutex);

    if(SYN(Seg) && ACK(Seg)){
        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";
    }
    else{
        std::cout<<"Unexpected Data Received\n";
        return;
    }

    Seg = CreateDataSeg(0b00110000,0,buf.pop_some_byte(1024));//ACK NAME
    Seg.SrcPort = addrSender.sin_port;;
    Seg.DestPort = addrReceiver.sin_port;
    CalculatedCheckSum(Seg);

    SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

    id = Seg.seqno;
    pthread_mutex_lock(&mutex);
    WaitingSegmentid[id] = Seg;
    WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
    pthread_mutex_unlock(&mutex);

    std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

    status_now = ESTABLISHED;
}
void Send_ESTABLISHED(int Acker,sockaddr_in addrSender)
{
    sockaddr_in addrAckSender;
    socklen_t addrAckSenderlen = sizeof(addrAckSender);
    char recv_buf[1500]={};

    auto start = std::chrono::high_resolution_clock::now();
    auto finish = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = finish - start;

    while(true){
        recvfrom(Acker,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrAckSender,&addrAckSenderlen);
        ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
        if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

        int64_t id = Seg.seqno;
        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";

        if(SYN(Seg)){std::cout<<"Receive SYN in established status. Will resend ACK.\n";continue;}
        if(ACK(Seg)){

            pthread_mutex_lock(&mutex);
            ReliableUDPSegment recent_Seg = WaitingSegmentid[id];
            WaitingAckSegment.erase(recent_Seg);
            WaitingSegmentid.erase(id);
            if(WaitingSegmentid.size()){pthread_mutex_unlock(&mutex);continue;}//停等机制，没有收到接收方对上次发送的数据包的ACK
            pthread_mutex_unlock(&mutex);

        }

        finish = std::chrono::high_resolution_clock::now();
        elapsed = finish - start;
        if(elapsed.count() > 0){
            avg_delay += elapsed.count();
            max_delay = std::max(max_delay,(double)elapsed.count());
            min_delay = std::min(min_delay,(double)elapsed.count());
        }

        if(elapsed.count() > 0 && elapsed.count() < 2*RetransTime){
            RetransTime = std::max(50,(9*RetransTime + (int)(elapsed.count()*1000)+50)/10);
        }
        //std::cout<<"RetransTime is "<<RetransTime<<"\n";


        if(id == end_seqno){//传输完毕,发送FIN信号
            Seg = CreateEmptySeg(0b01000000,0);//FIN
            Seg.SrcPort = addrSender.sin_port;
            Seg.DestPort = addrReceiver.sin_port;
            CalculatedCheckSum(Seg);

            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

            int64_t id = Seg.seqno;

            pthread_mutex_lock(&mutex);
            WaitingSegmentid[id] = Seg;
            WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
            pthread_mutex_unlock(&mutex);

            std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
            status_now = FIN_WAIT;
            break;
        }
        else if(buf.end_tag){//需要传输的数据均已发送完毕，等待对方确认所有数据
            continue;
        }
        else{//发送下一个数据包
            Seg = CreateDataSeg(0b00000000,now_seqno,buf.pop_some_byte(1024));//Data
            Seg.SrcPort = addrSender.sin_port;
            Seg.DestPort = addrReceiver.sin_port;
            CalculatedCheckSum(Seg);

            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

            start = std::chrono::high_resolution_clock::now();
            ++cnt;

            if(buf.end_tag){end_seqno = now_seqno;}
            else{now_seqno += 1024;}

            int64_t id = Seg.seqno;

            pthread_mutex_lock(&mutex);
            WaitingSegmentid[id] = Seg;
            WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
            pthread_mutex_unlock(&mutex);

            std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
        }
    }
}
void Send_FINWAIT(int Acker,sockaddr_in addrSender)
{
    sockaddr_in addrAckSender;
    socklen_t addrAckSenderlen = sizeof(addrAckSender);
    char recv_buf[1500]={};
    recvfrom(Acker,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrAckSender,&addrAckSenderlen);
    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

    int64_t id = Seg.seqno;
    std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";
    if(ACK(Seg) && FIN(Seg)){
        pthread_mutex_lock(&mutex);
        ReliableUDPSegment recent_Seg = WaitingSegmentid[id];
        WaitingAckSegment.erase(recent_Seg);
        WaitingSegmentid.erase(id);
        pthread_mutex_unlock(&mutex);

        Seg = CreateEmptySeg(0b00100000,0);//LAST ACK
        Seg.SrcPort = addrSender.sin_port;
        Seg.DestPort = addrReceiver.sin_port;
        CalculatedCheckSum(Seg);

        SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
        std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

        status_now = TIME_WAIT;
    }

}
void Send_TIMEWAIT(int Acker,sockaddr_in addrSender)
{
    std::cout<<"Send successfully, will close in 2s.\n";
    struct timeval tv;
    tv.tv_sec = 2; //2秒超时
    tv.tv_usec = 0;
    setsockopt(Acker, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

    sockaddr_in addrAckSender;
    socklen_t addrAckSenderlen = sizeof(addrAckSender);
    char recv_buf[1500]={};
    int ret = recvfrom(Acker,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrAckSender,&addrAckSenderlen);
    if(ret < 0){
        std::cout<<"Sender will close\n";
        status_now = SEND_CLOSED;
        return;
    }

    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

    else{
        std::cout<<"Receive data in TIMEWAIT status. Will resend ACK.\n";
        ReliableUDPSegment Seg = CreateEmptySeg(0b00100000,0);//LAST ACK
        Seg.SrcPort = addrSender.sin_port;
        Seg.DestPort = addrReceiver.sin_port;
        CalculatedCheckSum(Seg);

        SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
        std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
    }
}
