#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include <pthread.h>
#include <bits/stdc++.h>
#include "structures.h"
#include "utility_functions.h"

#define MSGSIZE 2048
#define CHUNKSIZE 32000
#define LOG_FILE "server_log.txt"
#define LOCALHOST "127.0.0.1"
#define QUEUE_SIZE 10

// message conventions
/*
    Req: get file info (from peer)      
    gfi:<filename>
    Res: 
    number_of_chunks:file_size:chunk_bit_vector:filename
*/

/*
    Req: get chunk at given index of given file
    gcn:<filename>:<chunk number>
    Res:
    buffer with buffer size, buffer separated by ':'
    Ack:
    1:ack
    0:ack
*/

/*
    Req: Add the file to the list of available files in the network
    uf:<port number of peer server>:<filename>:<filesize>:<number of chunks>
    Res:
        Success: Success:filename
        Failure: Fail:filename

*/

/*
    Req: get file info (from tracker)
    gfit:<filename>
    Res:
    <filename>:<file_size>:<number of chunks>:<port numbers separated by commas>
*/

/*
    Req: create user
    cu:<user_name>:<password>
    Res:
        Success
        Failure
*/

/*
    Req: login
    login:user_id:password:<port_number of the peer server>
    Res:
        Success
        Failure
*/

/*
    Req: logout
    logout:<port_num>
    Res:
        Success
        Failure
*/

/*
    Req: create group
    cg:<group_id>:<userid>
    Res:
        Success
        Failure
*/

/*
    Req: Get group owner address
    gga:<group_id>
    Res:
        Success:<owner_url>:<owner_port_num>
        Failure
*/
/*
    Req: Join group
    join:<group_id>:<port_number>
    Res:
        Success:<owner_port_number>
        Failure
*/
/*
    Req: Add peer to group
    apg:<group_id>:<user_id>
    Res:
        Success
        Failure
*/
/*
    Req: Leave group
    lg:<group_id>:<user_id>
    Res:
        Success
        Failure
*/
/*
    Req: List groups
    list_groups
    Res:
        List of group ids
*/
/*
    Req: Check if member of group
    cm:group_no:port_no
    Res:
        1
        0
*/
using namespace std;

// Global variables
int g_port; // Port for server thread
int g_socketfd; // file descriptor to store socket id for the servere thread
unordered_map<string, file_info_tracker > g_file_info_tracker;
unordered_map<string, string> g_users;
unordered_map<string, bool> g_online_peers;
unordered_map<int, group_info> g_group_info;
unordered_map<string, address_info> g_userid_address_mapping;

ofstream logfile(LOG_FILE);
void log_message(ofstream &t_file, const char* msg) {
    t_file << msg << endl;
    return;
}

streampos get_file_size(ifstream &t_file) {
    streampos begin, end;
    t_file.seekg(0);
    begin = t_file.tellg();
    // cout << begin << endl;
    t_file.seekg(0, ios::end);
    end = t_file.tellg();
    return end - begin;
}

bool add_file_info(char* t_port, char* t_filename, char* t_filesize, char* t_chunks) {
    int client_port = get_int(t_port);
    if(g_file_info_tracker.find(t_filename) != g_file_info_tracker.end()) {
        g_file_info_tracker[t_filename].peers_with_file.push_back(client_port);
        for(int i = 0; i < g_file_info_tracker[t_filename].peers_with_file.size(); i++) {
            cout << g_file_info_tracker[t_filename].peers_with_file[i] << endl;
        }
        return true;
    }
    file_info_tracker temp;
    g_file_info_tracker[string(t_filename)].info.chunks = get_int(t_chunks);
    g_file_info_tracker[string(t_filename)].info.file_name = string(t_filename).c_str();
    g_file_info_tracker[string(t_filename)].info.file_size = get_int(t_filesize);
    g_file_info_tracker[string(t_filename)].peers_with_file.push_back(client_port);
    // g_file_info_tracker[t_filename] = temp;

    cout << "chunks " << g_file_info_tracker[t_filename].info.chunks << endl;
    cout << "file_info " << g_file_info_tracker[t_filename].info.file_name << endl;
    cout << "file_size " << g_file_info_tracker[t_filename].info.file_size << endl;
    cout << "client ports " << endl;

    for(int i = 0; i < g_file_info_tracker[t_filename].peers_with_file.size(); i++) {
        cout << g_file_info_tracker[t_filename].peers_with_file[i] << endl;
    }
    // cout << "chunks " << g_file_info_tracker[t_filename].info.chunks << endl;
    return true;
}

