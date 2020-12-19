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
#define CHUNKSIZE 512000
#define LOG_FILE "server_log.txt"
#define LOCALHOST "127.0.0.1"

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
    cg:<group_id>:<port_number>
    Res:
        Success
        Failure
*/

/*
    Req: Get group owner port number
    ggp:<group_id>
    Res:
        Success:<owner_port_number>
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
    apg:<group_id>:<port_number>
    Res:
        Success
        Failure
*/

/*
    Req: Leave group
    lg:<group_id>:<port_no>
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
int g_port;
char g_port_Ar[4];
int g_tracker_port;
int g_tracker_socket;
unordered_map<int, bool> g_groups_owned;
ofstream logfile(LOG_FILE);
void log_message(ofstream &t_file, const char* msg) {
    t_file << msg << endl;
    return;
}
unordered_map<string, file_info> file_info_map;

void get_tracker_info(char* t_filename) {
    ifstream tracker_file(t_filename);
    if(tracker_file.is_open()) {
        tracker_file >> g_tracker_port;
    }
    else {
        perror("Error in opening tracker file");
    }
    return;
}

streampos get_file_size(ifstream &t_file) {
    streampos begin, end;
    t_file.seekg(0);
    begin = t_file.tellg();
    cout << begin << endl;
    t_file.seekg(0, ios::end);
    end = t_file.tellg();
    cout << end << endl;
    return end - begin;
}

int get_abb_index(const char* t_msg) {
    if(strcmp(t_msg, "gfi") == 0) {
        return 1;
    }
    if(strcmp(t_msg, "gcn") == 0) {
        return 2;
    }
    if(strcmp(t_msg, "join") == 0) {
        return 3;
    }
    return 0;
}

inline void print_err_and_exit(const char* t_msg) {
    cout << t_msg << endl;
    exit(1);
}

inline void print_message(const char* t_msg) {
    cout << t_msg << endl;
}

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

int get_option_number(const char* t_command) {
    if(strcmp(t_command, "upload_file") == 0) {
        return 1;
    }
    if(strcmp(t_command, "download_file") == 0) {
        return 2;
    }
    if(strcmp(t_command, "create_user") == 0) {
        return 3;
    }
    if(strcmp(t_command, "login") == 0) {
        return 4;
    }
    if(strcmp(t_command, "logout") == 0) {
        return 5;
    }
    if(strcmp(t_command, "create_group") == 0) {
        return 6;
    }
    if(strcmp(t_command, "join_group") == 0) {
        return 7;
    }
    if(strcmp(t_command, "leave_group") == 0) {
        return 8;
    }
    if(strcmp(t_command, "list_groups") == 0) {
        return 9;
    }
    return 0;
}

bool check_if_member_of_group(char* t_group_no, int t_socket_fd) {
    string msg = "cm:" + string(t_group_no) + ":" + to_string(g_port);
    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socket_fd, buffer, strlen(buffer), 0);

    char resp[MSGSIZE];
    bzero(resp, MSGSIZE);
    int rs;

    rs = recv(t_socket_fd, resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in joining group");
    }
    else {
        vector<char* > response = parse_message(resp);
        if(strcmp(response[0], "1") == 0) return true;
        return false;
    }
    return false;
}

void upload_file_to_tracker(char* t_filename, char* t_groupid, int t_socketfd) {
    ifstream file(t_filename, ios::binary);
    
    bool is_member = check_if_member_of_group(t_groupid, t_socketfd);
    if(is_member) {
        int file_size = get_file_size(file);
        cout << "file size = " << file_size << endl;
        long long int num_chunks = file_size / CHUNKSIZE + 1;

        vector<bool> chunks(num_chunks, true);
        file_info f = {
            num_chunks,
            file_size,
            chunks,
            "input.txt"
        };

        string msg = "uf:"+to_string(g_port)+":"+string(t_filename)+":"+to_string(file_size)+":"+to_string(num_chunks) + ":" + string(t_groupid);
        char buffer[MSGSIZE];
        strcpy(buffer, msg.c_str());

        send(t_socketfd, buffer, strlen(buffer), 0);
        return;
    }
    cout << "Unable to upload to this group " << endl;
    return;
}

void get_file_info_from_tracker(char* file_resp, char* t_filename, int t_socketfd) {
    string msg = "gfit:" + string(t_filename);

    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    cout << "sent request for info" << endl;
    
    bzero(file_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, file_resp, MSGSIZE, 0);
    cout << "tried to receive the call" << endl;
    if(rs < 0) {
        print_err_and_exit("error in receiving file info");
    }
    cout << "recv buffer inside get_file_info_function " << file_resp << endl;
    return;
}

void write_chunk(string t_filename, long long int t_index, char* t_buffer, long long int t_size) {
    ofstream output_file(t_filename, ios::app);

    output_file.seekp(t_index * CHUNKSIZE);
    output_file.write(t_buffer, t_size);

    output_file.close();
}

void write_chunk(ofstream &t_f, long long int t_index, char* t_buffer, long long int t_size, long long int t_offset) {

    // t_f.seekp(t_index * CHUNKSIZE + t_offset);
    // if(t_index == 15) {
        cout << "writing for index " << t_index << " and offset calculated as " << (t_index * CHUNKSIZE + t_offset) << endl;
    // }
    t_f.write(t_buffer, t_size);
    // if(t_index == 15) {
        cout << "written " << t_size << " chars " << endl;
    // }
    // output_file.close();
}

void* get_chunk_from_peer(void* arg) {
    string filename = ((chunk_info*)arg) -> file_name;
    long long int index = ((chunk_info*)arg) -> index;
    int server_port_number = ((chunk_info*)arg) -> port_number;

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");

    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port_number); //host to network short

    if(inet_pton(AF_INET, LOCALHOST, &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    // print_message(string("Trying to connect with " + LOCALHOST + ": " + to_string(PORT)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }

    string msg_string = "gcn:" + filename + ":" + to_string(index);
    cout << "trying to send " << msg_string << endl;
    char buffer[MSGSIZE];
    strcpy(buffer, msg_string.c_str());
    send(socketfd, buffer, sizeof(buffer), 0);

    char resp_buffer[CHUNKSIZE];
    long long int bytes_received = 0;
    long long int rs;

    ofstream output_file(filename);
    output_file.seekp(index*CHUNKSIZE);
    while(bytes_received < CHUNKSIZE) {
        memset(resp_buffer, 0, CHUNKSIZE);
        // fflush(stdin);
        // fflush(stdout);
        // cout << "memsetted" << endl;
        if((rs = recv(socketfd, resp_buffer, CHUNKSIZE, 0)) > 0) {
            // cout << "received response after sending chunk request" << endl;
            cout << "received " << rs << endl;
            // cout << "received " << resp_buffer << endl;
            // cout << "received " << strlen(resp_buffer) << endl;
            // char *length = strtok(resp_buffer, ":");
            // char *chunk_buffer = strtok(NULL, ":");
            
            // long long int expected_chunk_length = get_int(length);
            // for (int i = 0; i <resp_components[2] strlen(resp_buffer); i++) {
            //     cout << resp_buffer[i];
            // }
            // cout << endl;
            // if(resp_buffer[rs - 1] == '\0') {
            //     cout << "last bit received for chunk number "<< index << endl;
            //     write_chunk(filename, index, resp_buffer, rs - 1, bytes_received);
            //     break; 
            // }
            if(resp_buffer[rs - 1] == '\0') {
                write_chunk(output_file, index, resp_buffer, rs - 1, bytes_received);
                break;
            }
            else {
                write_chunk(output_file, index, resp_buffer, rs, bytes_received);
            }
            bytes_received += rs;
            memset(resp_buffer, 0, CHUNKSIZE);
            // char dummy_buffer[] = "hello";
            // // strcpy(resp_buffer, ack.c_str());
            // send(socketfd, dummy_buffer, sizeof(dummy_buffer), 0);
        }
        else {
            break;
        }
        
        
    }
    // cout << "out of while loop" << endl;
    // close(socketfd);
    output_file.close();
    pthread_exit(NULL);
    cout << "thread exited" << endl;
}

void create_user(char* t_resp, char* t_username, char* t_password, int t_socketfd) {
    string msg = "cu:" + string(t_username) + ":" + string(t_password);
    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    cout << "requesting to create user with username " << string(t_username) << " and password " << string(t_password) << endl;

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in creating user");
    }
    return;

}

void login_user(char* t_resp, char* t_username, char* t_password, int t_socketfd) {
    string msg = "login:" + string(t_username) + ":" + string(t_password) + ":" + to_string(g_port);
    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    cout << "requesting to login user with username " << string(t_username) << " and password " << string(t_password) << endl;

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in logging user in");
    }
    return;

}

void logout_user(char* t_resp, int t_socketfd) {
    string msg = "logout:" + to_string(g_port);
    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    cout << "requesting to logout " << endl;

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in logging out");
    }
    return;

}

void create_group(char* t_resp, char* t_gid, int t_socketfd) {
    string msg = "cg:" + string(t_gid) + ":" + to_string(g_port);
    char buffer[MSGSIZE];

    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in creating group");
    }
    return;
}

void get_group_owner_port(char* t_resp, char* t_gid, int t_socketfd) {
    string msg = "ggp:" + string(t_gid);
    char buffer[MSGSIZE];

    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(t_socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in creating group");
    }
    return;
}

void request_to_join_group(char* t_resp, char* t_gid, char* t_owner_port) {
    sockaddr_in server_address;
    int socketfd;
    
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(get_int(t_owner_port)); //host to network short

    if(inet_pton(AF_INET, LOCALHOST, &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    print_message(string("Trying to connect with " + string(LOCALHOST) + ": " + to_string(g_tracker_port)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }
    print_message("connected successfully to the owner peer");

    string msg = "join:" + string(t_gid) + ":" + to_string(g_port);

    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());
    send(socketfd, buffer, strlen(buffer), 0);

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in joining group");
    }
    return;
}

vector<int> get_peer_ports(char* t_peer_list) {
    vector<int> peer_list;

    char *token = strtok(t_peer_list, ",");
    while(token) {
        // cout << "token = " << token << endl;
        peer_list.push_back(get_int(token));
        token = strtok(NULL, ",");
    }
    return peer_list;
}

void* client_thread(void* arg) {
    bool keep_running = true;

    //open new socket to communicate.
    sockaddr_in server_address;
    int socketfd;
    
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(g_tracker_port); //host to network short

    if(inet_pton(AF_INET, LOCALHOST, &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    print_message(string("Trying to connect with " + string(LOCALHOST) + ": " + to_string(g_tracker_port)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }
    print_message("connected successfully to the tracker");
    g_tracker_socket = socketfd;
    // Get command from the user.
    string command;
    char command_buffer[MSGSIZE];
    while(keep_running) {
        cout << "> ";
        getline(cin, command);
        bzero(command_buffer, MSGSIZE);
        strcpy(command_buffer, command.c_str());
        vector<char*> command_components = parse_message(command_buffer, " ");
        int option = get_option_number(command_components[0]);
        switch (option)
        {
            case 1: upload_file_to_tracker(command_components[1], command_components[2], socketfd);
                    break;
            
            case 2: {
                        char file_resp[MSGSIZE];
                        get_file_info_from_tracker(file_resp, command_components[1], socketfd);

                        vector<char*> resp_components = parse_message(file_resp);
                        vector<int> peer_ports = get_peer_ports(resp_components[3]);

                        long long int num_chunks = get_int(resp_components[2]);
                        long long int chunks_downloaded = 0;
                        long long int file_size = get_int(resp_components[1]);
                        vector<pthread_t> downloader_threads;
                        vector<chunk_info> chunk_infos;
                        cout << "number of ports = " << peer_ports.size();
                        cout << "number of chunks = " << num_chunks << endl;
                        cout << "file size = " << file_size << endl;
                        // fstream temp(string(resp_components[0]).c_str());
                        // string ch = "\0";
                        // temp << ch;
                        // temp.close();
                        if(peer_ports[0] == 0) {
                            cout << "No peer available for download " << endl;
                            break;
                        }
                        while(chunks_downloaded < num_chunks) {
                            long long int threads_required = min(num_chunks - chunks_downloaded, (long long)peer_ports.size());
                            downloader_threads.resize(threads_required);
                            chunk_infos.resize(threads_required);
                            cout << "threads required = " << threads_required << endl;
                            for(long long int i = 0; i < threads_required; i++) {
                                chunk_info ci;
                                chunk_infos[i].file_name = resp_components[0];
                                chunk_infos[i].index = chunks_downloaded + i;
                                chunk_infos[i].port_number = peer_ports[i];
                                if(pthread_create(&downloader_threads[i], NULL, get_chunk_from_peer, (void*)&chunk_infos[i]) < 0) {
                                    print_err_and_exit("Unable to create downloader threads");
                                }
                            }
                            for(long long int i = 0; i < threads_required; i++) {
                                pthread_join(downloader_threads[i], NULL);
                            }
                            chunks_downloaded += threads_required;
                            downloader_threads.resize(0);
                            chunk_infos.resize(0);
                        }
                        break;
                        
                    }
                case 3: {
                            char resp[MSGSIZE];
                            create_user(resp, command_components[1], command_components[2], socketfd);

                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "User created succesfully" << endl;
                            }
                            else {
                                cout << "Error in creating user" << endl;
                            }
                            break;
                }

                case 4: {
                            char resp[MSGSIZE];
                            login_user(resp, command_components[1], command_components[2], socketfd);

                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "User logged succesfully" << endl;
                            }
                            else {
                                cout << "Error in logging in" << endl;
                            }
                            break;
                }

                case 5: {
                            char resp[MSGSIZE];
                            logout_user(resp, socketfd);

                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "User logged out succesfully" << endl;
                            }
                            else {
                                cout << "Error in logging out" << endl;
                            }
                            break;
                }
                case 6: {
                            cout << "Trying to create group with id " << command_components[1];
                            char resp[MSGSIZE];

                            create_group(resp, command_components[1], socketfd);

                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                g_groups_owned[get_int(command_components[1])] = true;
                                cout << "Group " << command_components[1] << "created succesfully " << endl;
                            }
                            else {
                                cout << "Error in creating group " << command_components[1] << endl;
                            }
                            break;
                }
                case 7: {
                            cout << "Trying to join group " << command_components[1];
                            char resp[MSGSIZE];

                            get_group_owner_port(resp, command_components[1], socketfd);

                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "group owner port number =  " << response_components[1] << endl;
                                request_to_join_group(resp, command_components[1], response_components[1]);
                            }
                            else {
                                cout << "Error in joining group " << command_components[1] << endl;
                            }
                            break;
                }
                case 8: {
                            cout << "Trying to leave group " << command_components[1] << endl;
                            char resp[MSGSIZE];
                            bzero(resp, MSGSIZE);
                            string msg = "lg:" + string(command_components[1]) + ":" + to_string(g_port);
                            char buffer[MSGSIZE];
                            bzero(buffer, MSGSIZE);
                            strcpy(buffer, msg.c_str());
                            send(socketfd, buffer, strlen(buffer) + 1, 0);

                            int rs;

                            rs = recv(socketfd, resp, MSGSIZE, 0);

                            if(rs < 0) {
                                print_err_and_exit("error in leaving group");
                            }
                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "Left group " << command_components[1] << endl;
                            }
                            else {
                                cout << "Error in leaving group " << command_components[1] << endl;
                            }
                            break;
                }
                case 9: {
                            string msg = "list_groups";
                            char buffer[MSGSIZE];
                            bzero(buffer, MSGSIZE);
                            strcpy(buffer, msg.c_str());
                            send(socketfd, buffer, strlen(buffer) + 1, 0);

                            int rs;
                            char resp[MSGSIZE];
                            bzero(resp, MSGSIZE);
                            rs = recv(socketfd, resp, MSGSIZE, 0);
                            cout << resp << endl;
                            if(rs < 0) {
                                print_err_and_exit("error in listing groups");
                            }
                            vector<char*> response_components = parse_message(resp);
                            if(strcmp(response_components[0], "Success") == 0) {
                                for(int i = 1; i < response_components.size(); i++) {
                                    cout << response_components[i] << endl;
                                }
                            }
                            else {
                                cout << "Error in listing groups " << endl;
                            }
                            break;
                }
            default:
                break;
        }
    }
}
void* send_chunks(void* arg) {
    long long int index = ((chunk_info_server*)arg) -> index;
    int clientfd = ((chunk_info_server*)arg) ->clientfd;
    string filename = ((chunk_info_server*)arg) -> file_name;

    ifstream requested_file(filename);
    requested_file.seekg(index * CHUNKSIZE);

    char buffer[CHUNKSIZE + 1];
    char dummy_buffer[CHUNKSIZE];
    long long int bytesRead = 0;
    while(!requested_file.eof() && bytesRead < CHUNKSIZE) {
        requested_file.read(buffer, CHUNKSIZE);
        bytesRead += requested_file.gcount();
        // string temp = string(buffer).substr(0, CHUNKSIZE);
        // memset(buffer, '\0', sizeof(buffer));
        // strcpy(buffer, temp.c_str());
        // cout << "sending " << buffer << endl;
        buffer[CHUNKSIZE] = '\0';
        send(clientfd, buffer, strlen(buffer) + 1, 0); //send chunk
        memset(buffer, '\0', sizeof(buffer));
        // if(recv(clientfd, dummy_buffer, sizeof(dummy_buffer), 0) > 0) {
        //     // bool success = get_int(strtok(buffer, ":"));
        //     cout << "ack received " << endl;
        //     // if(!success) { 
        //     //     requested_file.seekg(bytesRead - requested_file.gcount());
        //     //     bytesRead -= requested_file.gcount();
        //     // }
        // }
    }
    // cout << "sending last chunk" << endl;
    // memset(buffer, 0, CHUNKSIZE + 10);
    // string end_chunk = to_string(0) + ":abc";
    // cout << end_chunk << endl;
    // strcpy(buffer, end_chunk.c_str());
    // cout << buffer << endl;
    // send(clientfd, buffer, strlen(buffer) + 1, 0);
}

void add_peer_to_group(char* t_resp, char* t_group, char* t_peer_port) {
    sockaddr_in server_address;
    int socketfd;
    
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(g_tracker_port); //host to network short

    if(inet_pton(AF_INET, LOCALHOST, &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    print_message(string("Trying to connect with " + string(LOCALHOST) + ": " + to_string(g_tracker_port)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }
    print_message("connected successfully to the tracker");
    string msg = "apg:" + string(t_group) + ":" + string(t_peer_port);

    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());


    send(socketfd, buffer, strlen(buffer), 0);

    bzero(t_resp, MSGSIZE);
    int rs;

    rs = recv(socketfd, t_resp, MSGSIZE, 0);

    if(rs < 0) {
        print_err_and_exit("error in joining group");
    }
    close(socketfd);
    return;
}

void* serve_requests(void* arg) {
    // cout << endl << "running hi function " << endl;
    int clientfd = *((int *)arg);

    char recv_buffer[MSGSIZE];
    while(recv(clientfd, recv_buffer, sizeof(recv_buffer), 0) > 0) {
        cout << recv_buffer << endl;
        vector<char*> message = parse_message(recv_buffer);
        // cout << message.size() << endl;
        int index = get_abb_index(message[0]);
        switch(index) {
            // case 1: cout << "requesting file info for " << message[1] << endl;
            //         cout << "sending file info" << endl;
            //         send_file_info(clientfd, message[1]);
            //         // ifstream temp("abc.txt", ios::binary);
            //         // char buffer[10];
            //         // while(!temp.eof()) {
            //         //     temp.read(buffer, 10);
            //         //     cout << temp.gcount() << " read and size of buffer = " << sizeof(buffer) << endl;
            //         // }
            //         break;
            case 2: cout << "requesting chunk number " << message[2] << " for file " << message[1] << endl;
                    chunk_info_server ci;
                    ci.index = get_int(message[2]);
                    ci.file_name = message[1];
                    ci.clientfd = clientfd;
                    send_chunks((void*)&ci);
                    break;
            case 3: {
                        cout << "requesting to join group " << message[1] << endl;
                        char ch = 'y';
                        cout << "allow port " << message[2] << " to join group " << message[1] << "? " << endl;
                        // cin >> ch;
                        char resp[MSGSIZE];
                        cout << ch << endl;
                        if(ch == 'Y' || ch == 'y') {
                            cout << "calling add peer to group" << endl;
                            add_peer_to_group(resp, message[1], message[2]);
                            cout << "received response in owner = " << resp << endl;
                            vector<char*> response_components = parse_message(resp);
                            cout << response_components.size() << endl;
                            if(strcmp(response_components[0], "Success") == 0) {
                                cout << "Port " << message[2] << "succesfully added to group " << message[1] << endl;
                                string msg = "Success:" + to_string(g_port);
                                char buffer[MSGSIZE];
                                strcpy(buffer, msg.c_str());

                                send(clientfd, buffer, strlen(buffer) + 1, 0);
                                break;
                            }
                        }
                        string msg = "Failure";
                        char buffer[MSGSIZE];
                        strcpy(buffer, msg.c_str());

                        send(clientfd, buffer, strlen(buffer) + 1, 0);
                        break;
            } 

        }
    } 
    cout << "exiting listener thread" << endl;
    pthread_exit(0);
}



int main(int argc, char* argv[]) {
    g_port = atoi(argv[1]);
    get_tracker_info(argv[2]);
    cout << "tracker port number " << g_tracker_port << endl;

    pthread_t t_id1;
	if(pthread_create(&t_id1, NULL, client_thread, NULL)!= 0) 
	{
		cout<<"Failed to create client thread\n";
	} 
    // ifstream file("input.txt", ios::binary);
    
    // int file_size = get_file_size(file);
    // long long int num_chunks = file_size / CHUNKSIZE + 1;

    // vector<bool> chunks(num_chunks, true);
    // file_info f = {
    //     num_chunks,
    //     file_size,
    //     chunks,
    //     "input.txt"
    // };

    // char buffer[CHUNKSIZE + 10];
    // file.read(buffer, CHUNKSIZE);
    // // buffer[CHUNKSIZE] = '\0';
    // cout << buffer << endl;
    // cout << "num of chunks = " << f.chunks << endl;
    
    // file_info_map["input.txt"] = f;
    // cout << file_info_map["input.txt"].file_name << endl;

    // string s = "input.txt";
    // const char* fileName = s.c_str();
    // cout << file_info_map[fileName].file_name << endl;
    // ------------------------------------------------------Peer as a server----------------------------------------
    int socketfd; // file descriptor to store socket id
    struct sockaddr_in server_address;
    int QUEUE_SIZE = 5;

    socketfd = socket(AF_INET, SOCK_STREAM, 0); // create socket
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");
    
    // set server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(g_port); //host to network short
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind server to port
    if(bind(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("Unable to bind");
    }
    print_message("Binding successful");
    // start listening
    if(listen(socketfd, QUEUE_SIZE) < 0) print_err_and_exit("listen error.");
    print_message((string("listening on port ") + to_string(g_port) + string("...")).c_str());

    sockaddr_in client_address; // address of client, initially empty
    vector<pthread_t> t_ids;
    while(1) {
        int client_address_length = sizeof(client_address);
        print_message("Waiting for connection");
        fflush(stdout);

        int clientfd = accept(socketfd, (sockaddr*)&client_address, (socklen_t*)&client_address_length);

        pthread_t t_id;
        t_ids.push_back(t_id);
        int size = t_ids.size();
        cout << "new connection from: " << inet_ntoa(client_address.sin_addr) << " : " << ntohs(client_address.sin_port);

        

        pthread_create(&(t_ids.at(t_ids.size() - 1)), NULL, serve_requests, &clientfd);

    }

    for(int i = 0; i < t_ids.size(); i++) {
        pthread_join(t_ids[i], NULL); // blocking call, waits till the thread is completed, but we want parellel threads, hence outside above for loop.
    }

    return 0;
}