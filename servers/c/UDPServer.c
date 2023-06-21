// Sample network sockets code - UDP Server.
// Written for COMP3331 by Andrew Bennett, October 2019.

#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h> // write(), read(), close()

#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <err.h> // warnx()
#include <termios.h> // Contains POSIX terminal control definitions
#include <termio.h>
#include <linux/serial.h>

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

#define SERVER_IP "0.0.0.0"
#define SERVER_PORT 1337

#define SERIAL_DEVICE "/dev/serial0"

#define WIDTH 112
#define HEIGHT 16

#define BUF_LEN 16

typedef struct {
    bool state;
    bool flip;
    mtx_t mutex;
} DotInfo;

struct connection_info {
    char host[INET_ADDRSTRLEN];
    int port;
};

////////////////////////////////////////////////////////////////////////
// Forward declarations
int recv_handler(void *args);
int draw_handler(void *args);

int serial_open(const char *device, int rate);

// Get a connection_info struct based on the sockaddr we're communicating with.
struct connection_info get_connection_info(struct sockaddr_in *sa);

// Populate a sockaddr_in struct with the specified IP / port to connect to.
void fill_sockaddr(struct sockaddr_in *sa, char *ip, int port);

////////////////////////////////////////////////////////////////////////

DotInfo* dots[WIDTH*HEIGHT];

int server_fd;
int serial_fd;

////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    // Create the server's socket.
    //
    // The first parameter indicates the address family; in particular,
    // `AF_INET` indicates that the underlying network is using IPv4.
    //
    // The second parameter indicates that the socket is of type
    // SOCK_DGRAM, which means it is a UDP socket (rather than a TCP
    // socket, where we use SOCK_STREAM).
    //
    // This returns a file descriptor, which we'll use with our
    // recvfrom functions later.
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Create the sockaddr that the server will use to send data to the
    // client.
    struct sockaddr_in sockaddr_to;
    fill_sockaddr(&sockaddr_to, SERVER_IP, SERVER_PORT);

    // Let the server reuse the port if it was recently closed and is
    // now in TIME_WAIT mode.
    const int so_reuseaddr = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(int));

    printf("Binding UDP socket...\n");
    bind(server_fd, (struct sockaddr *) &sockaddr_to, sizeof(sockaddr_to));

    // Connect to serial port device at custom baudrate
    serial_fd = serial_open(SERIAL_DEVICE, 74880);
    if (serial_fd < 0) {
        warnx("Error %i while opening serial port: %s\n", errno, strerror(errno));
    }

    // Populate DotInfo array
    // Force all pixels to be flipped to "off" on the first display loop
    for (unsigned int row = 0; row < HEIGHT; row++) {
        for (unsigned int column = 0; column < WIDTH; column++) {
            dots[column + WIDTH * row] = malloc(sizeof(DotInfo));
            DotInfo* dot = dots[column + WIDTH * row];

            dot->flip = true;
            dot->state = true;

            mtx_t mutex;
            mtx_init(&mutex, mtx_plain);
            dot->mutex = mutex;
        }
    }

    // Create the threads.
    thrd_t recv_thread;
    thrd_create(&recv_thread, recv_handler, (void *) 0);

    thrd_t draw_thread;
    thrd_create(&draw_thread, draw_handler, (void *) 0);

    while (1) {
        // Equivalent to `sleep(0.1)`
        usleep(100000);
    }

    // This code will never be reached, but assuming there was some way
    // to tell the server to shutdown, this code should happen at that
    // point.

    // Clean up the threads.
    int retval;
    thrd_join(recv_thread, &retval);
    thrd_join(draw_thread, &retval);

    // Close the socket
    close(server_fd);
    if(serial_fd > 0) {
        close(serial_fd);
    }

    return 0;
}

int recv_handler(void *args_) {
    // Array that we'll use to store the data we're receiving.
    char recv_buf[BUF_LEN + 1] = {0};

    // Create the sockaddr that the server will use to receive data from
    // the client.
    struct sockaddr_in sockaddr_from;

    // We need to create a variable to store the length of the sockaddr
    // we're using here, which the recvfrom function can update if it
    // stores a different amount of information in the sockaddr.
    socklen_t from_len = sizeof(sockaddr_from);

    while (1) {
        int num_bytes = recvfrom(server_fd, recv_buf, BUF_LEN, 0, 
          (struct sockaddr *) &sockaddr_from, &from_len);

        // Create a struct with the information about the client
        // (host, port) -- similar to "clientAddress" in the Python.
        struct connection_info client = get_connection_info(&sockaddr_from);

        if(num_bytes != 2)
        {
            warnx("Received package with invalid length from %s:%d\n",
                client.host, client.port);
            continue;
        }

        if((recv_buf[0] & 0x80) == 0 || (recv_buf[1] & 0x80) != 0)
        {
            warnx("Received package with invalid structure from %s:%d\n",
                client.host, client.port);
            continue;
        }

        // Parse column, row, state from message
        unsigned int column = (recv_buf[0] & 0x7F);
        unsigned int row = (recv_buf[1] & 0x0F);
        bool new_state = (recv_buf[1] & 0x10) != 0;

        if(column >= WIDTH || row >= HEIGHT)
        {
            warnx("Received package exceeding allowed positions from %s:%d\n",
                client.host, client.port);
            continue;
        }

        // Acquire a lock on this pixel so it is not accessed by draw thread
        // until we are done changing it
        DotInfo* dot = dots[column + WIDTH * row];
        mtx_lock(&(dot->mutex));
        dot->flip = new_state != dot->state;
        mtx_unlock(&(dot->mutex));

        //printf("Received flipdotflut package from %s:%d\n",
        //        client.host, client.port);
    }

    return EXIT_SUCCESS;
}

