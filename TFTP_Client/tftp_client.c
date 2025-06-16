#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio_ext.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tftp.h"
#include "tftp_client.h"

Mode mode = NORMAL;	// setting default mode

int main() 
{
	char command[256];
	tftp_client_t client;
	memset(&client, 0, sizeof(client));	// Initialize client structure

	system("clear");			// Clear screen
	printf("\x1b[31mUse man command for <commands> and <usage>\x1b[0m\n");

	while (1)				// Main loop for command-line interface
	{
		printf("\x1b[32m\ntftp>\x1b[0m ");
		__fpurge(stdin);
		fgets(command, sizeof(command), stdin);

		command[strcspn(command, "\n")] = 0;	// Remove newline character

		process_command(&client, command);	// Process the command
	}
}

/* Function to process commands */
void process_command(tftp_client_t *client, char *input_str) 
{
	char* cmd = strtok(input_str, " ");	// Extract command
	char* opt = strtok(NULL, " ");		// Extract option1
	char* opt2 = strtok(NULL, " ");		// Extract option2

	/* Validate command */
	if(!isvalid_command(cmd))
	{
		printf("Invalid Command!\n");
		printf("Use man command for <commands> and <usage> \n");
		return;
	}

	/* Prevent executing other commands before connection */
	if( (client->conn_stat == 0) && ( strcmp(cmd,"connect")) && (strcmp(cmd, "exit") ) && (strcmp(cmd, "mode")) && (strcmp(cmd, "man" ) ) )
	{
		printf("\x1b[31m<error>\x1b[0m Server not connected yet!\n");
		return;
	}

	/* Validate connect command and call fn to proccess connect command accordingly */
	if(!strcmp("connect", cmd))
	{
		if(opt == NULL || opt2 == NULL)
			printf("\x1b[31m<error>\x1b[0m Too few args. passed for connect!\n");

		else
		{
			if(isvalid_ipv4(opt))
				connect_to_server(client, opt, atoi(opt2));
			else
				printf("\x1b[31m<error>\x1b[0m Invalid IP!\n");
		}
	}

	/* Validate get command and call fn to proccess get command accordingly */
	else if(!strcmp("get", cmd))
	{
		if(opt == NULL)
			printf("\x1b[31m<error>\x1b[0m No filename passed!\n");
		else
			get_file(client, opt);
	}

	/* Validate put command and call fn to proccess put command accordingly */
	else if(!strcmp("put", cmd))
	{
		if(opt == NULL)
			printf("\x1b[31m<error>\x1b[0m No filename passed!\n");
		else
			put_file(client, opt);
	}

	/* Validate chmode command and call fn to proccess chmode command accordingly */
	else if(!strcmp("chmode", cmd))
	{
		if(opt == NULL)
			printf("\x1b[31m<error>\x1b[0m No options[mode] passed!\n");
		else
			ch_mode( opt);
	}

	/* Display current ftp mode if command is mode */
	else if(!strcmp("mode", cmd))
	{
		printf("FTP mode: %s\n", mode_strings[mode]);
	}

	/* Display man page if command is man */
	else if(!strcmp("man", cmd))
	{
		display_man();
	}

	/* Exit client program if command is exit */
	else if(!strcmp("exit", cmd) )
	{
		_exit(0);
	}

	/* Call disconnect function if command is disconnect */
	else if(!strcmp("disconnect", cmd) )
	{
		disconnect(client);
	}
}

