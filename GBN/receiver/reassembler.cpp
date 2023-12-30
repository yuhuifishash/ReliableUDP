#include "reassembler.h"
#include <iostream>

extern int64_t now_ackno;

Reassembler::Reassembler()
{

}

void Reassembler::create_file(std::string name)
{
    fout.open("testdata_receiver/" + name,std::ios::out | std::ios::binary);
}

int Reassembler::recv_string(char* str, int seqno, int len)
{
    if((int)buffer.size() == seqno){
        for(int i=0;i<len;++i){
            buffer.push_back(str[i]);
        }
        now_ackno = seqno;
        return 1;
    }
    return 0;
}

void Reassembler::written_to_file()
{
    for(auto ch:buffer){
        fout.write(&ch,1);
    }
    fout.close();
}