int get_abb_index(const char* t_msg) {
    if(strcmp(t_msg, "gfi") == 0) {
        return 1;
    }
    if(strcmp(t_msg, "gcn") == 0) {
        return 2;
    }
    if(strcmp(t_msg, "uf") == 0) {
        return 3;
    }
    if(strcmp(t_msg, "gfit") == 0) {
        return 4;
    }
    if(strcmp(t_msg, "cu") == 0) {
        return 5;
    }
    if(strcmp(t_msg, "login") == 0) {
        return 6;
    }
    if(strcmp(t_msg, "logout") == 0) {
        return 7;
    }
    if(strcmp(t_msg, "cg") == 0) {
        return 8;
    }
    if(strcmp(t_msg, "gga") == 0) {
        return 9;
    }
    if(strcmp(t_msg, "apg") == 0) {
        return 10;
    }
    if(strcmp(t_msg, "lg") == 0) {
        return 11;
    }
    if(strcmp(t_msg, "list_groups") == 0) {
        return 12;
    }
    if(strcmp(t_msg, "cm") == 0) {
        return 13;
    }
    return 5;
}

inline void print_err_and_exit(const char* t_msg) {
    cout << t_msg << endl;
    exit(1);
}

inline void print_message(const char* t_msg) {
    cout << t_msg << endl;
}

// string get_comma_separated_peers(vector<int> &t_peers) {
//     string ans = "";
//     bool online_peer_exists = false;
//     for(int i = 0; i < t_peers.size(); i++) {
//         if(g_online_peers[t_peers[i]]) {
//             online_peer_exists = true;
//             ans = ans + to_string(t_peers[i]) + ",";
//             cout << ans << endl;
//         }
//     }

//     if (online_peer_exists) return ans.substr(0, ans.length() - 1);
//     return "0";
// }

// string generate_file_info_message(file_info_tracker &t_f) {
//     cout << "generating file into message" << endl;
//     string msg;

//     cout << "checking file info in hashmap " << endl;
    
//     cout << "chunks " << t_f.info.chunks << endl;
//     cout << "file_info " << t_f.info.file_name << endl;
//     cout << "file_size " << t_f.info.file_size << endl;

//     msg = msg + string(t_f.info.file_name) + ":";
//     cout << msg << endl;
    
//     msg = msg + to_string(t_f.info.file_size) + ":";
//     cout << msg << endl;
    
//     msg = msg + to_string(t_f.info.chunks) + ":";
//     cout << msg << endl;
    
//     msg = msg + get_comma_separated_peers(t_f.peers_with_file);
//     cout << msg << endl;

//     return msg;
// }

// void send_file_info(int t_client_fd, string t_file_name) {
//     file_info f = file_info_map[t_file_name];
//     log_message(logfile, "sending file");
//     log_message(logfile, f.file_name);
//     char buffer[10000];
//     get_file_info_into_buffer(buffer, f);
//     cout << "sending size = " << sizeof(buffer) << endl;

//     // cout << "sending " << buffer << endl;
//     send(t_client_fd, buffer, sizeof(buffer), 0);
// }

// void* send_chunks(void* arg) {
//     long long int index = ((chunk_info*)arg) -> index;
//     int clientfd = ((chunk_info*)arg) ->socket_id;
//     string filename = ((chunk_info*)arg) -> file_name;

//     ifstream requested_file(filename);
//     requested_file.seekg(index * CHUNKSIZE);

