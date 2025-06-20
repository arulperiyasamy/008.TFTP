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
#include "tftp_server.h"


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

	uint16_t server_port;
        printf("Configure server port: ");
        scanf(" %hu", &server_port);    

	/* Set up server address */
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	printf("Server address set...\n");

	/* Bind the socket */
	if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
	{
		perror("Error: Bind failed");
		return 0;
	}
	printf("Socket bind with server...\n");

	printf("TFTP Server listening on \x1b[32mPORT %d\x1b[0m\n", server_port);

	/* Main loop to handle incoming requests */
	while (1) 
	{
		recvfrom(sock_fd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_len);
		
		handle_client(sock_fd, client_addr, client_len, &packet);
	}

	close(sock_fd);
	return 0;
}

void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet) 
{
	/* Extract the TFTP operation (read or write) from the received packet */
	uint16_t opcode = ntohs(packet -> opcode);

	/* and call send_file or receive_file accordingly */
	if(opcode == CONN)
	{
		printf("\n\x1b[31m<request>\x1b[0m Recieved connection request from client\n");
		respond_connreq(sock_fd, client_addr, client_len);
	}

	else if(opcode == RRQ)
	{
		printf("\n\x1b[31m<request>\x1b[0m Recieved read request from client\n");
		respond_rrq(sock_fd, client_addr, client_len, packet);
	}

	else if(opcode == WRQ)
	{
		printf("\n\x1b[31m<request>\x1b[0m Recieved write request from client\n");
		respond_wrq(sock_fd, client_addr, client_len, packet);
	}
}


void respond_connreq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len)
{
	/* Set up ACK Packet */
	tftp_packet ack_packet;
	memset(&ack_packet, 0, sizeof(ack_packet));
	ack_packet.opcode = htons(ACK);
	printf("Set up ACK Packet...\n");

	/* Send ack packet to client */
	sendto(sock_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *) &client_addr, client_len);
	printf("ACK Packet sent to server...\n");
}

void respond_rrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
	/* Open requested file */
	int fd;
	fd = open(packet -> body.request.filename, O_RDONLY);
	printf("TFTP Mode: %s\n", mode_strings[packet->body.request.mode]);

	/* Setting Error Packet */
	tftp_packet err_packet;		
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);
	
	/* Prompt and send error packet if file not found in server */	
	if(fd == -1)
	{
		printf("Requested file not available\n");
		err_packet.body.error_packet.error_code = htons(FILE_NOT_FOUND);	// Set error code as FILE_NOT_FOUND
		strcpy(err_packet.body.error_packet.error_msg, "Requested file not available in server");
	}
	/* Prompt and send error packet if file found in server */	
	else
	{
		printf("Requested file available\n");
		err_packet.body.error_packet.error_code = htons(FILE_FOUND);		// Set error code as FILE_FOUND
		strcpy(err_packet.body.error_packet.error_msg, "Requested file available in server");
	}
	printf("Error packet set\n");
	sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
	printf("Error Packet sent to client...\n");

	/* Return if file not found in server */
	if(fd == -1)
		return; 

	/* Recieve error packet from client for proceedind file transfer*/
	memset(&err_packet, 0, sizeof(err_packet));
	recvfrom(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *)&client_addr, &client_len);		

	/* Send file if client snet opcode as CONN */
	if(err_packet.opcode == htons(CONN))	
	{
		send_file(sock_fd, client_addr, client_len, fd, packet->body.request.mode);
	}
	else
	{
		printf("File Transfer Stopped by client!\n");
	}
}
void respond_wrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet)
{
	/* Set up error packet */
	tftp_packet err_packet;
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);

	/* Open file for recieving from client */
        int fd;
        fd = open( packet->body.request.filename, O_CREAT | O_EXCL | O_WRONLY, 0644);
       	
	/* Handle file opening error */
	if( fd == -1 && (errno != EEXIST) )
        {
                printf("\x1b[31m<error>\x1b[0m Can't create fd\n");
	        err_packet.body.error_packet.error_code = htons(FOPEN_ERROR);		// send error packet to client with error code FOPEN_ERROR
	        strcpy(err_packet.body.error_packet.error_msg, "Error in creating fd in server"); 
		sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
        }
	/* Wait for client choice to truncate file if file already exist in server */
	else if( fd == -1 && (errno == EEXIST) )
	{
		err_packet.body.error_packet.error_code = htons(F_CONFLICT);		// send error packet to client with error code F_CONFLICT
		strcpy(err_packet.body.error_packet.error_msg, "Similar file exist in server");
		sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);

		/* Set up ack packet */
		tftp_packet ack_packet;
		printf("Waiting for client choice to override file\n");
		recvfrom(sock_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&client_addr, &client_len);		
	
		/* If recieved ack as O_RIDE_NO from client don't proceed transmission */	
		if(ack_packet.body.ack_packet.block_number == ntohl(O_RIDE_NO) )
		{
			printf("Override not approved\n");
			printf("File Transfer Stopped\n");
		}
		/* If recieved ack as O_RIDE_OK from client proceed transmission */
		if(ack_packet.body.ack_packet.block_number == ntohl(O_RIDE_OK) )
		{
			printf("Override approved\n");
			fd = open(packet->body.request.filename, O_TRUNC | O_WRONLY );
			
			/* Send error packet if error occurs in file opening */	
			if(fd == -1)
			{
				err_packet.body.error_packet.error_code = htons(FOPEN_ERROR);
				strcpy(err_packet.body.error_packet.error_msg, "File Truncation failed");
				sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
			}
			/* Proceed for transmission if file opened successfully */
			else
			{
				err_packet.body.error_packet.error_code = htons(FOPEN_SUCCESS);
				strcpy(err_packet.body.error_packet.error_msg, "File Truncation Success");
				sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
			
				receive_file(sock_fd, client_addr, client_len, fd, packet->body.request.mode);
				printf("Override Done!\n");
			}	
		}
	}
	/* Proceed for transmission if no conflict occurs */
	else
	{
		printf("No Conflict!\n");
		err_packet.body.error_packet.error_code = htons(FOPEN_SUCCESS);
		strcpy(err_packet.body.error_packet.error_msg, "Creating fd in server success");
		sendto(sock_fd, &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client_addr, client_len);
		printf("File Transfer Started\n");
        	receive_file(sock_fd, client_addr, client_len, fd, packet->body.request.mode);
	}	
}
