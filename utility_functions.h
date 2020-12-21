#ifndef UTILITY_FUNCTIONS
#define UTILITY_FUNCTIONS

#include "structures.h"
#include <bits/stdc++.h>

using namespace std;
string get_colon_joined_string(vector<string> &t_ip) {
    string ans = "";
    for(long long int i = 0; i < t_ip.size(); i++) {
        ans += t_ip[i];
        if(i != t_ip.size() - 1) ans += ":";
    }
    return ans;
}

inline void reset_buffer(char* t_buffer) {
    memset(t_buffer, 0, sizeof(t_buffer));
}

void get_file_info_into_buffer(char *t_buffer, file_info &t_file_info) {
    vector<string> msg_strings;

    string chunks = to_string(t_file_info.chunks);
    msg_strings.push_back(chunks);

    string file_size = to_string(t_file_info.file_size);
    msg_strings.push_back(file_size);

    string chunks_info = "";
    for(long long int i = 0; i < t_file_info.chunks_bitset.size(); i++) {
        if(t_file_info.chunks_bitset[i]) {
            chunks_info += "1";
            continue;
        }
        chunks_info += "0";
    }
    msg_strings.push_back(chunks_info);

    string file_name = t_file_info.file_name;
    msg_strings.push_back(file_name);

    string msg_string = get_colon_joined_string(msg_strings);    
    reset_buffer(t_buffer);
    strcpy(t_buffer, msg_string.c_str());
}

long long int get_int(char* t_int) {
    long long int ans = 0;

    long long int length = strlen(t_int);
    for(long long int i = 0; i < length; i++) {
        ans = ans * 10 + (t_int[i] - '0');
    }
    return ans;
}

vector<char*> parse_message(char* t_recv_buffer, char *delim = ":") {
    cout << "parsing message" << endl;
    vector<char*> tokens;
    char *token = strtok(t_recv_buffer, delim);
    while(token) {
        // cout << "token = " << token << endl;
        tokens.push_back(token);
        token = strtok(NULL, delim);
    }
    return tokens;
}

#endif