int draw_handler(void *args_) {
    char send_buf[2] = {0, 0};

    while (1) {
        for (unsigned int row = 0; row < HEIGHT; row++) {
            for (unsigned int column = 0; column < WIDTH; column++) {
                DotInfo* dot = dots[column + (WIDTH * row)];
                // Acquire a lock on this pixel so it is not changed by the
                // receive thread until we are done handling it
                mtx_lock(&(dot->mutex));
                bool flip = dot->flip;
                bool new_state;
                if(dot->flip) {
                    new_state = !dot->state;
                    dot->state = new_state;
                    dot->flip = false;
                }
                mtx_unlock(&(dot->mutex));

                // We don't need to keep the receive handler waiting 
                // while handling the flip
                if(flip) {
                    if(serial_fd >= 0) {
                        send_buf[0] = (1 << 7) | ((WIDTH-1-column) & 0x7F);
                        send_buf[1] = (new_state ? (1 << 4) : 0) | (row & 0x0F);

                        write(serial_fd, send_buf, sizeof(send_buf));
                    }
                    //printf("%d, %d, %d\n", column, row, dot->state);
                    usleep(215); // Make sure not to exceed ~ 4700 flips/s
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////
// Serial helper functions

static int rate_to_constant(int baudrate) {
#define B(x) case x: return B##x
	switch(baudrate) {
		B(50);     B(75);     B(110);    B(134);    B(150);
		B(200);    B(300);    B(600);    B(1200);   B(1800);
		B(2400);   B(4800);   B(9600);   B(19200);  B(38400);
		B(57600);  B(115200); B(230400); B(460800); B(500000); 
		B(576000); B(921600); B(1000000);B(1152000);B(1500000); 
	default: return 0;
	}
#undef B
}    

/* Open serial port in raw mode, with custom baudrate if necessary */
int serial_open(const char *device, int rate)
{
	struct termios options;
	struct serial_struct serinfo;
	int fd;
	int speed = 0;

	/* Open and configure serial port */
	if ((fd = open(device,O_WRONLY|O_NOCTTY)) == -1)
		return -1;

	speed = rate_to_constant(rate);

	if (speed == 0) {
		/* Custom divisor */
		serinfo.reserved_char[0] = 0;
		if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0)
			return -1;
		serinfo.flags &= ~ASYNC_SPD_MASK;
		serinfo.flags |= ASYNC_SPD_CUST;
		serinfo.custom_divisor = (serinfo.baud_base + (rate / 2)) / rate;
		if (serinfo.custom_divisor < 1) 
			serinfo.custom_divisor = 1;
		if (ioctl(fd, TIOCSSERIAL, &serinfo) < 0)
			return -1;
		if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0)
			return -1;
		if (serinfo.custom_divisor * rate != serinfo.baud_base) {
			warnx("Actual baudrate: %d / %d = %f",
			      serinfo.baud_base, serinfo.custom_divisor,
			      (float)serinfo.baud_base / serinfo.custom_divisor);
		}
	}

	fcntl(fd, F_SETFL, 0);
	tcgetattr(fd, &options);
	cfsetispeed(&options, speed ?: B38400);
	cfsetospeed(&options, speed ?: B38400);
	cfmakeraw(&options);

	options.c_cflag |= (CLOCAL | CREAD); // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    // N81, no handshaking
    options.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    options.c_cflag |= CS8; // 8 bits per byte (most common)
    options.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	options.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)

	if (tcsetattr(fd, TCSANOW, &options) != 0)
		return -1;

	return fd;
}

////////////////////////////////////////////////////////////////////////
// Socket helper functions

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

// Populates a connection_info struct from a `sockaddr_in`.
struct connection_info get_connection_info(struct sockaddr_in *sa) {
    struct connection_info info = {};
    info.port = ntohs(sa->sin_port);
    inet_ntop(sa->sin_family, &(sa->sin_addr), info.host, INET_ADDRSTRLEN);

    return info;
}

////////////////////////////////////////////////////////////////////////
