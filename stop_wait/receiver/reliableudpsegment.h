#ifndef RELIABLEUDPSEGMENT_H
#define RELIABLEUDPSEGMENT_H

#include <cstdint>
#include <sys/socket.h>
#include <string>

/*
 * SYN FIN ACK
*/
enum ReliableReceiverUDPStatus
{
    LISTEN = 1,SYN_RCVD = 2,
    ESTABLISHED = 3,
    CLOSE_WAIT = 4,
    CLOSED = 6
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
