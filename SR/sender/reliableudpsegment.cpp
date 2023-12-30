#include "reliableudpsegment.h"
#include <iostream>
#include <cstring>

ReliableUDPSegment CreateEmptySeg(uint8_t FLAGS,int64_t seqno)
{
    ReliableUDPSegment* Seg = new ReliableUDPSegment();
    memset(Seg->data,0,sizeof(Seg));
    Seg->FLAGS = FLAGS;
    Seg->seqno = seqno;
    return *Seg;
}

ReliableUDPSegment CreateDataSeg(uint8_t FLAGS,int64_t seqno,std::string data)
{
    ReliableUDPSegment* Seg = new ReliableUDPSegment();
    memset(Seg->data,0,sizeof(Seg));
    Seg->FLAGS = FLAGS;
    Seg->seqno = seqno;
    Seg->Len = data.size();
    memcpy(Seg->data,data.c_str(),data.size());
    return *Seg;
}

void CalculatedCheckSum(ReliableUDPSegment& Seg)
{
    uint32_t sum = 0;
    uint16_t ans = 0;
    sum += Seg.SrcPort;
    sum += Seg.DestPort;
    sum += Seg.FLAGS;
    sum += Seg.Len>>16;
    sum += Seg.Len & 0x0000FFFF;
    sum += Seg.seqno >> 48;
    sum += (Seg.seqno >> 32) & (0xFFFF0000);
    sum += (Seg.seqno >> 16) & (0xFFFF00000000);
    sum += Seg.seqno & 0xFFFF000000000000;
    for(int i = 0;i < 1024; i+=2){
        sum +=  (((uint16_t)Seg.data[i])<<8)|Seg.data[i+1];
    }
    sum = (sum >> 16) + (sum & 0x0000FFFF);
    if(sum & 0x00010000){ans = (sum & 0x0000FFFF) + 1;}
    else{ans = sum;}
    Seg.check_sum = ~ans;
    //std::cout<<~ans<<std::endl<<"\n";
}

bool CheckSegSum(ReliableUDPSegment& Seg)
{
    uint32_t sum = 0;
    uint16_t ans = 0;
    sum += Seg.SrcPort;
    sum += Seg.DestPort;
    sum += Seg.FLAGS;
    sum += Seg.check_sum;
    sum += Seg.Len>>16;
    sum += Seg.Len & 0x0000FFFF;
    sum += Seg.seqno >> 48;
    sum += (Seg.seqno >> 32) & (0xFFFF0000);
    sum += (Seg.seqno >> 16) & (0xFFFF00000000);
    sum += Seg.seqno & 0xFFFF000000000000;
    sum += Seg.Len & 0xFFFF000000000000;
    for(int i = 0;i < 1024; i+=2){
        sum +=  (((uint16_t)Seg.data[i])<<8)|Seg.data[i+1];
    }
    sum = (sum >> 16) + (sum & 0x0000FFFF);
    if(sum & 0x00010000){ans = (sum & 0x0000FFFF) + 1;}
    else{ans = sum;}
    //std::cout<<ans<<std::endl;
    return ans == 0xFFFF;
}


std::string GetSegInfo(ReliableUDPSegment& Seg)
{
    std::string ans;
    ans += "[";
    if(SYN(Seg)){ans += " SYN ";}
    if(FIN(Seg)){ans += " FIN ";}
    if(ACK(Seg)){ans += " ACK ";}
    ans += "] ";
    ans += "seqno=";
    ans += std::to_string(Seg.seqno);
    ans += " ";
    if(Seg.Len){
        ans += "DataLen = ";
        ans += std::to_string(Seg.Len);
    }
    return ans;
}


