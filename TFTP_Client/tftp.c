/* Common file for server & client */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "tftp.h"

/* Strings used for printing prompts */
const char mode_strings[3][20] = 
{
	"NORMAL",
	"OCTET",
	"NET_ASCII"
};

/* This function send file to requested device server ---> client (or) client ---> server */
/* Trasmission follows ftp_mode requested by device */
void send_file(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode) 
{
	int block_no = 0;
	/* If requested ftp mode is normal */
	if(mode == NORMAL)
	{	
		int bytes_read;
		char buffer[512];
		while(1)
		{
			/* read 512 bytes from file and send */
			bytes_read = read(fd, buffer, 512);	
			send_buffer(sock_fd, client_addr, client_len, buffer, bytes_read, block_no);
			block_no++;	// increment block number for next buffer
			if(bytes_read < 512)
			{
				break;		// break it it is last packet
			}
		}
		printf("\x1b[31m<info>\x1b[0m File sent succesfully\n");
	}

	/* If requested ftp mode is netascii */
	else if(mode == NET_ASCII)
	{
		char ch;		// Variable to store read bytes
		char buffer[512];	// Create a buffer of size 512 bytes
		int index = 0;		// Variable used to tracks the index of buffer 
		int block_no = 0;	// Variable to track block number

		/* Run loop untill eof */
		while(read(fd, &ch, 1) > 0) 
		{
			/* If \n encountered  replace it with \r\n */
			/* Handle buffer overflow while replacing with \r\n */
			/* Send buffer when it overflows */
			if (ch == '\n') 
			{
				if(index == 512)
				{
					index = 0;
					send_buffer(sock_fd, client_addr, client_len, buffer, 512, block_no);
					block_no++;
				}
				buffer[index++] = '\r';

				if(index == 512)
				{
					index = 0;
					send_buffer(sock_fd, client_addr, client_len, buffer, 512, block_no);
					block_no++;
				}
				buffer[index++] = '\n';
			}
			/* Store ch as it is if it is not '\n' */
			else
				buffer[index++] = ch;

			/* send buffer if it overflows */
			if(index == 512)
			{
				index =0;
				send_buffer(sock_fd, client_addr, client_len, buffer, 512, block_no);
				block_no++;
			}				
		}
		/* Send last buffer */
		send_buffer(sock_fd, client_addr, client_len, buffer, index, block_no);
		printf("\x1b[31m<info>\x1b[0m File sent succesfully\n");
	}

	/* If requested ftp mode is octet */
	else if(mode == OCTET)
	{
		int bytes_read;		// Variable stores bytes read from fd
		char ch;		// Variable used as buffer
		while(1)
		{
			bytes_read = read(fd, &ch, 1);		// Read byte by byte
			send_buffer(sock_fd, client_addr, client_len, &ch, bytes_read, block_no);
			block_no++;				// Increment block number after sending each byte
			/* loop breaks if eof is reached */
			if(bytes_read == 0)
				break;
		}
		printf("\x1b[31m<info>\x1b[0m File sent succesfully\n");
	}
}

/* This function send file to requested device server ---> client (or) client ---> server */
/* Trasmission follows ftp_mode requested by device */
void receive_file(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode) 
{
	/* Set up ack and data packet */
	tftp_packet data; memset(&data, 0, sizeof(data)); data.opcode = htons(DATA);
	tftp_packet ack; memset(&ack, 0, sizeof(ack)); ack.opcode = htons(ACK);

	int bytes_received;
	int is_recieved = 1;
	int max_bytes;

	if (mode == NORMAL || mode == NET_ASCII)
		max_bytes = 512;
	else if (mode == OCTET)
		max_bytes = 1;

	/* Same recive mechanism */
	while(1)
	{
		bytes_received = recvfrom(sock_fd, &data, sizeof(data), 0, (struct sockaddr *)&client_addr, &client_len);
		/* Check if the bytes recived is the size of packet */
		if(bytes_received == sizeof(data))
		{
			/* Send block numver in ack packet if the packet recieved correctly */
			ack.body.ack_packet.block_number = data.body.data_packet.block_number;
			sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
			write(fd, data.body.data_packet.data, data.body.data_packet.size );
		}	
		else
		{
			/* Send ack with -1 if packet not recived corretly */
			ack.body.ack_packet.block_number = -1;
			sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
		}

		/* Break loop is last packet recieved i.e.size < max_bytes */
		if( data.body.data_packet.size < max_bytes )
		{
			printf("\x1b[31m<info>\x1b[0m File Recieved\n");
			break;
		}
	}
}

void send_buffer(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, char* buffer, int size, int block_no)
{
	/* set up data & ack packet*/
	tftp_packet data; memset(&data, 0, sizeof(data)); data.opcode = htons(DATA);
	tftp_packet ack; memset(&ack, 0, sizeof(ack)); ack.opcode = htons(ACK);

	int ack_flag = 0;					// flag used to store ack status init to 0 ensure 1st iteration
	data.body.data_packet.size = size;			// send size of data to be written by buffer in reciver
	data.body.data_packet.block_number = block_no;		// store block number in packet
	strcpy(data.body.data_packet.data, buffer);		// copy buffer to the packet

	/* Loop retransmits untill ack is successful */	
	while(ack_flag == 0)
	{
		/* Send data and recive ack for the particular packet */
		sendto(sock_fd, &data, sizeof(data), 0, (struct sockaddr *) &client_addr, client_len);
		recvfrom(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, &client_len);
		/* If packet not received correctly loop continue */
		if(ack.body.ack_packet.block_number == -1) 
		{   
			printf("Packet not recieved correctly\n");
		}   
		/* If packet recived correctly flag set to 1 thus loop ends (no resend) */
		else
		{
			printf("ACK for Block: [%d] recieved [Transfer Success]\n",data.body.data_packet.block_number);
			ack_flag = 1;
		}
	}
}
