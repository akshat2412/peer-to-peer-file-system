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

using namespace std;

// Global variables
int g_port;
char g_port_Ar[4];
int g_tracker_port;
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
    if(t_msg[0] == 'g' && t_msg[1] == 'f' && t_msg[2] == 'i') {
        return 1;
    }
    if(t_msg[0] == 'g' && t_msg[1] == 'c' && t_msg[2] == 'n') {
        return 2;
    }
    return 3;
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
    return 0;
}

void upload_file_to_tracker(char* t_filename, int t_socketfd) {
    ifstream file(t_filename, ios::binary);
    
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

    string msg = "uf:"+to_string(g_port)+":"+string(t_filename)+":"+to_string(file_size)+":"+to_string(num_chunks);
    char buffer[MSGSIZE];
    strcpy(buffer, msg.c_str());

    send(t_socketfd, buffer, strlen(buffer), 0);
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
    // cout << "trying to send " << msg_string << endl;
    char buffer[MSGSIZE];
    strcpy(buffer, msg_string.c_str());
    send(socketfd, buffer, sizeof(buffer), 0);

    char resp_buffer[CHUNKSIZE];
    int rs;
    while(1) {
        memset(resp_buffer, '\0', CHUNKSIZE);
        // fflush(stdin);
        // fflush(stdout);
        // cout << "memsetted" << endl;
        if((rs = recv(socketfd, resp_buffer, 1000, 0)) > 0) {
            // cout << "received response after sending chunk request" << endl;
            // cout << "received " << rs << endl;
            // cout << "received " << resp_buffer << endl;
            // cout << "received " << strlen(resp_buffer) << endl;
            // char *length = strtok(resp_buffer, ":");
            // char *chunk_buffer = strtok(NULL, ":");
            
            // long long int expected_chunk_length = get_int(length);
            // for (int i = 0; i <resp_components[2] strlen(resp_buffer); i++) {
            //     cout << resp_buffer[i];
            // }
            // cout << endl;
            if(resp_buffer[rs - 1] == '\0') {
                cout << "last bit received for chunk number "<< index << endl;
                write_chunk(filename, index, resp_buffer, rs);
                break; 
            }
            write_chunk(filename, index, resp_buffer, rs);
            memset(resp_buffer, '\0', sizeof(resp_buffer));
            // char dummy_buffer[] = "hello";
            // // strcpy(resp_buffer, ack.c_str());
            // send(socketfd, dummy_buffer, sizeof(dummy_buffer), 0);
        }
    }
    // cout << "out of while loop" << endl;
    // close(socketfd);
    pthread_exit(NULL);
    cout << "thread exited" << endl;
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
            case 1: upload_file_to_tracker(command_components[1], socketfd);
                    break;
            
            case 2: {
                        char file_resp[MSGSIZE];
                        get_file_info_from_tracker(file_resp, command_components[1], socketfd);

                        vector<char*> resp_components = parse_message(file_resp);
                        vector<int> peer_ports = get_peer_ports(resp_components[3]);

                        long long int num_chunks = get_int(resp_components[2]);
                        long long int chunks_downloaded = 0;

                        vector<pthread_t> downloader_threads;
                        vector<chunk_info> chunk_infos;
                        cout << "number of ports = " << peer_ports.size();
                        cout << "number of chunks = " << num_chunks << endl;
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

    char buffer[CHUNKSIZE];
    char dummy_buffer[CHUNKSIZE];
    long long int bytesRead = 0;
    while(!requested_file.eof() && bytesRead < CHUNKSIZE) {
        requested_file.read(buffer, CHUNKSIZE);
        bytesRead += requested_file.gcount();
        // string temp = string(buffer).substr(0, CHUNKSIZE);
        // memset(buffer, '\0', sizeof(buffer));
        // strcpy(buffer, temp.c_str());
        // cout << "sending " << buffer << endl;
        send(clientfd, buffer, CHUNKSIZE, 0); //send chunk
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