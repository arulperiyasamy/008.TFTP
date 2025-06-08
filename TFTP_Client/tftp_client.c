#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio_ext.h>
#include <netinet/in.h>  // For sockaddr_in
#include <sys/socket.h>  // For socket functions and sockaddr
#include <arpa/inet.h>
#include "tftp.h"
#include "tftp_client.h"

char* mode = "NORMAL";

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
void process_command(tftp_client_t *client, char *command) 
{
	if(!strcmp("connect", command))
	{
		int port = 6969;
		char ip_str[16];
		printf("Enter IP: \n");
		scanf("%[^\n]", ip_str);
		if(isvalid_ipv4(ip_str))
		{
			printf("IP Validated!!\n");
			connect_to_server(client, ip_str, port);
		}
		else
			printf("Invalid IP!\n");
	}
	else if(!strcmp("get", command))
	{
		/* Input file name */
		char f_name[40];
		printf("Enter filename: \n");	
		scanf("%[^\n]", f_name);
	
		/* Calling put_file() */
		get_file(client, f_name);
	}

	else if(!strcmp("exit", command) )
	{
		_exit(0);
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
        /* Set up RRQ packet */
        tftp_packet packet;
        memset(&packet, 0, sizeof(packet));
        packet.opcode = htons(RRQ);    // Set opcode to Read request        
        strcpy(packet.body.request.filename, filename);
        strcpy(packet.body.request.mode, mode);
	printf("Read request packet set...\n");
	
	/* Send packet to server */
        sendto((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
        printf("Packet sent...\n");    
	
	socklen_t addr_len = sizeof(client->server_addr);
	/* Set up error packet to recieve */
	memset(&packet, 0, sizeof(packet));
	packet.opcode = htons(ERROR);    // Set opcode to Read request

	/* Recieve error packet */
	recvfrom((client -> sock_fd), &packet, sizeof(packet), 0, (struct sockaddr *) &client->server_addr, &addr_len);
	printf("**%s**\n", packet.body.error_packet.error_msg);
	printf("ERROR packet recieved from server\n\n");


}

void put_file(tftp_client_t *client, char *filename) 
{
	// Send RRQ and recive file 
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
