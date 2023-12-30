#include <iostream>
#include <vector>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <chrono>
#include "reliableudpsegment.h"
#include "reliableudpconnect.h"
#include "bytestream.h"
#include "router/sendrouter.h"

int UDPSender;
sockaddr_in addrReceiver;
ByteStream buf;
pthread_mutex_t mutex;
pthread_mutex_t send_mutex;

extern double avg_delay;
extern double max_delay;
extern double min_delay;
extern int cnt;

ReliableSenderUDPStatus status_now;//可靠UDP传输自动机状态
int main(int argc, char *argv[])
{

    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&send_mutex,NULL);

    UDPSender = socket(AF_INET,SOCK_DGRAM, IPPROTO_UDP);

    addrReceiver.sin_family = AF_INET;
    addrReceiver.sin_port = htons(9961);
    addrReceiver.sin_addr.s_addr = inet_addr("127.0.0.1");

    sockaddr_in addrAcker;
    addrAcker.sin_family = AF_INET;
    addrAcker.sin_port = htons(9760);
    addrAcker.sin_addr.s_addr = inet_addr("127.0.0.1");

    int UDPAcker = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    bind(UDPAcker,(sockaddr*)&addrAcker,sizeof(addrAcker));

    std::fstream f;
    f.open("testdata/1.jpg",std::ios::in | std::ios::binary);
    buf.push_byte(f);
    buf.set_name("1.jpg");

    status_now = CLOSED;

    double filesize = buf.get_size()/1024.0;
    std::cout<<"The file size is "<<filesize<<"KB\n";

    //build the router Sendthread
    pthread_t send_t;
    pthread_create(&send_t,NULL,SendThread,NULL);

    auto start = std::chrono::high_resolution_clock::now(); 
    ReliableUDPConnect(UDPAcker,addrAcker);

    auto finish = std::chrono::high_resolution_clock::now();

    std::cout<<"connect close\n";

    std::chrono::duration<float> elapsed = finish - start;
    std::cout<<"Total time is "<<elapsed.count()-2.0<<"s"<<"\n";
    std::cout<<"The speed is "<<filesize/(elapsed.count()-2.0)<<"KB/s"<<"\n";

    avg_delay/=cnt;
    std::cout<<"Average delay is "<<avg_delay*1000.0<<" ms\n";
    std::cout<<"Max delay is "<<max_delay*1000.0<<" ms\n";
    std::cout<<"Min delay is "<<min_delay*1000.0<<" ms\n";


    return 0;
}


