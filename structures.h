#ifndef STRUCTURES
#define STRUCTURES
#include <bits/stdc++.h>

using namespace std;

    struct file_info {
        long long int chunks;
        long long int file_size;
        vector<bool> chunks_bitset;
        string file_name;
    };

    struct chunk_info {
        char* file_name;
        long long int index;
        int port_number;
    };

    struct chunk_info_server {
        char* file_name;
        long long int index;
        int clientfd;
    };

    struct file_info_tracker {
        file_info info;
        vector<string> peers_with_file;
        vector<string> peers_with_chunks;
    };

    struct group_info {
        int group_id;
        unordered_map<string, bool> member_peers;
        unordered_map<string, bool> files;
        string owner_userid;
    };

    struct address_info {
        string url;
        int port;
        int fd;
    };


#endif