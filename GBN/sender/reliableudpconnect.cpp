#include "reliableudpconnect.h"
#include "reliableudpsegment.h"
#include "bytestream.h"
#include "router/sendrouter.h"
#include <iostream>
#include <cstring>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <ctime>
#include <chrono>

int RetransTime = 300;

std::map<int64_t,ReliableUDPSegment> WaitingSegmentid;
std::map<ReliableUDPSegment,std::pair<int,int> > WaitingAckSegment;
std::map<int64_t,timeval> delayMap;//用于计算延时

int ESTABLISHED_end_tag = 0;
int recv_flag = 0;
std::map<int64_t,int> AckCounter;
extern int UDPSender;
extern sockaddr_in addrReceiver;
extern ReliableSenderUDPStatus status_now;
extern ByteStream buf;
extern pthread_mutex_t mutex;

uint16_t Sender_windowsize = 1024*32;
pthread_t send_established_t;

int64_t now_ackno = -1024;  //已被确认数据号
int64_t now_seqno = 0;  //发送数据号
int64_t end_seqno = -1; //最后一个数据包标号

int cnt = 0;
double avg_delay = 0;
double max_delay = 0;
double min_delay = 1e9;

void updateWaitingSegment(int64_t ackno)
{
    pthread_mutex_lock(&mutex);

    for(auto it = WaitingSegmentid.begin();it != WaitingSegmentid.end();){
        if(it->first <= ackno){
            WaitingAckSegment.erase(it->second);
            it = WaitingSegmentid.erase(it);
        }
        else{
            ++it;
        }
    }

    pthread_mutex_unlock(&mutex);
}

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
        if(timer.second <= 0){
            timer.second = timer.first;
            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
            
            auto Seg_info = Seg;
            printf("timeout,resend %s to Receiver\n",GetSegInfo(Seg_info).c_str());
        }
    }
    pthread_mutex_unlock(&mutex);
}

void* ESTABLISHED_send_thread(void* p)
{
    sockaddr_in addrSender = *((sockaddr_in*)p);
    
    while(true){
        if(recv_flag){
            pthread_mutex_lock(&mutex);
            recv_flag = 0;
            
            std::cout<<"Sender_usable_Windowsize is "<<Sender_windowsize - (now_seqno - now_ackno) + 1024<<"\n";
            int64_t Send_endseqno = now_seqno + Sender_windowsize - (now_seqno - now_ackno) + 1024;

            while(now_seqno < Send_endseqno && !buf.end_tag){
                ReliableUDPSegment Seg = CreateDataSeg(0b00000000,now_seqno,buf.pop_some_byte(std::min((int64_t)1024,Send_endseqno - now_seqno)));//Data
                Seg.SrcPort = addrSender.sin_port;
                Seg.DestPort = addrReceiver.sin_port;
                CalculatedCheckSum(Seg);

                if(buf.end_tag){end_seqno = now_seqno;}
                else{now_seqno += Seg.Len;}

                SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
                std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

                int64_t id = Seg.seqno;
                
                WaitingSegmentid[id] = Seg;
                WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};
                
                timeval tv_start;
                gettimeofday(&tv_start,NULL);
                delayMap[id] = tv_start;
                ++cnt;
            }
            pthread_mutex_unlock(&mutex);
        }
        if(buf.end_tag){
            break;
        }
    }
    return NULL;
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
    Seg.SrcPort = addrSender.sin_port;
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
    std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
    pthread_mutex_unlock(&mutex);


    status_now = ESTABLISHED;
}
void Send_ESTABLISHED(int Acker,sockaddr_in addrSender)
{
    sockaddr_in addrAckSender;
    socklen_t addrAckSenderlen = sizeof(addrAckSender);
    char recv_buf[1500]={};

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
            if(WaitingSegmentid.size()){pthread_mutex_unlock(&mutex);continue;}//对第一个包采用停等机制
            pthread_mutex_unlock(&mutex);
        }

        std::cout<<"Sender_usable_Windowsize is "<<Sender_windowsize<<"\n";
        int64_t Send_endseqno = Sender_windowsize;

        pthread_mutex_lock(&mutex);
        while(now_seqno < Send_endseqno && !buf.end_tag){
            Seg = CreateDataSeg(0b00000000,now_seqno,buf.pop_some_byte(std::min((int64_t)1024,Send_endseqno - now_seqno)));//Data
            Seg.SrcPort = addrSender.sin_port;
            Seg.DestPort = addrReceiver.sin_port;
            CalculatedCheckSum(Seg);

            if(buf.end_tag){end_seqno = now_seqno;}
            else{now_seqno += Seg.Len;}

            
            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
            std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";

            int64_t id = Seg.seqno;
            
            WaitingSegmentid[id] = Seg;
            WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
            
            timeval tv_start;
            gettimeofday(&tv_start,NULL);
            delayMap[id] = tv_start;
            ++cnt;
        }
        pthread_mutex_unlock(&mutex);
        break;
    }



    pthread_create(&send_established_t,NULL,ESTABLISHED_send_thread,(void*)(&addrSender));
    
    while(true){
        recvfrom(Acker,recv_buf,sizeof(recv_buf),0,(sockaddr*)&addrAckSender,&addrAckSenderlen);
        ReliableUDPSegment Seg = *(ReliableUDPSegment*)recv_buf;
        if(!CheckSegSum(Seg)){std::cout<<"CheckSum error,drop this data\n";return;}

        int64_t ackno = Seg.seqno;
        pthread_mutex_lock(&mutex);
        std::cout<<"Receive {"<<GetSegInfo(Seg)<<"}(ackno = seqno)\n";
        pthread_mutex_unlock(&mutex);

        timeval tv_end;
        gettimeofday(&tv_end,NULL);
        timeval tv_start = delayMap[ackno];
        delayMap.erase(ackno);
        double delay = ((double)(1000000*(tv_end.tv_sec-tv_start.tv_sec)+ tv_end.tv_usec-tv_start.tv_usec))/1000.0;
        if(delay < 10000){
            max_delay = std::max(max_delay,delay);
            min_delay = std::min(min_delay,delay);
            avg_delay += delay;
        }

        if(ackno > now_ackno){updateWaitingSegment(ackno);}//更新超时重传任务

        pthread_mutex_lock(&mutex);
        now_ackno = ackno;
        recv_flag = 1;
        pthread_mutex_unlock(&mutex);

        AckCounter[ackno]++;
        if(AckCounter[ackno] >= 3){//快速重传对应数据包
            AckCounter[ackno] = 0;

            pthread_mutex_lock(&mutex);
            for(auto &[Seg,timer]:WaitingAckSegment){
                SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));
                auto Seg_info = Seg;
                printf("Fast Resend %s to Receiver\n",GetSegInfo(Seg_info).c_str());
            }
            pthread_mutex_unlock(&mutex);
        }

        if(now_ackno == end_seqno){//传输完毕,发送FIN信号
            Seg = CreateEmptySeg(0b01000000,0);//FIN
            Seg.SrcPort = addrSender.sin_port;
            Seg.DestPort = addrReceiver.sin_port;
            CalculatedCheckSum(Seg);

            pthread_mutex_lock(&mutex);

            SendToRouter(UDPSender,Seg,sizeof(Seg),0,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

            int64_t id = Seg.seqno;
            
            WaitingSegmentid[id] = Seg;
            WaitingAckSegment[Seg] = std::pair<int,int>{RetransTime,RetransTime};//first 100ms
            
            std::cout<<"Send {"<<GetSegInfo(Seg)<<"}\n";
            pthread_mutex_unlock(&mutex);

            status_now = FIN_WAIT;
            break;
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
