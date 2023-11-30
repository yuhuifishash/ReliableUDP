#ifndef RELIABLEUDPSEGMENT_H
#define RELIABLEUDPSEGMENT_H

#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * SYN FIN ACK
*/
enum ReliableSenderUDPStatus
{
    CLOSED = 1,SYN_SENT = 2,
    ESTABLISHED = 3,
    FIN_WAIT = 4,TIME_WAIT = 6,
    SEND_CLOSED = 7
};

struct ReliableUDPSegment
{
    uint16_t SrcPort;
    uint16_t DestPort;
    uint16_t check_sum;
    uint8_t FLAGS;
    int64_t seqno;
    uint32_t Len;
    char data[1024+5];

    bool operator < (const ReliableUDPSegment& s)const
    {
        return seqno < s.seqno;
    }
};


#define SYN(Seg)  ((Seg).FLAGS & 0b10000000)
#define FIN(Seg)  ((Seg).FLAGS & 0b01000000)
#define ACK(Seg) ((Seg).FLAGS & 0b00100000)
#define RST(Seg) ((Seg).FLAGS & 0b00000001)
#define NAME(Seg) ((Seg).FLAGS & 0b00010000)

ReliableUDPSegment CreateEmptySeg(uint8_t FLAGS,int64_t seqno);
ReliableUDPSegment CreateDataSeg(uint8_t FLAGS,int64_t seqno,std::string data);
void CalculatedCheckSum(ReliableUDPSegment& Seg);//差错检验
bool CheckSegSum(ReliableUDPSegment& Seg);
std::string GetSegInfo(ReliableUDPSegment& Seg);//用于打印日志


#endif // RELIABLEUDPSEGMENT_H