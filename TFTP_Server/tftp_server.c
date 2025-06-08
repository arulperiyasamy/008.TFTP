#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tftp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define SERVER_PORT 6969

void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
void respond_connreq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len);
void respond_rrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
void respond_wrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len);

int main() 
{
	int sock_fd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);
	tftp_packet packet;

	/* Create UDP socket */
	if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("Error: Could not create socket\n");
		return 1;
	}
	printf("Socket created...\n");

	// Set socket timeout option
	//TODO Use setsockopt() to set timeout option

	// Set up server address
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	printf("Server address set...\n");

	// Bind the socket
	if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
	{
		perror("Error: Bind failed");
		return 0;
	}
	printf("Socket bind with server...\n");

	printf("TFTP Server listening on port %d...\n\n", PORT);

	// Main loop to handle incoming requests
	while (1) 
	{
		int n = recvfrom(sock_fd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_len);
		if (n < 0) 
		{
			perror("Receive failed or timeout occurred");
			continue;
		}
		handle_client(sock_fd, client_addr, client_len, &packet);
	}

	close(sock_fd);
	return 0;
}

void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet) 
{
	// Extract the TFTP operation (read or write) from the received packet
	uint16_t opcode = ntohs(packet -> opcode);

	// and call send_file or receive_file accordingly
	if(opcode == CONN)
	{
		printf("Recived connection request from client...\n");
		respond_connreq(sock_fd, client_addr, client_len);
	}

	else if(opcode == RRQ)
	{
		printf("Recived read request from client...\n");
		respond_rrq(sock_fd, client_addr, client_len, packet);
	}

	else if(opcode == WRQ)
	{
		printf("Recived write request from client...\n");
		respond_wrq(sock_fd, client_addr, client_len);
	}
}


void respond_connreq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len)
{
	/* Set up ACK Packet */
	tftp_packet ack_packet;
	memset(&ack_packet, 0, sizeof(ack_packet));
	ack_packet.opcode = htons(ACK);
	printf("Set up ACK Packet...\n");

	sendto(sock_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *) &client_addr, client_len);
	printf("ACK Packet sent to server...\n\n");
}
void respond_rrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
	int fd;
	fd = open(packet -> body.request.filename, O_RDONLY);

	if(fd == -1)
	{
		printf("Requested file not found in the directory!\n\n");
		return;
	}
	printf("Requested file found!\n");

	/* Setting Error Packet */
	tftp_packet err_packet;	

	err_packet.opcode = htons(ERROR);
	strcpy(err_packet.body.error_packet.error_msg, "Requested file available in server");
	printf("Error packet set\n");
	sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
	printf("Error Packet sent to client...\n\n");
	

	if( !strcmp("NORMAL", packet->body.request.mode) )
	{
		
	}



	
}
void respond_wrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len)
{
	;
}
