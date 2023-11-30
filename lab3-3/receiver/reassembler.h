#ifndef REASSEMBLER_H
#define REASSEMBLER_H
#include <fstream>
#include <vector>
#include <deque>
#include <string>
#include <map>

class Reassembler
{
    std::map<int64_t,std::pair<std::string,int> > tmp_str_map; //< seqno,<str,len> >
    std::vector<char> buffer;
    std::ofstream fout;
public:
    Reassembler();
    void create_file(std::string name);
    void recv_string(char* str,int seqno,int len);
    std::string get_recv_window_status();
    void written_to_file();
};

#endif 
// REASSEMBLER_H