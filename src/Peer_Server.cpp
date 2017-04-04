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
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#define BUF_LEN 1024

using namespace std;

struct sockaddr_storage server_storage;
socklen_t addr_size = sizeof(struct sockaddr_storage);

fd_set read_fd_set;
fd_set tmp_fd_set;
int server_socket, peer_socket;

char buffer[BUF_LEN];
sockaddr_in socket_address;


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
 * Peer server main function
 */
int main(void)
{
    server_socket = socket(PF_INET, SOCK_STREAM, 0);

    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(12002);
    socket_address.sin_addr.s_addr = INADDR_ANY;
    memset(socket_address.sin_zero, '\0', sizeof socket_address.sin_zero);

    // Set socket options
    int set_option_value = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*) &set_option_value, sizeof(set_option_value));

    if(bind(server_socket, (struct sockaddr*) &socket_address, sizeof(socket_address)) == -1)
        error("binding");

    // Start server listening
    if(listen(server_socket, 5) != 0)
        error("listen");

    cout << "Server listening ...\n";

    FD_ZERO(&read_fd_set);
    FD_SET(server_socket, &read_fd_set);
    int maxfd = server_socket;

    while(true)
    {
        timeval time_st;
        memcpy(&tmp_fd_set, &read_fd_set, sizeof(tmp_fd_set));

        // Timeval struct to check timeouts
        time_st.tv_sec = 1000;
        time_st.tv_usec = 0;
        int result = select(maxfd + 1, &tmp_fd_set, NULL, NULL, &time_st);

        // Handle timeouts and errors
        if (result == 0)
            cout << "Timeout. Please try again\n";
        else if (result <= -1)
            error("select");

        else if (result > 0)
        {
            if (FD_ISSET(server_socket, &tmp_fd_set))
            {
                peer_socket = accept(server_socket, (struct sockaddr*) &server_storage, &addr_size);

                if (peer_socket >= 0)
                {
                    // Update maxfd to new maximum value if changed
                    FD_SET(peer_socket, &read_fd_set);
                    if (maxfd < peer_socket)
                        maxfd = peer_socket;
                }
                else
                    error("accept");

                FD_CLR(server_socket, &tmp_fd_set);
            }

            for (int i = 0; i < maxfd + 1; i++)
            {
                if (FD_ISSET(i, &tmp_fd_set))
                {
                    result = recv(i, buffer, BUF_LEN, 0);
                    string file_name_str(buffer,result);

                    // Handle timeouts and errors
                    if (result == 0)
                        cout << "Receive timed out\n";
                    else if (result < 0)
                        error("recv");

                    if(result > 0)
                    {
                        buffer[result] = 0;
                        cout << "Opening " << buffer << "\n";

                        FILE* request_file = fopen(file_name_str.c_str(), "rb");

                        // Get requested file size from stats
                        struct stat request_file_stats;
                        stat(file_name_str.c_str(), &request_file_stats);
                        sprintf(buffer, "%d", (int) request_file_stats.st_size);

                        // Iterate over entire file till end is found
                        int result_bytes_read;
                        while(!feof(request_file))
                        {
                            result_bytes_read = fread(buffer, 1, BUF_LEN, request_file);
                            send(i, buffer, result_bytes_read, 0);
                            cout << "Sent bytes: " << result_bytes_read << "\n";
                        }

                        fclose(request_file);
                        cout << "File closed\n\n";

                        // Handle file read errors
                        if (result_bytes_read == -1 && errno != EINTR)
                            error("file read");

                        cout << "File sent_bytes: " << result_bytes_read << endl;
                        close(i);
                        FD_CLR(i, &read_fd_set);
                    }
                }
            }
        }
    }
}
