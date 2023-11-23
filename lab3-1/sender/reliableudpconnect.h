#ifndef RELIABLEUDPCONNECT_H
#define RELIABLEUDPCONNECT_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
void tickevent();
void ReliableUDPConnect(int Acker,sockaddr_in addrSender);

void Send_CLOSED(int Acker,sockaddr_in addrSender);
void Send_SYNSENT(int Acker,sockaddr_in addrSender);
void Send_ESTABLISHED(int Acker,sockaddr_in addrSender);
void Send_FINWAIT(int Acker,sockaddr_in addrSender);
void Send_TIMEWAIT(int Acker,sockaddr_in addrSender);

#endif // RELIABLEUDPCONNECT_H
