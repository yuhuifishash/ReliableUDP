#ifndef BYTESTREAM_H
#define BYTESTREAM_H
#include <deque>
#include <string>
#include <fstream>

class ByteStream
{
private:
    std::string name{};
    std::deque<char> buffer{};
    bool name_send_tag = 0;
public:
    bool end_tag = 0;
    int get_size(){return buffer.size();}
    void set_name(std::string s){name = s;}
    void push_byte(std::fstream& f);//get bytes from disk to memory
    std::string pop_some_byte(int len);//get at most len bytes from memory (if end then set end flag)
    bool is_end();

    ByteStream();
};

#endif // BYTESTREAM_H