int create_socket(tftp_client_t *client)
{
	/* Create UDP socket */
	if( ( (client -> sock_fd) = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		printf("Error: Could not create socket\n");
		return 0;
	}
	return 1;
}

/* This function is to initialize socket with given server IP, no packets sent to server in this function */
void connect_to_server(tftp_client_t *client, char *ip, int port) 
{
	/* Create UDP socket */
	if ( create_socket(client) == 0 )
		return;

	/* Set socket timeout option */
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC; 
        timeout.tv_usec = 0;
	setsockopt(client -> sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	/* Set up server address */
	client->server_addr.sin_family = AF_INET;
	client->server_addr.sin_port = htons(port); 
	client->server_addr.sin_addr.s_addr = inet_addr(ip);

	/* Set up connection packet */
	tftp_packet packet;
	memset(&packet, 0, sizeof(packet));
	packet.opcode = htons(CONN);	// Set opcode to connect	

	/* Send packet to server */
	sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));

	/* Set up recieve packet */
	tftp_packet recv_packet;
	socklen_t addr_len = sizeof(client->server_addr);

	/* Recieve ack from server */
	int n = recvfrom((client -> sock_fd), &recv_packet, sizeof(recv_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	if( n < 0)
	{
		client->conn_stat = 0;		// make connection status if timeout occurs
		perror("\x1b[31m<error>\x1b[0m Receive failed or timeout occurred");
		printf("Check IP or Port number\n");
		return;
	}

	/* Ensure ACK is recieved */	
	if(ntohs(recv_packet.opcode) == ACK)
	{
		printf("\x1b[31m<info>\x1b[0m Server Available!\n");
		client->conn_stat = 1;		// make connection status 1 if ack recieved
	}
	else
		client->conn_stat = 0;		// make connection status 0 if ack not recieved

	return;
}

/* Transfer file Server --->  Client */
void get_file(tftp_client_t *client, char *filename) 
{
	/* Set up RRQ packet */
	tftp_packet rrq_packet;
	memset(&rrq_packet, 0, sizeof(rrq_packet));
	rrq_packet.opcode = htons(RRQ);    // Set opcode to Read request        
	strcpy(rrq_packet.body.request.filename, filename);
	rrq_packet.body.request.mode = mode;

	/* Send packet to server */
	sendto((client -> sock_fd), &rrq_packet, sizeof(rrq_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));

	socklen_t addr_len = sizeof(client->server_addr);
	/* Set up error packet to recieve */
	tftp_packet err_packet;
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);    // Set opcode to ERROR

	/* Recieve error packet */
	recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	printf("**%s**\n", err_packet.body.error_packet.error_msg);
	if(err_packet.body.error_packet.error_code == ntohs(FILE_NOT_FOUND))
		return;
	
	/* Set up connection packet */
	tftp_packet packet;
	memset(&packet, 0, sizeof(packet));
	
	/* Open a file to recieve data */
	int fd; 
        fd = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0644);
    
	/* Prompt and return if error in file opening (error other than pre-existance) */
        if(fd == -1 && (errno != EEXIST))
        {
                printf("\x1b[31m<error>\x1b[0m Error in file opening!\n");
		packet.opcode = htons(ERROR);    // Set opcode to error
		sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
                return;
        }
	/* Ask choice if file conflict occurs */
	/* Terminate file transfer if user choice in no */
	/* Continue file transfer if user choice in yes */
	else if( fd == -1 && (errno == EEXIST) )
	{
		char ch;
		printf("\x1b[31m<file_conflict>\x1b[0m Do you want to override existing file? [Y/y]: ");
		scanf(" %c", &ch);
		if( ch == 'Y' || ch == 'y' )
		{
			fd = open(filename, O_TRUNC | O_WRONLY );	// Terminate existing file (based on user choice)
			packet.opcode = htons(CONN);    		// Set opcode to connect 
		        sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
                	receive_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
		}
		else
		{
			printf("\x1b[31m<error>\x1b[0m File transfer aborted\n");
			packet.opcode = htons(ERROR);    // Set opcode to error        
	       	        sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
			return;
		}
	}

	/* Continue to file transfer if no conflict */
	else
	{
		packet.opcode = htons(CONN);    // Set opcode to connect
		sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
		receive_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
	}
}

