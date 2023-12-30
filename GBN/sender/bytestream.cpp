#include "bytestream.h"
#include <iostream>

ByteStream::ByteStream(){}

void ByteStream::push_byte(std::fstream& f)
{

    char ch;
    while(!f.eof()){
        f.get(ch);
        buffer.push_back(ch);
    }
    f.close();
}

std::string ByteStream::pop_some_byte(int len)
{
    if(name_send_tag == 0){
        name_send_tag = 1;
        return name;
    }
    std::string ans;
    len = std::min(len,(int)buffer.size());
    while(len--){
        ans += buffer.front();
        buffer.pop_front();
    }
    if(buffer.size() == 0){
        end_tag = 1;
    }
    return ans;
}

bool ByteStream::is_end()
{
    return end_tag;
}