//     char buffer[CHUNKSIZE + 10];
//     char dummy_buffer[CHUNKSIZE];
//     long long int bytesRead = 0;
//     while(!requested_file.eof() && bytesRead < CHUNKSIZE) {
//         requested_file.read(buffer, CHUNKSIZE);
//         bytesRead += requested_file.gcount();
//         string temp = to_string(requested_file.gcount()) + ":" + string(buffer).substr(0, CHUNKSIZE);
//         memset(buffer, '\0', sizeof(buffer));
//         strcpy(buffer, temp.c_str());
//         // cout << "sending " << buffer << endl;
//         send(clientfd, buffer, strlen(buffer) + 1, 0); //send chunk
//         memset(buffer, '\0', sizeof(buffer));
//         if(recv(clientfd, dummy_buffer, sizeof(dummy_buffer), 0) > 0) {
//             // bool success = get_int(strtok(buffer, ":"));
//             cout << "ack received " << endl;
//             // if(!success) { 
//             //     requested_file.seekg(bytesRead - requested_file.gcount());
//             //     bytesRead -= requested_file.gcount();
//             // }
//         }
//     }
//     // cout << "sending last chunk" << endl;
//     // memset(buffer, 0, CHUNKSIZE + 10);
//     // string end_chunk = to_string(0) + ":abc";
//     // cout << end_chunk << endl;
//     // strcpy(buffer, end_chunk.c_str());
//     // cout << buffer << endl;
//     // send(clientfd, buffer, strlen(buffer) + 1, 0);
// }

bool is_peer_online(string t_userid) {
    if(g_online_peers.find(t_userid) == g_online_peers.end() || g_online_peers[t_userid] == false) {
        return false;
    }
    return true;
}

