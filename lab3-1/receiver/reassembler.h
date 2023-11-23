#ifndef REASSEMBLER_H
#define REASSEMBLER_H
#include <fstream>
#include <vector>
#include <deque>

class Reassembler
{
    std::deque<char> buffer;
    std::ofstream fout;
public:
    Reassembler();
    void create_file(std::string name);
    void recv_string(char* str,int seqno,int len);
    void written_to_file();
};

#endif // REASSEMBLER_H
