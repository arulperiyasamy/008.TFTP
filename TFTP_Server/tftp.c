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

	else if(mode == OCTET)
	{
		int bytes_read;
		char ch;
		while(1)
		{
			bytes_read = read(fd, &ch, 1);
			send_buffer(sock_fd, client_addr, client_len, &ch, bytes_read, block_no);
			block_no++;
			if(bytes_read == 0)
				break;
		}
		printf("\x1b[31m<info>\x1b[0m File sent succesfully\n");
	}
}

void receive_file(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode) 
{
	tftp_packet data; memset(&data, 0, sizeof(data));
	tftp_packet ack; memset(&ack, 0, sizeof(ack));
	ack.opcode = ACK;

	int bytes_received = sizeof(data);
	int is_recieved = 1;

	if (mode == NORMAL || mode == NET_ASCII)
	{
		while(1)
		{
			bytes_received = recvfrom(sock_fd, &data, sizeof(data), 0, (struct sockaddr *)&client_addr, &client_len);
			if(bytes_received == sizeof(data))
			{
				ack.body.ack_packet.block_number = data.body.data_packet.block_number;
				sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
				write(fd, data.body.data_packet.data, data.body.data_packet.size );
			}	
			else
			{
				ack.body.ack_packet.block_number = -1;
				sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
			}

			if( data.body.data_packet.size < 512 )
			{
				printf("\x1b[31m<info>\x1b[0m File Recieved\n");
				break;
			}
		}
	}

	else if(mode == OCTET)
	{
		while(1)
		{
			bytes_received = recvfrom(sock_fd, &data, sizeof(data), 0, (struct sockaddr *)&client_addr, &client_len);
			if(bytes_received < sizeof(data))
			{
				ack.body.ack_packet.block_number = -1;
				sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
			}
			else
			{	
				ack.body.ack_packet.block_number = data.body.data_packet.block_number;
				sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, client_len);
				write(fd, data.body.data_packet.data, 1 );
			}

			if(data.body.data_packet.size == 0)
			{
				
				printf("\x1b[31m<info>\x1b[0m File Recieved\n");
				break;
			}
		}
	}

}

void send_buffer(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, char* buffer, int size, int block_no)
{
	tftp_packet data;
        tftp_packet ack;
        memset(&data, 0, sizeof(data));
        memset(&ack, 0, sizeof(ack));
        data.opcode = htons(DATA);
        ack.opcode = htons(ACK);

	int ack_flag = 0;
	data.body.data_packet.size = size;
	data.body.data_packet.block_number = block_no;
	strcpy(data.body.data_packet.data, buffer);

	while(ack_flag == 0)
	{
		sendto(sock_fd, &data, sizeof(data), 0, (struct sockaddr *) &client_addr, client_len);
		recvfrom(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, &client_len);
		if(ack.body.ack_packet.block_number == -1) 
		{   
			printf("Packet not recieved correctly\n");
		}   
		else
		{
			printf("ACK for Block: [%d] recieved [Transfer Success]\n",data.body.data_packet.block_number);
			ack_flag = 1;
		}
	}
}