void* serve_requests(void* arg) {
    // cout << endl << "running hi function " << endl;
    address_info client_address = *((address_info *)arg);
    int clientfd = client_address.fd;
    char recv_buffer[MSGSIZE];
    char send_buffer[MSGSIZE];
    string ack;
    cout << "clientfd = " << clientfd << endl;
    while(recv(clientfd, recv_buffer, sizeof(recv_buffer), 0) > 0) {
        cout << recv_buffer << endl;
        vector<char*> message = parse_message(recv_buffer);
        cout << message.size() << endl;
        int index = get_abb_index(message[0]);
        cout << "index = " << index << endl;
        switch(index) {
            case 3: {cout << "request for uploading file " << message[2] << endl;
                    add_file_info(message[1], message[2], message[3], message[4]);
                    g_group_info[get_int(message[5])].files[string(message[2])] = true;
                    string t_filename = string(message[2]);
                    cout << "chunks " << g_file_info_tracker[t_filename].info.chunks << endl;
                    cout << "file_info " << g_file_info_tracker[t_filename].info.file_name << endl;
                    cout << "file_size " << g_file_info_tracker[t_filename].info.file_size << endl;
                    cout << "client ports " << endl;

                    for(int i = 0; i < g_file_info_tracker[t_filename].peers_with_file.size(); i++) {
                        cout << g_file_info_tracker[t_filename].peers_with_file[i] << endl;
                    }
                    // send(clientfd, send_buffer, strlen(send_buffer), 0);
                    break;}
            // case 4: {
            //             cout << "requesting file info for " << message[1] << " for file " << message[1] << endl;
            //             file_info_tracker f = g_file_info_tracker[message[1]];
                        
            //             string msg = generate_file_info_message(f);
            //             cout << "sending " << msg << endl;
            //             bzero(send_buffer, MSGSIZE);
            //             strcpy(send_buffer, msg.c_str());
            //             cout << "sending " << send_buffer << endl;
            //             send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
            //             break;
            //         }
            
            case 5: {   // Create user
                        /*
                            Req: create user
                            cu:<user_name>:<password>
                            Res:
                                Success
                                Failure
                        */
                        cout << "request for creating user " << message[1] << endl;
                        string m_user_id = string(message[1]);
                        string m_password = string(message[2]);

                        if(g_users.find(m_user_id) != g_users.end()) {
                            cout << "User already exists" << endl;
                            
                            string msg = "Failure";
                            bzero(send_buffer, MSGSIZE);
                            strcpy(send_buffer, msg.c_str());
                            send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                            break;
                        }
                        else {
                            g_users[m_user_id] = string(m_password);
                            cout << "User created succesfully" << endl;
                            
                            string msg = "Success";
                            bzero(send_buffer, MSGSIZE);
                            strcpy(send_buffer, msg.c_str());
                            send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                            break;
                        }
                    }

            case 6: {   // Login user
                        /*
                            Req: login
                            login:user_id:password:<port_number of the peer server>
                            Res:
                                Success
                                Failure
                        */
                        cout << "request for logging in user " << message[1] << endl;
                        string m_user_id = string(message[1]);
                        string m_password = string(message[2]);
                        int m_port_no = get_int(message[3]);

                        if(g_users.find(m_user_id) != g_users.end()) {
                            string pw = g_users[m_user_id];
                            if(strcmp(pw.c_str(), m_password.c_str()) == 0) {
                                g_online_peers[m_user_id] = true;
                                address_info user_address = client_address;
                                user_address.port = m_port_no;
                                g_userid_address_mapping[m_user_id] = user_address;
                                
                                cout << "User logged in succesfully" << endl;
                                
                                string msg = "Success";
                                bzero(send_buffer, MSGSIZE);
                                strcpy(send_buffer, msg.c_str());
                                send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                                break;
                            }
                        }
                        cout << "Failed to log in" << endl;
                        
                        string msg = "Failure";
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
                    }

            // case 7: {   // Logout user
            //             cout << "Request for logging out " << message[1] << endl;
            //             g_online_peers[get_int(message[1])] = false;
            //             string msg = "Success";
            //             bzero(send_buffer, MSGSIZE);
            //             strcpy(send_buffer, msg.c_str());
            //             send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
            //             break;
            // }

            case 8: {   // Create group
                        /*
                            Req: create group
                            cg:<group_id>:<userid>
                            Res:
                                Success
                                Failure
                        */
                        int m_group_id = get_int(message[1]);
                        string m_owner_userid = message[2];

                        cout << "Request for creating group " << m_group_id << " from " << " User " << m_owner_userid << endl;
                        
                        if(!is_peer_online(m_owner_userid) || g_group_info.find(m_group_id) != g_group_info.end()) {
                            cout << "Failed to create group" << endl;
                            
                            string msg = "Failure";
                            bzero(send_buffer, MSGSIZE);
                            strcpy(send_buffer, msg.c_str());
                            send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                            break;
                        }

                        unordered_map<string, bool> member_peers;
                        unordered_map<string, bool> files;
                        group_info temp = {
                            m_group_id,
                            member_peers,
                            files,
                            m_owner_userid
                        };
                        g_group_info[m_group_id] = temp;

                        string msg = "Success";
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
            }

            case 9: {   // Get address of group owner
                        /*
                            Req: Get group owner address
                            gga:<group_id>
                            Res:
                                Success:<owner_url>:<owner_port_num>
                                Failure
                        */
                        int m_group_id = get_int(message[1]);
                        cout << "Requesting owner address of group " << m_group_id << endl;
                        string msg = "";
                        if(g_group_info.find(m_group_id) != g_group_info.end()) {
                            string owner_userid = g_group_info[m_group_id].owner_userid;
                            address_info owner_address = g_userid_address_mapping[owner_userid];

                            vector<string> message_parts;
                            message_parts.push_back("Success");
                            message_parts.push_back(owner_address.url);
                            message_parts.push_back(to_string(owner_address.port));
                            msg = get_colon_joined_string(message_parts);
                        }
                        else {
                            msg = "Failure";
                        }
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
            }
            
            case 10: {  // Add peer to group
                        /*
                            Req: Add peer to group
                            apg:<group_id>:<user_id>
                            Res:
                                Success
                                Failure
                        */
                        int m_groupid = get_int(message[1]);
                        string m_userid = message[2];
                        cout << "Requesting to add peer " << m_userid << " to group " << m_groupid << endl;
                        
                        g_group_info[m_groupid].member_peers[m_userid] = true;
                        
                        string msg = "Success";
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
            }
            
            case 11: {  // Leave group
                        /*
                            Req: Leave group
                            lg:<group_id>:<user_id>
                            Res:
                                Success
                                Failure
                        */
                        int m_groupid = get_int(message[1]);
                        string m_userid = message[2];

                        cout << m_userid << " requesting to leave group " << message[1] << endl;
                        g_group_info[get_int(message[1])].member_peers[m_userid] = false;

                        string msg = "Success";
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
            }
            
            case 12: {  // Send list of groups
                        /*
                            Req: List groups
                            list_groups
                            Res:
                                List of group ids
                        */
                        cout << "Sending list of groups " << endl;
                        string msg = "Success";

                        unordered_map<int, group_info>::iterator it;
                        it = g_group_info.begin();
                        while(it != g_group_info.end()) {
                            msg += (":" + to_string(it -> first));
                            cout << msg << endl;
                            it++;
                        }
                        bzero(send_buffer, MSGSIZE);
                        strcpy(send_buffer, msg.c_str());
                        send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
                        break;
            }
            // case 13: {
            //             cout << "checking membership" << endl;
            //             if(g_group_info.find(get_int(message[1])) == g_group_info.end()) {
            //                 string msg = "0";
            //                 bzero(send_buffer, MSGSIZE);
            //                 strcpy(send_buffer, msg.c_str());
            //                 send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
            //                 break;
            //             }
            //             if( (g_group_info[get_int(message[1])].member_peers.find(get_int(message[2])) == g_group_info[get_int(message[1])].member_peers.end() )
            //                 || (g_group_info[get_int(message[1])].member_peers[get_int(message[2])] == false) ) {
            //                 string msg = "0";
            //                 bzero(send_buffer, MSGSIZE);
            //                 strcpy(send_buffer, msg.c_str());
            //                 send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
            //                 break;
            //             }

            //             string msg = "1";
            //             bzero(send_buffer, MSGSIZE);
            //             strcpy(send_buffer, msg.c_str());
            //             send(clientfd, send_buffer, strlen(send_buffer) + 1, 0);
            //             break;
            // }

        }
        memset(recv_buffer, '\0', MSGSIZE);
        cout << "recv buffer zeroed" << endl;
    } 
    cout << "exiting listener thread" << endl;
    pthread_exit(0);
}

