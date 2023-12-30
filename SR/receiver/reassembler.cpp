#include "reassembler.h"
#include <iostream>

extern int64_t now_ackno;
extern uint16_t Receiver_windowsize;

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
        now_ackno = seqno;
        for(auto it = tmp_str_map.begin();it != tmp_str_map.end();){
            if((int)buffer.size() == it->first){
                for(int i=0;i<it->second.second;++i){
                    buffer.push_back(it->second.first[i]);
                }
                now_ackno = it->first;
                it = tmp_str_map.erase(it);
            }
            else{
                break;
            }
        }
    }
    else if((int)buffer.size() < seqno){
        std::string s;
        for(int i=0;i<len;++i){s.push_back(str[i]);}
        tmp_str_map.insert({seqno,{s,len}});
    }

}

void Reassembler::written_to_file()
{
    for(auto ch:buffer){
        fout.write(&ch,1);
    }
    fout.close();
}

std::string Reassembler::get_recv_window_status()
{
    std::string ans;
    int64_t now_seqno = buffer.size();
    for(int64_t i = now_seqno;i < now_seqno + Receiver_windowsize;i += 1024){
        if(tmp_str_map.find(i) != tmp_str_map.end()){
            ans += '1';
        }
        else{
            ans += '0';
        }
    }
    return ans;
}
