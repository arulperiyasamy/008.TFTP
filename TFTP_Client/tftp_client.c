#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio_ext.h>
#include <netinet/in.h>  // For sockaddr_in
#include <sys/socket.h>  // For socket functions and sockaddr
#include <arpa/inet.h>
#include "tftp.h"
#include "tftp_client.h"

// setting default mode
Mode mode = NORMAL;

int main() 
{
	char command[256];
	tftp_client_t client;
	memset(&client, 0, sizeof(client));  // Initialize client structure

	// Main loop for command-line interface
	while (1) 
	{
		printf("\x1b[31mtftp>\x1b[0m ");
		__fpurge(stdin);
		fgets(command, sizeof(command), stdin);

		// Remove newline character
		command[strcspn(command, "\n")] = 0;

		// Process the command
		process_command(&client, command);
	}
	return 0;
}

// Function to process commands
void process_command(tftp_client_t *client, char *input_str) 
{

	int port = 6969;

	//	char command[20];
	//	char ip_str[16];
	char* cmd = strtok(input_str, " ");
	char* opt = strtok(NULL, " ");

	if(!strcmp("connect", cmd))
	{
		if(opt == NULL)
		{
			printf("No options[ipv4] passed for connect!\n");
		}

		else
		{
			if(isvalid_ipv4(opt))
			{
				printf("IP Validated!!\n");
				connect_to_server(client, opt, port);
			}
			else
			{
				printf("Invalid IP!\n");
			}
		}
	}
	else if(!strcmp("get", cmd))
	{
		if(opt == NULL)
		{
			printf("No filename passed!\n");
		}
		else
		{
			get_file(client, opt);
		}
	}
	else if(!strcmp("put", cmd))
	{
		if(opt == NULL)
		{   
			printf("No filename passed!\n");
		}   
		else
		{
			put_file(client, opt);
		}	
	}
	else if(!strcmp("chmode", cmd))
	{
		if(opt == NULL)
		{   
			printf("No options[mode] passed!\n");
		}   
		else
		{
			ch_mode( opt);
		}

	}
	else if(!strcmp("ckmode", cmd) || !strcmp("mode", cmd))
	{
		printf("Current file transfer mode: %s\n", mode_strings[mode]);
	}
	else if(!strcmp("exit", cmd) )
	{
		_exit(0);
	}
	else
	{
		printf("Invalid command!\n");
	}
}