/* Transfer file Client ---> Server */
void put_file(tftp_client_t *client, char *filename) 
{
	/* Open a file to recieve data */
	int fd; 
	fd = open(filename, O_RDONLY);
	if(fd < 0)
	{
		printf("\x1b[31m<error>\x1b[0m File not availabe!\n");
		return;
	}

	/* Set up RRQ packet */
	tftp_packet wrq_packet;
	memset(&wrq_packet, 0, sizeof(wrq_packet));
	wrq_packet.opcode = htons(WRQ);    			// Set opcode to Write request        
	strcpy(wrq_packet.body.request.filename, filename);	// Copy filename to be sent to the wrq packet
	wrq_packet.body.request.mode = mode;			// Assign ftp mode to mode variable of packet

	/* Send packet to server */
	sendto((client -> sock_fd), &wrq_packet, sizeof(wrq_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));

	socklen_t addr_len = sizeof(client->server_addr);
	/* Set up error packet to recieve */
	tftp_packet err_packet;
	memset(&err_packet, 0, sizeof(err_packet));
	err_packet.opcode = htons(ERROR);    			// Set opcode to ERROR

	/* Recieve error packet */
	recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);

	/* Prompt error message if error code is FOPEN_ERROR */
	if(err_packet.body.error_packet.error_code == ntohs(FOPEN_ERROR))
	{
		printf("\x1b[31m<error>\x1b[0m File Transfer Failed\n");
		printf("%s\n",err_packet.body.error_packet.error_msg);
	}
	/* Ask user choice if error code is F_CONFLICT */
	else if(err_packet.body.error_packet.error_code == ntohs(F_CONFLICT))
	{
		/* Set up error Packet and Ack Packet */
		tftp_packet err_packet;
		tftp_packet ack_packet;
		memset(&err_packet, 0, sizeof(err_packet));
		memset(&ack_packet, 0, sizeof(ack_packet));
		err_packet.opcode = htons(ERROR); 			   // Set opcode to ERROR
		ack_packet.opcode = htons(ACK); 			   // Set opcode to ACK

		char ch;
		printf("\x1b[31m<file_conflict>\x1b[0m Do you want to override the file in server [Y/y]: ");
		scanf(" %c", &ch);
		if(ch == 'y' || ch == 'Y')
			ack_packet.body.ack_packet.block_number = htonl(O_RIDE_OK);	// Send user choice O_RIDE_OK to server

		else
			ack_packet.body.ack_packet.block_number = htonl(O_RIDE_NO);	// Send user choice O_RIDE_NO to server

		sendto((client -> sock_fd), &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));

		/* Recieve Ack from server about the file opening status */
		if(ch == 'y' || ch == 'Y')
		{	
			recvfrom((client -> sock_fd), &err_packet, sizeof(err_packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);

			/* Send file if file opening is success */
			if(err_packet.body.error_packet.error_code != ntohs(FOPEN_ERROR))
			{
				send_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
			}
			/* Prompt and exit if file opening is failure */
			else
			{
				printf("\x1b[31m<error>\x1b[0m %s\n", err_packet.body.error_packet.error_msg);
			}
		}
	}
	/* Continue to send file if no conflict */
	else if( err_packet.body.error_packet.error_code == ntohs(FOPEN_SUCCESS) )
	{
		send_file(client -> sock_fd, client->server_addr, sizeof(client->server_addr), fd, mode);
	}
}

/* This function assigns mode value according to user input option */
void ch_mode(char* mode_str)
{
	if(!strcmp(mode_str, "normal"))
		mode = NORMAL;
	else if(!strcmp(mode_str, "octet"))
		mode = OCTET;
	else if(!strcmp(mode_str, "netascii"))
		mode = NET_ASCII;
	/* Prompt error if opt is wrong */
	else
	{
		printf("\x1b[31m<error>\x1b[0m Invalid option passed to chmode!\n");
		printf("Valid options: normal, octet, netascii\n");
	}
}

/* This function clears client metadata and close socket */
void disconnect(tftp_client_t *client) 
{
	close( client -> sock_fd );			// close udp socket
	memset(client, 0, sizeof(tftp_client_t));	// clear client metadata
	printf("\x1b[31m<info>\x1b[0m Disconnected from server\n");
}

/* This function validates the ipv4 */
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

/* This function validate the command entered */
int isvalid_command(char* cmd)
{
	if(!strcmp("connect",cmd) || !strcmp("get",cmd) || !strcmp("put",cmd) || !strcmp("mode",cmd) || !strcmp("chmode",cmd) || !strcmp("disconnect",cmd) || !strcmp("man",cmd) || !strcmp("exit",cmd))
		return 1;
	else
		return 0;
}

/* This function displays man page of my tftp client */
void display_man()
{
	printf("\x1b[31m%15s\x1b[0m \t\x1b[31m%-60s\x1b[0m\n","<commands>","<usage>");
	printf("%15s \t%-60s\n","connect","connect <ipv4> <port>");
	printf("%15s \t%-60s\n","get","get <file_name>");
	printf("%15s \t%-60s\n","put","put <file_name>");
	printf("%15s \t%-60s\n","chmode","chmode <ftp_mode> can be normal (or) octet (or) netascii");
	printf("%15s \t%-60s\n","mode","mode");
	printf("%15s \t%-60s\n","disconnect","disconnect");
	printf("%15s \t%-60s\n","exit","exit");
}
