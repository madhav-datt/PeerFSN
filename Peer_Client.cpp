#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <list>
#include <string>

using namespace std;

#define BUF_LEN 1024

list<string> files_list;
vector<string> peer_all_files_list;
string file_server_ip;

char buffer[BUF_LEN];
int file_socket;
unsigned int length;
struct sockaddr_in peer_server, peer_client;
struct hostent* host_name_local;
int num_bytes;
int socket_open;

// Function prototypes
int update_file_list(void);
void connect_fis_server(void);


/**
 * Raise and print perror message and exit with failure
 * @param message
 */
void error(const char* message)
{
    perror(message);
    exit(EXIT_FAILURE);
}


/**
 * Tokenize str into token string with delimiters
 * @param str input string
 * @param tokens output tokens vector
 */
void tokenize(const string &str, vector<string> &tokens)
{
    // Delimiter string for tokenization
    const string delimiter = ":";

    // Skip delimiters and find initial non-delimiter string
    string::size_type last_position = str.find_first_not_of(delimiter, 0);
    string::size_type position = str.find_first_of(delimiter, last_position);

    // Find first token and add to the vector
    // Skip delimiter and find next non-delimiter token
    while (!(string::npos == position && string::npos == last_position))
    {
        tokens.push_back(str.substr(last_position, position - last_position));
        last_position = str.find_first_not_of(delimiter, position);
        position = str.find_first_of(delimiter, last_position);
    }
}


/**
 * Connect to target ip address server and request file
 * Save downloaded file as specified in path
 * @param target_ip of server to download file from
 * @param download_file_name full file name and path for downloading
 * @param save_file_name full file name and path to save downloaded file
 */
void request_download_file(string target_ip, string download_file_name, string save_file_name)
{
    sockaddr_in server_address;
    ssize_t rec_bytes;

    // Set up peer_server parameters to prepare for file download
    file_socket = socket(PF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(12002);
    server_address.sin_addr.s_addr = inet_addr(target_ip.c_str());
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));

    socklen_t server_address_size = sizeof(server_address);

    if (server_address_size >= 0)
        if(connect(file_socket, (struct sockaddr*) &server_address, server_address_size) < 0)
            error("connect socket");

    // Send filename to FIS peer_server for file availability query
    // Get IP address of peer with download_file_name
    char tmp_buffer[BUF_LEN];
    strcpy(tmp_buffer, download_file_name.c_str());
    send(file_socket, tmp_buffer, strlen(tmp_buffer), 0);

    FILE* save_file = fopen(save_file_name.c_str(),"w");
    char buffer[BUF_LEN];

    // Download file peer_client peer_server at target ip address
    while((rec_bytes = (ssize_t) (int) recv(file_socket, buffer, BUF_LEN, MSG_WAITALL)) > 40)
    {
        for(int i = 0; i < rec_bytes; i++)
            putc(buffer[i], save_file);
    }

    fclose(save_file);
    close(file_socket);
    cout << "File successfully saved\n";
}


/**
 * Add files in current directory to list of available files
 */
void current_directory_list(void)
{
    DIR* dir_pointer = opendir(".");
    struct dirent *directory;
    if (dir_pointer != NULL)
    {
        // Find and add names of all files_list in directory
        while ((directory = readdir(dir_pointer)) != NULL)
            if (directory->d_type == DT_REG)
                files_list.push_back(directory->d_name);
        closedir(dir_pointer);
    }
}


/**
 * Request details from FIS server for download_file_name
 * @param download_file_name
 * @return -1 in case of file list read errors
 */
int request_file_details(string download_file_name)
{
    connect_fis_server();

    download_file_name = "REQ" + download_file_name;
    strcpy(buffer,download_file_name.c_str());

    // Set up sendto and revfrom sockets, get download_file_name details
    num_bytes = (int) sendto(socket_open, buffer, strlen(buffer), 0, (const struct sockaddr*) &peer_server, length);
    if (num_bytes == -1)
        error("sendto socket");

    num_bytes = (int) recvfrom(socket_open, buffer, BUF_LEN, 0, (struct sockaddr*) &peer_client, &length);
    if (num_bytes == -1)
        error("recvfrom socket");

    write(1, "ack received: ", 12);
    write(1, buffer, (size_t) num_bytes);

    close(socket_open);

    // Handle download_file_name read errors
    if (buffer != NULL && buffer[0] == '-')
        return -1;
    return 0;
}


