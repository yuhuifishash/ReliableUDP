#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include "reliableudpaccept.h"
#include "reliableudpsegment.h"
#include "reassembler.h"

int UDPSender;
sockaddr_in addrSender;
ReliableReceiverUDPStatus status_now;
Reassembler recv_reassembler;

pthread_mutex_t mutex;

int main(int argc, char *argv[])
{
    pthread_mutex_init(&mutex,NULL);

    int UDPReceiver = socket(AF_INET,SOCK_DGRAM, IPPROTO_UDP);


    sockaddr_in addrReceiver;
    addrReceiver.sin_family = AF_INET;
    addrReceiver.sin_port = htons(9961);
    addrReceiver.sin_addr.s_addr = inet_addr("127.0.0.1");

    bind(UDPReceiver,(sockaddr*)&addrReceiver,sizeof(addrReceiver));

    status_now = LISTEN;
    ReliableUDPAccept(UDPReceiver);

    std::cout<<"connect close\n";

    //pthread_join()
    return 0;
}
