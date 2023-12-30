#include "reliableudpaccept.h"
#include "reliableudpsegment.h"
#include "reassembler.h"
#include <iostream>
#include <unistd.h>
#include <map>
#include <pthread.h>

#define RetransTime 500

std::map<int64_t,ReliableUDPSegment> WaitingSegmentid;
std::map<ReliableUDPSegment,std::pair<int,int> > WaitingAckSegment;
extern int UDPSender;
extern sockaddr_in addrSender;
extern ReliableReceiverUDPStatus status_now;
extern Reassembler recv_reassembler;
extern pthread_mutex_t mutex;

uint16_t Receiver_windowsize = 12*1024;
int64_t now_ackno = 0;
int64_t max_recv_seqno = 0;

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
            sendto(UDPSender,(char*)&Seg,sizeof(Seg),0,(sockaddr*)&addrSender,sizeof(addrSender));
            
            auto Seg_info = Seg;
            printf("timeout,resend %s to Receiver\n",GetSegInfo(Seg_info).c_str());
        }
    }
    pthread_mutex_unlock(&mutex);
}

void ReliableUDPAccept(int UDPReceiver)
{
    pthread_t timeevent_t;
    pthread_create(&timeevent_t,NULL,timeevent_thread,NULL);
    while(true){
        if(status_now == LISTEN){
            Receive_LISTEN(UDPReceiver);
        }
        if(status_now == SYN_RCVD){
            Receive_SYN_RCVD(UDPReceiver);
        }
        if(status_now == ESTABLISHED){
            Receive_ESTABLISHED(UDPReceiver);
        }
        if(status_now == CLOSE_WAIT){
            Receive_CLOSE_WAIT(UDPReceiver);
        }
        if(status_now == CLOSED){
            Receive_CLOSED(UDPReceiver);
            break;
        }
    }
}

void Receive_LISTEN(int UDPReceiver)
{
    socklen_t addrSenderlen = sizeof(addrSender);
    char recv_buf[1500]={};
    recvfrom(UDPReceiver,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrSender,&addrSenderlen);

    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

    addrSender.sin_family = AF_INET;
    addrSender.sin_port = Seg.SrcPort;

    if(SYN(Seg)){
        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";
    }
    else{
        std::cout<<"unexpected Data Received\n";
        return;
    }

    UDPSender = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    Seg = CreateEmptySeg(0b10100000,0); //SYN ACK
    Seg.DestPort = addrSender.sin_port;
    Seg.SrcPort = htons(9961);
    CalculatedCheckSum(Seg);

    sendto(UDPSender,(char*)&Seg,sizeof(Seg),0,(sockaddr*)&addrSender,sizeof(addrSender));

    pthread_mutex_lock(&mutex);
    int64_t id = Seg.seqno;
    WaitingSegmentid[id] = Seg;
    WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
    pthread_mutex_unlock(&mutex);

    std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

    status_now = SYN_RCVD;
}

void Receive_SYN_RCVD(int UDPReceiver)
{
    char recv_buf[1500]={};
    sockaddr_in tmp_addrSender;

    socklen_t tmp_addrSenderlen = sizeof(tmp_addrSender);
    recvfrom(UDPReceiver,recv_buf,sizeof(recv_buf),0,(sockaddr*)&tmp_addrSender,&tmp_addrSenderlen);

    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

    if(ACK(Seg) && NAME(Seg)){
        int64_t id = Seg.seqno;

        pthread_mutex_lock(&mutex);
        ReliableUDPSegment recent_Seg = WaitingSegmentid[id];
        WaitingAckSegment.erase(recent_Seg);
        WaitingSegmentid.erase(id);
        pthread_mutex_unlock(&mutex);

        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";
        std::cout<<"accept connect\n";

        recv_reassembler.create_file(Seg.data);

        Seg = CreateEmptySeg(0b00100000,0); //ACK
        Seg.DestPort = addrSender.sin_port;
        Seg.SrcPort = htons(9961);
        CalculatedCheckSum(Seg);

        sendto(UDPSender,(char*)&Seg,sizeof(Seg),0,(sockaddr*)&addrSender,sizeof(addrSender));
        std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

        status_now = ESTABLISHED;
    }
    else{
        std::cout<<"unexpected Data Received\n";
    }
}
void Receive_ESTABLISHED(int UDPReceiver)
{
    while(true){
        char recv_buf[1500]={};
        sockaddr_in tmp_addrSender;
        socklen_t tmp_addrSenderlen = sizeof(tmp_addrSender);
        ReliableUDPSegment Seg;

        recvfrom(UDPReceiver,recv_buf,sizeof(recv_buf),0,(sockaddr*)&tmp_addrSender,&tmp_addrSenderlen);

        Seg = *(ReliableUDPSegment*)recv_buf;
        if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";

        int64_t seqno = Seg.seqno;

        max_recv_seqno = std::max(max_recv_seqno,seqno);

        recv_reassembler.recv_string(Seg.data,Seg.seqno,Seg.Len);

        if(FIN(Seg)){
            Seg = CreateEmptySeg(0b01100000,0); //FIN ACK
            Seg.DestPort = addrSender.sin_port;
            Seg.SrcPort = htons(9961);
            CalculatedCheckSum(Seg);

            sendto(UDPSender,(char*)&Seg,sizeof(Seg),0,(sockaddr*)&addrSender,sizeof(addrSender));

            pthread_mutex_lock(&mutex);
            int64_t id = Seg.seqno;
            WaitingSegmentid[id] = Seg;
            WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
            pthread_mutex_unlock(&mutex);

            std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

            recv_reassembler.written_to_file();
            status_now = CLOSE_WAIT;
            break;
        }
        

        Seg = CreateEmptySeg(0b00100000,seqno); //ACK
        Seg.DestPort = addrSender.sin_port;
        Seg.SrcPort = htons(9961);
        std::cout<<"Receiver window status is "<<recv_reassembler.get_recv_window_status()<<"\n";
        CalculatedCheckSum(Seg);

        sendto(UDPSender,(char*)&Seg,sizeof(Seg),0,(sockaddr*)&addrSender,sizeof(addrSender));
        std::cout<<"Send {"<<GetSegInfo(Seg)<<"}(ackno = seqno)\n";

    }
}

void Receive_CLOSE_WAIT(int UDPReceiver)
{
    char recv_buf[1500]={};
    sockaddr_in tmp_addrSender;

    socklen_t tmp_addrSenderlen = sizeof(tmp_addrSender);

    recvfrom(UDPReceiver,recv_buf,sizeof(recv_buf),0,(sockaddr*)&tmp_addrSender,&tmp_addrSenderlen);

    ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
    if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

    std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}\n";;

    if(ACK(Seg)){
        int64_t id = Seg.seqno;

        pthread_mutex_lock(&mutex);
        ReliableUDPSegment recent_Seg = WaitingSegmentid[id];
        WaitingAckSegment.erase(recent_Seg);
        WaitingSegmentid.erase(id);
        pthread_mutex_unlock(&mutex);

        status_now = CLOSED;
    }
}

void Receive_CLOSED(int UDPReceiver)
{
    std::cout<<"receive successfully,receiver will close.\n";
    return;
}
