#ifndef STRUCTURES
#define STRUCTURES
#include <bits/stdc++.h>

using namespace std;

    struct file_info {
        long long int chunks;
        long long int file_size;
        vector<bool> chunks_info;
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
        vector<int> peers_with_file;
        vector<int> peers_with_chunks;
    };

    struct group_info {
        int group_id;
        unordered_map<string, bool> member_peers;
        unordered_map<string, bool> files;
        string owner_userid;
    };

    struct client_address_info {
        string client_address;
        int client_port;
        int clientfd;
    };


#endif