int main(int argc, char* argv[]) {
    // Local variables
    char buffer[MSGSIZE]; // for storing received message
    sockaddr_in client_address; // address of client, initially empty
    vector<pthread_t> t_ids; // for storing ids of created threads

    // ------------------------------------------------------Peer as a server----------------------------------------
    
    
    struct sockaddr_in server_address;

    // create socket
    g_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(g_socketfd < 0) print_err_and_exit("Couldn't create socket");


    // set server address
    server_address.sin_family = AF_INET;
    g_port = atoi(argv[1]);
    server_address.sin_port = htons(g_port); //host to network short
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind server to port
    if(bind(g_socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("Unable to bind");
    }
    print_message("Binding successful");

    // start listening
    if(listen(g_socketfd, QUEUE_SIZE) < 0) print_err_and_exit("listen error.");
    print_message((string("Tracker listening on port ") + to_string(g_port) + "...").c_str() );

    
    while(1) {
        int client_address_length = sizeof(client_address);
        print_message("Waiting for connection");
        fflush(stdout);

        int clientfd = accept(g_socketfd, (sockaddr*)&client_address, (socklen_t*)&client_address_length);

        pthread_t t_id;
        t_ids.push_back(t_id);
        int size = t_ids.size();
        cout << "new connection from: " << inet_ntoa(client_address.sin_addr) << " : " << ntohs(client_address.sin_port) << endl;

        address_info client_address_info_struct = {
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port),
            clientfd
        };
        pthread_create(&(t_ids.at(t_ids.size() - 1)), NULL, serve_requests, &client_address_info_struct);

    }

    for(int i = 0; i < t_ids.size(); i++) {
        pthread_join(t_ids[i], NULL); // blocking call, waits till the thread is completed, but we want parellel threads, hence outside above for loop.
    }

    return 0;
}