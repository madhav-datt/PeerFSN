#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SPLIT_COUNT 3
#define BUF_LEN 1024

using namespace std;

// FIS File Name : IP Address mapping
map<string, string> fis_file_map;

string buffer_to_string(char* buffer, int str_size);


/**
 * Append filename:ip address pair to fis_file_map
 * @param filename string
 * @param ip address string
 */
void append_file(string filename, string ip)
{
    fis_file_map.insert(pair<string, string> (filename,ip));
}


/**
 * @param tmp_string
 * @return type substring split at SPLIT_COUNT
 */
string get_type_string(string tmp_string)
{
    return tmp_string.substr(0, SPLIT_COUNT);
}


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
 * Print FIS file mapping from loaded from file_name
 */
void print_file_map(void)
{
    cout << "File Information System Mapping:\n";
    map<string, string>::iterator iter;
    for (iter = fis_file_map.begin(); iter != fis_file_map.end(); iter++)
        cout << iter->second << " : " << iter->first << "\n";
}


/**
 * Find server IP for file with fis_file_map lookup
 * @param file_buffer
 * @param str_size
 */
void get_ip(char* file_buffer, int str_size)
{
    string file_name = buffer_to_string(file_buffer, str_size);
    cout << "File request received: " << file_name << "\n";
    if (fis_file_map.find(file_name) != fis_file_map.end())
    {
        strcpy(file_buffer, fis_file_map[file_name].c_str());
    }
    else
    {
        cout << "Could not find file_name\n";
        sprintf (file_buffer, "-");
    }
}


/**
 * @param buffer
 * @param str_size string size
 * @return string from converted buffer
 */
string buffer_to_string(char* buffer, int str_size)
{
    string tmp_string(buffer, (unsigned long) str_size);
    return tmp_string.substr(SPLIT_COUNT);
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
 * Build file list from fis_file_map
 * @return output string with file list
 */
string build_file_list(void)
{
    string out_str;
    map<string,string>::iterator iter;

    // Iterate over fis_file_map and build output string
    for (iter = fis_file_map.begin(); iter != fis_file_map.end(); iter++)
        out_str = out_str + iter->first + ":";
    return out_str;
}


/**
 * FIS Server main function
 */
int main(void)
{
    // Build FIS File map from text file
    string filename;
    cout << "Enter filename containing filename-ip map: ";
    cin >> filename;
    FILE *fp = fopen(filename.c_str(), "r");
    char temp1[30];
    char temp2[30];
    while(fscanf(fp, "%s", temp1) == 1)
    {
        fscanf(fp, "%s", temp2);
        append_file(temp1, temp2);
    }
    fclose(fp);

    cout << "\nFile map:\n";
    print_file_map();

    // Define socket address structs
    struct sockaddr_in peer_server;
    struct sockaddr_in peer_client;
    char buffer[BUF_LEN];

    int length = sizeof(peer_server);
    socklen_t peer_client_len = sizeof(struct sockaddr_in);

    // Open new socket
    int open_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (open_socket == -1)
        error("socket");

    bzero(&peer_server, (size_t) length);
    peer_server.sin_family = AF_INET;
    peer_server.sin_addr.s_addr = INADDR_ANY;
    peer_server.sin_port = htons(12000);

    if (bind(open_socket, (struct sockaddr*) &peer_server, (socklen_t) length) == -1)
        error("socket binding");

    while (true)
    {
        int n = recvfrom(open_socket, buffer, 1024, 0, (struct sockaddr*) &peer_client, &peer_client_len);
        if (n == -1)
            error("recvfrom client");

        cout << "Datagram IP: " << inet_ntoa(peer_client.sin_addr) << "\n";
        cout << "Datagram port: " << (int) ntohs(peer_client.sin_port) << "\n";

        string type = get_type_string(buffer);

        // File request type input received
        if (strcmp(type.c_str(), "REQ") == 0)
        {
            cout << "File request\n";
            get_ip(buffer, n);
        }


        // Updated file list requested
        else if (strcmp(type.c_str(), "UPD") == 0)
        {
            cout << "Updated file list request\n";
            string file_list_tmp = build_file_list();
            strcpy(buffer, file_list_tmp.c_str());
        }

        // File list type input received
        else if (strcmp(type.c_str(), "ADD") == 0)
        {
            cout << "File list\n";
            vector<string> tokens_tmp;

            string file_list_tmp = buffer_to_string(buffer, n);
            tokenize(file_list_tmp, tokens_tmp);

            vector<string>::iterator iter;
            for (iter = tokens_tmp.begin(); iter != tokens_tmp.end(); iter++)
                fis_file_map[*iter] = inet_ntoa(peer_client.sin_addr);

            print_file_map();
            strcpy(buffer, "Files added\n");
        }


        // Send to client over open socket
        n = sendto(open_socket, buffer, strlen(buffer), 0, (struct sockaddr*) &peer_client, peer_client_len);
        if (n == -1)
            error("sendto client");
    }
}
