#ifndef RELIABLEUDPACCEPT_H
#define RELIABLEUDPACCEPT_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void tickevent();
void ReliableUDPAccept(int UDPReceiver);

void Receive_LISTEN(int UDPReceiver);
void Receive_SYN_RCVD(int UDPReceiver);
void Receive_ESTABLISHED(int UDPReceiver);
void Receive_CLOSE_WAIT(int UDPReceiver);
void Receive_CLOSED(int UDPReceiver);
#endif // RELIABLEUDPACCEPT_H