void create_socket(tftp_client_t *client)
{
	/* Create UDP socket */
	if( ( (client -> sock_fd) = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		printf("Error: Could not create socket\n");
	}
}

// This function is to initialize socket with given server IP, no packets sent to server in this function
void connect_to_server(tftp_client_t *client, char *ip, int port) 
{
	/* Create UDP socket */
	create_socket(client);

	if( (client -> sock_fd)  < 0)
		return ;
	else
		printf("Socket created...\n");

	/* Set socket timeout option */

	/* Set up server address */
	client->server_addr.sin_family = AF_INET;
	client->server_addr.sin_port = htons(port); 
	client->server_addr.sin_addr.s_addr = inet_addr(ip);
	printf("Server address set...\n");

	/* Set up connection packet */
	tftp_packet packet;
	memset(&packet, 0, sizeof(packet));
	packet.opcode = htons(CONN);	// Set opcode to connect	
	printf("Connection packet set...\n");

	/* Send packet to server */
	sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
	printf("Packet sent...\n");	

	/* Set up recieve packet */
	tftp_packet recv_packet;
	socklen_t addr_len = sizeof(client->server_addr);
	printf("Recieve packet set...\n");

	/* Recieve ack from server */
	recvfrom((client -> sock_fd), &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	printf("Recieved Packet from server...\n");

	/* Ensure ACK is recieved */	
	if(ntohs(recv_packet.opcode) == ACK)
		printf("Server Available!\n\n");

	return;
}

void get_file(tftp_client_t *client, char *filename) 
{

	int fd;
	fd = open(filename, O_CREAT | O_EXCL | O_WRONLY);
	
	if(fd == -1 && (errno != EEXIST))
	{
		printf("Error in file opening!\n");
		return;
	}


	/* Set up RRQ packet */
	tftp_packet rrq_packet;
	memset(&rrq_packet, 0, sizeof(rrq_packet));
	rrq_packet.opcode = htons(RRQ);    // Set opcode to Read request        
	strcpy(rrq_packet.body.request.filename, filename);
	rrq_packet.body.request.mode = mode;
	printf("Read request packet set...\n");

	/* Send packet to server */
	sendto((client -> sock_fd), &rrq_packet, sizeof(rrq_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
	printf("Packet sent...\n");    

	socklen_t addr_len = sizeof(client->server_addr);
	/* Set up error packet to recieve */
	tftp_packet err_packet;
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);    // Set opcode to ERROR

	/* Recieve error packet */
	recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	printf("ERROR packet recieved from server\n\n");
	printf("**%s**\n", err_packet.body.error_packet.error_msg);
	if(err_packet.body.error_packet.error_code == FILE_NOT_FOUND)
		return;

	char choice;	
	printf("Data Transfer Mode: %s\n", mode_strings[mode]);
	printf("Do you want to continue to recieve data? Y/y\n");
	scanf(" %c", &choice);

	/* Set up connection packet */
	tftp_packet packet;
	memset(&packet, 0, sizeof(packet));
	printf("Confirmation packet set...\n");

	if(choice == 'Y' || choice == 'y')
	{
		if( fd == -1 && (errno == EEXIST) )
		{
			printf("Conflict with existing file names\n");

			char ch;
			printf("Do you want to override existing file: Y/y\n");
			scanf(" %c", &ch);
			if( ch == 'Y' || ch == 'y' )
			{
				fd = open(filename, O_TRUNC | O_WRONLY );
			}
			else
			{
				printf("File transfer aborted\n");
				return;
			}
		}



		printf("Client proceeded for transmission\n");
		packet.opcode = htons(CONN);    // Set opcode to connect 
		sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
		printf("Confirmation sent...\n");

		receive_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
	}
	else
	{
		packet.opcode = htons(ERROR);    // Set opcode to connect        
		sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
		printf("Transmission Aborted\n");
	}


}
void put_file(tftp_client_t *client, char *filename) 
{
	int fd; 
	fd = open(filename, O_RDONLY);
	if(fd < 0)
	{
		printf("File not availabe\n");
		return;
	}

	/* Set up RRQ packet */
	tftp_packet wrq_packet;
	memset(&wrq_packet, 0, sizeof(wrq_packet));
	wrq_packet.opcode = htons(WRQ);    // Set opcode to Write request        
	strcpy(wrq_packet.body.request.filename, filename);
	wrq_packet.body.request.mode = mode;
	printf("Read request packet set...\n");

	/* Send packet to server */
	sendto((client -> sock_fd), &wrq_packet, sizeof(wrq_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
	printf("Packet sent...\n");    

	socklen_t addr_len = sizeof(client->server_addr);
	/* Set up error packet to recieve */
	tftp_packet err_packet;
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);    // Set opcode to ERROR

	/* Recieve error packet */
	recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	printf("ERROR packet recieved from server\n\n");
	printf("**%s**\n", err_packet.body.error_packet.error_msg);
	if(err_packet.body.error_packet.error_code == FOPEN_ERROR)
	{
		printf("File Transfer Failed\n");
		printf("%s\n",err_packet.body.error_packet.error_msg);
	}
	else if(err_packet.body.error_packet.error_code == F_CONFLICT)
	{
		tftp_packet err_packet;

		printf("%s\n",err_packet.body.error_packet.error_msg);
		tftp_packet ack_packet;
		memset(&ack_packet, 0, sizeof(ack_packet));

		char ch;
		printf("Do you want to override the file in server Y/y:\n");
		scanf(" %c", &ch);
		if(ch == 'y' || ch == 'Y')
		{
			ack_packet.body.ack_packet.block_number = O_RIDE_OK;
		}
		else
		{
			ack_packet.body.ack_packet.block_number = O_RIDE_NO;
		}
		sendto((client -> sock_fd), &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
		printf("Client choice sent to server...\n");

		if(ch == 'y' || ch == 'Y')
		{	
			memset(&err_packet, 0, sizeof(err_packet));
			err_packet.opcode = htons(ERROR);    // Set opcode to ERROR
			recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
			printf("ERROR packet recieved from server\n\n");
			printf("**%s**\n", err_packet.body.error_packet.error_msg);

			if(err_packet.body.error_packet.error_code != FOPEN_ERROR)
			{
				send_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
			}
		}
	}
	else if( err_packet.body.error_packet.error_code == FOPEN_SUCCESS )
	{
		send_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
	}
}

void ch_mode(char* mode_str)
{
	if(!strcmp(mode_str, "normal"))
		mode = NORMAL;
	else if(!strcmp(mode_str, "octet"))
		mode = OCTET;
	else if(!strcmp(mode_str, "netascii"))
		mode = NET_ASCII;
	else
		printf("Tnvalid option passed to chmode!\n");
}



void disconnect(tftp_client_t *client) 
{
	// close fd

}
void send_request(int sockfd, struct sockaddr_in server_addr, char *filename, int opcode)
{

}

void receive_request(int sockfd, struct sockaddr_in server_addr, char *filename, int opcode)
{

}

int isvalid_ipv4(char* ip_str)
{
	int i = 0;
	int dot_count = 0;
	while(ip_str[i])
	{
		if( ip_str[i] == '.' )
		{
			/* Return 0 if two dots are consequent */
			if( ip_str[i+1] == '.' )
				return 0;	
			dot_count++;
		}
		i++;
	}
	/* Return 0 if dot count < 3(minimum no. in ipv4) */
	if( dot_count < 3 )
		return 0;

	return 1;
}
