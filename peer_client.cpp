#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include "structures.h"
#include "utility_functions.h"

#define PORT 5000
#define LOG_FILE "log.txt"
#define MSGSIZE 2048
#define CHUNKSIZE 32000

using namespace std;

string LOCALHOST = "127.0.0.1";

ofstream logfile(LOG_FILE);
// message conventions
/*
    Req: get file info      
    gfi:<filename>
    Res: 
    number_of_chunks:file_size:chunk_bit_vector:filename
*/

/*
    Req: get chunk at given index of given file
    gcn:<filename>:<chunk number>
*/

void log_message(ofstream &t_file, const char* msg) {
    t_file << msg << endl;
    return;
}

inline void print_err_and_exit(const char* t_msg) {
    cout << t_msg << endl;
    exit(1);
}

inline void print_message(const char* t_msg) {
    cout << t_msg << endl;
}

file_info get_file_info(int t_socketfd, char* t_filename) {
    string msg = "gfi:";
    msg = msg + t_filename;
    send(t_socketfd, msg.c_str(), sizeof(msg), 0);
    file_info f;
    char buffer[10000];
    int rs;
    if(rs = recv(t_socketfd, &buffer, 10000, 0)) {
        cout << "received " << endl;
        cout << rs << endl;
        cout << buffer << endl;
        vector<char*> components = parse_message(buffer);
        f.chunks = get_int(components[0]);
        f.file_name = components[3];
        f.file_size = get_int(components[1]);

    }
    else {
        cout << "file not received" << endl;
    }

    cout << f.file_name << endl;
    cout << f.file_size << endl;
    cout << f.chunks << endl;
    return f;
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

    if(inet_pton(AF_INET, LOCALHOST.c_str(), &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    // print_message(string("Trying to connect with " + LOCALHOST + ": " + to_string(PORT)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }
    
    // print_message("connected successfully to the server");

    string msg_string = "gcn:" + filename + ":" + to_string(index);
    // cout << "trying to send " << msg_string << endl;
    char buffer[MSGSIZE];
    strcpy(buffer, msg_string.c_str());
    send(socketfd, buffer, sizeof(buffer), 0);

    char resp_buffer[CHUNKSIZE + 10];
    int rs;
    while(1) {
        memset(resp_buffer, '\0', CHUNKSIZE + 10);
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
            // for (int i = 0; i < strlen(resp_buffer); i++) {
            //     cout << resp_buffer[i];
            // }
            // cout << endl;
            if(resp_buffer[rs - 1] == '\0') {
                cout << "last bit received for chunk number "<< index << endl;
                break; 
            }
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

void get_chunk(int server_socket_id, long long int t_chunks, vector<int> &ports) {
    vector<pthread_t> threads(min((int)ports.size(), (int)10));
    vector<chunk_info> chunks_info(min((int)ports.size(), (int)10));
    // vector<pthread_t> threads(2);
    // vector<chunk_info> chunks_info(3);
    long long int chunks_remaining = t_chunks;
    long long int threads_created = 0;
    while(chunks_remaining > 0) {

        for(int i = 0; i < min((long long int)threads.size(), chunks_remaining); i++) {
            chunks_info[i].file_name = "input.txt";
            chunks_info[i].index = t_chunks - chunks_remaining;
            chunks_info[i].port_number = ports[i];
            cout << "creating " << i << "th thread" << endl;
            cout << &chunks_info[i] << endl;
            if(pthread_create(&threads[i], NULL, get_chunk_from_peer, &chunks_info[i]) < 0) {
                print_err_and_exit("couldn't create download thread");
            }
            else {
                threads_created++;
            }
            // pthread_join(threads[i], NULL);
        }
        // cout << "finished creating threads" << endl;
        for(int i = 0; i < threads.size(); i++) {
            // sleep(0.3);
            pthread_join(threads[i], NULL);
            // cout << "waiting for " << i << "th thread to close" << endl;
            // pthread_join(threads[i], NULL);
            // cout << i << "th thread closed" << endl;
        }
        chunks_remaining -= threads_created;
        threads_created = 0;
    }

    return;
}
int main() {
    struct sockaddr_in server_address;
    int socketfd;
    
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0) print_err_and_exit("Couldn't create socket");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT); //host to network short

    if(inet_pton(AF_INET, LOCALHOST.c_str(), &server_address.sin_addr) < 0) print_err_and_exit("Invalid address");

    print_message(string("Trying to connect with " + LOCALHOST + ": " + to_string(PORT)).c_str());
    if(connect(socketfd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        print_err_and_exit("unable to establish connection");
    }
    
    print_message("connected successfully to the server");

    char str[50] = "Just sayin Hi!";

    cin.ignore();
    cout << "sending file request" << endl;
    file_info f = get_file_info(socketfd, "input.txt");
    close(socketfd);

    vector<int> ports;
    ports.push_back(5000);
    ports.push_back(5001);
    ports.push_back(5002);
    cin.ignore();

    get_chunk(socketfd, f.chunks, ports);
    cout << "got all the chunks" << endl;

}