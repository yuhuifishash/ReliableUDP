#include "reassembler.h"
#include <iostream>

Reassembler::Reassembler()
{

}

void Reassembler::create_file(std::string name)
{
    fout.open("testdata_receiver/" + name,std::ios::out | std::ios::binary);
}

void Reassembler::recv_string(char* str, int seqno, int len)
{
    if((int)buffer.size() == seqno){
        for(int i=0;i<len;++i){
            buffer.push_back(str[i]);
        }
    }
}

void Reassembler::written_to_file()
{
    for(auto ch:buffer){
        fout.write(&ch,1);
    }
    fout.close();
}