/**
 * Initialize and set up peer client
 * Connect with FIS Server
 */
void peer_client_init(void)
{
    connect_fis_server();

    // Build list of file name strings
    string file_name_str;
    list<string>::iterator iter;
    for(iter = files_list.begin(); iter != files_list.end(); iter++)
        file_name_str += *iter + ":";

    file_name_str = "ADD" + file_name_str;
    strcpy(buffer, file_name_str.c_str());

    // Set up sendto and revfrom sockets, get file details
    num_bytes = (int) sendto(socket_open, buffer, strlen(buffer), 0, (const struct sockaddr*) &peer_server, length);
    if (num_bytes == -1)
        error("sendto socket");

    num_bytes = (int) recvfrom(socket_open, buffer, BUF_LEN, 0, (struct sockaddr *)&peer_client, &length);
    if (num_bytes == -1)
        error("recvfrom socket");

    write(1, "ack received: ", 12);
    write(1, buffer, (size_t) num_bytes);

    close(socket_open);
    update_file_list();
}


/**
 * Update available file list from FIS Server
 * @return -1 in case of file list read errors
 */
int update_file_list(void)
{
    connect_fis_server();

    string update_request = "UPD";
    strcpy(buffer, update_request.c_str());

    // Set up sendto and revfrom sockets, get updated file details
    num_bytes = (int) sendto(socket_open, buffer, strlen(buffer), 0, (const struct sockaddr*) &peer_server, length);
    if (num_bytes == -1)
        error("sendto socket");

    num_bytes = (int) recvfrom(socket_open,buffer,BUF_LEN,0,(struct sockaddr *)&peer_client, &length);
    if (num_bytes == -1)
        error("recvfrom socket");

    string tmp_buffer(buffer, (unsigned long) num_bytes);
    vector<string> tokens;
    tokenize(tmp_buffer, tokens);

    // Clear old list to add updated files_list available
    peer_all_files_list.clear();

    vector<string>::iterator iter;
    for (iter = tokens.begin() ; iter != tokens.end(); iter++)
        peer_all_files_list.push_back(*iter);

    // Print available files_list list
    cout << "Available files_list for download:\n";
    for (iter = peer_all_files_list.begin(); iter != peer_all_files_list.end(); iter++)
        cout << ' ' << *iter << "\n";

    close(socket_open);

    // Handle file read errors
    if (buffer != NULL && buffer[0] == '-')
        return -1;
    return 0;
}


/**
 * Set up connection with FIS Server to start processes
 */
void connect_fis_server(void)
{
    // Open new socket
    socket_open = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_open < 0)
        error("socket open");

    // Get present host details to send to FIS peer_server
    host_name_local = gethostbyname("localhost");
    if (host_name_local <= 0)
        error("Host not known");

    bcopy(host_name_local->h_addr, (char *)&peer_server.sin_addr, (size_t) host_name_local->h_length);

    // Set FIS peer_server related global parameters for file access
    peer_server.sin_family = AF_INET;
    peer_server.sin_addr.s_addr = inet_addr(file_server_ip.c_str());
    peer_server.sin_port = htons(12000);
    length = sizeof(struct sockaddr_in);
}


/**
 * Peer client main function
 */
int main(void)
{
    current_directory_list();
    cout << "Please enter File Server's IP: ";
    cin >> file_server_ip;
    cout << endl;
    peer_client_init();

    // Exit loop if input is neither 1 or 2
    int input = 1;
    while (input == 1 || input == 2)
    {
        cout << "Select action\n1. View file list\n2. Download file\n3. Exit\n";
        cin >> input;

        if (input == 1)
        {
            peer_client_init();
            update_file_list();
        }
        else if (input == 2)
        {
            string download_file_name, save_file_name;

            cout << "File name: ";
            cin >> download_file_name;

            // Check if file exists
            if (request_file_details(download_file_name) < 0)
            {
                cout << "File does not exist. Please select peer_client available files_list.\n";
                continue;
            }

            cout << "\nSave downloaded file as: ";
            cin >> save_file_name;
            string ip(buffer, (unsigned long) num_bytes);
            request_download_file(ip, download_file_name, save_file_name);
        }
    }

    return 0;
}
