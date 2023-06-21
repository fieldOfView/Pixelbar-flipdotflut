// Sample network sockets code - UDP Client.
// Written for COMP3331 by Andrew Bennett, October 2019.

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*

A good reference for C sockets programming (esp. structs and syscalls):
https://beej.us/guide/bgnet/html/multi/index.html

And for information on C Threads:
https://www.gnu.org/software/libc/manual/html_node/ISO-C-Threads.html

One of the main structs used in this program is the "sockaddr_in" struct.
See: https://beej.us/guide/bgnet/html/multi/ipstructsdata.html#structs

struct sockaddr_in {
    short int          sin_family;  // Address family, AF_INET
    unsigned short int sin_port;    // Port number
    struct in_addr     sin_addr;    // Internet address
    unsigned char      sin_zero[8]; // Same size as struct sockaddr
};

*/

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1337

#define WIDTH 112
#define HEIGHT 16

// Populate a sockaddr_in struct with the specified IP / port to connect to.
void fill_sockaddr(struct sockaddr_in *sa, char *ip, int port);

int main(int argc, char *argv[]) {
    // Seed the random generator.
    srand((unsigned) time(NULL));

    // Array that we'll use to store the data we're sending / receiving.
    char buf[2] = {0, 0};

    // Create the client's socket.
    //
    // The first parameter indicates #the address family; in particular,
    // `AF_INET` indicates that the underlying network is using IPv4.
    //
    // The second parameter indicates that the socket is of type
    // SOCK_DGRAM, which means it is a UDP socket (rather than a TCP
    // socket, where we use SOCK_STREAM).
    //
    // This returns a file descriptor, which we'll use with our sendto /
    // recvfrom functions later.
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Create the sockaddr that the client will use to send data to the
    // server.
    struct sockaddr_in sockaddr_to;
    fill_sockaddr(&sockaddr_to, SERVER_IP, SERVER_PORT);

    // Now that we have a socket and a message, we send the messages
    // through the socket to the server.
    int num_bytes = 0;
    int num_packages = 0;
    for(unsigned int row=0; row<HEIGHT; row++) {
        for(unsigned int column=0; column<WIDTH; column++) {
            int polarity = rand() & 1;
            buf[0] = (1 << 7) | (column & 0x7F);
            buf[1] = polarity << 4 | (row & 0x0F);
            num_bytes += sendto(sock_fd, buf, 2, 0,
                    (struct sockaddr *) &sockaddr_to, sizeof(sockaddr_to));
            num_packages++;
            printf(polarity ? "*": " ");
        }
        printf("\n");
    }

    printf("Sent %d bytes in %d packages\n", num_bytes, num_packages);

    return 0;
}

// Populate a `sockaddr_in` struct with the IP / port to connect to.
void fill_sockaddr(struct sockaddr_in *sa, char *ip, int port) {

    // Set all of the memory in the sockaddr to 0.
    memset(sa, 0, sizeof(struct sockaddr_in));

    // IPv4.
    sa->sin_family = AF_INET;

    // We need to call `htons` (host to network short) to convert the
    // number from the "host" endianness (most likely little endian) to
    // big endian, also known as "network byte order".
    sa->sin_port = htons(port);
    inet_pton(AF_INET, ip, &(sa->sin_addr));
}
