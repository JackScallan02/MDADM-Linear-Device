#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* The client socket descriptor for the connection to the server */
int cli_sd = -1;


/* Attempts to read n (len) bytes from fd; returns true on success and false on failure. */
static bool nread(int fd, int len, uint8_t *buf) {
	
	int counter = 0;

	while (len > 256) {
		if (read(fd, buf + counter, 256) == -1) {
			return false;
		}
		
		len -= 256;
		counter += 256;
	}
	
	
	if (read(fd, buf + counter, len) == -1) {
		return false;
	}
	
	
  	return true;
}


/* Attempts to write n bytes to fd; returns true on success and false on failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
	int counter = 0;
	
	while (len > 256) {
		if (write(fd, buf + counter, 256) == -1) {
			return false;
		}
		len -= 256;
		counter += 256;
	}
	
	
	if (write(fd, buf + counter, len) == -1) {
		return false;
	}
	
	
  	return true;
}





/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK) */

static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {

	uint8_t header[HEADER_LEN];
	if (nread(sd, HEADER_LEN, header) == false) {
		return false;
	}
	
	uint16_t length = 0;
	uint16_t tempRet;
	uint32_t tempOp;
	
	memcpy(&length, header, 2);
	memcpy(&tempOp, header + 2, 4);
	memcpy(&tempRet, header + 6, 2);
	
	length = ntohs(length);
	*ret = ntohs(tempRet);
	*op = ntohl(tempOp);
	
	
	if (HEADER_LEN + 256 == length) { //We need to read the block as well
		if (nread(sd, 256, block) == false) {
			return false;
		}
	}
	
	return true;
}




/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL. */

static bool send_packet(int sd, uint32_t op, uint8_t *block) {

	uint8_t packet[HEADER_LEN + JBOD_BLOCK_SIZE];
	
	uint32_t cmd = op >> 26; //Used to determine if we are writing to block
	uint16_t netLength;
	uint16_t length = 8;
	
	if (cmd == JBOD_WRITE_BLOCK) {
		length += 256;
	}
	
	
	netLength = htons(length);
	op = htonl(op);
	
	memcpy(packet, &netLength, 2);
	memcpy(packet + 2, &op, 4);


	if (cmd == JBOD_WRITE_BLOCK) { //Writes the block to the packet
		memcpy(packet + 8, block, 256);
		if (nwrite(sd, length, packet)) {
			return true;
		}

	} else { //Doesn't write the block to the packet
		if (nwrite(sd, length, packet)) {
			return true;
		}
	}
	
	return false;
	
	
}


/* Attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port. */
bool jbod_connect(const char *ip, uint16_t port) {

	struct sockaddr_in caddr;
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);

	
	if (inet_aton(ip, &(caddr.sin_addr)) != 0) {
	
		int sockfd;
		sockfd = socket(PF_INET, SOCK_STREAM, 0);
		
		if (sockfd == -1) {
			printf("Failed to create socket");
			return false;
		}
		
		cli_sd = sockfd;
		
		if (connect(sockfd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1) {
			printf("Failed to connect");
			return false;
		}

		
		return true;
		
	}
	
	return false;
}




/* Disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd);
	cli_sd = -1;
	
}



/* Sends the JBOD operation to the server and receives and processes the response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
	uint16_t ret = 0;
	if (send_packet(cli_sd, op, block)) {
		if (recv_packet(cli_sd, &op, &ret, block) == false) {
			return -1;
		}
	}
	
	return ret;
}

