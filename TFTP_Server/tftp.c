/* Common file for server & client */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "tftp.h"

const char mode_strings[3][20] = {
	"NORMAL",
	"OCTET",
	"NET_ASCII"
};

void send_file(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode) 
{
	tftp_packet data;memset(&data, 0, sizeof(data));
	data.opcode = DATA;

	tftp_packet ack;memset(&ack, 0, sizeof(ack));
	ack.opcode = ACK;

	tftp_packet err;memset(&err, 0, sizeof(err));
	err.opcode = ERROR;

	int bytes_read;
	int is_recieved;
	is_recieved = 1;

	if(mode == NORMAL)
	{	
		bytes_read = 512;
		while(1)
		{
			if(is_recieved)
			{
				bytes_read = read(fd, data.body.data_packet.data, 512);
			}
			data.body.data_packet.size = bytes_read;
			sendto(sock_fd, &data, sizeof(data), 0, (struct sockaddr *) &client_addr, client_len);
			recvfrom(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, &client_len);	
			if(ack.body.ack_packet.block_number == -1)
			{
				printf("Packet not recieved correctly\n");
				is_recieved = 0;
			}
			else
			{
				printf("ACK for Block: [%d] recieved [Transfer Success]\n",data.body.data_packet.block_number);
				if(bytes_read < 512 )
				{
					printf("Sent last packet\n");
					break;
				}
				is_recieved = 1;
				data.body.data_packet.block_number++;
			}
		}
	}

	else if(mode == NET_ASCII)
	{
		char ch;
		char buffer[512];
		int index = 0;
		while(read(fd, &ch, 1) > 0) 
		{
			if (ch == '\n') {
				buffer[index++] = '\r';
				buffer[index++] = '\n';
			}
			else
				buffer[index++] = ch;

			if(index == 512)
			{
				strcpy(data.body.data_packet.data, buffer);
RESEND:				sendto(sock_fd, &data, sizeof(data), 0, (struct sockaddr *) &client_addr, client_len);
				recvfrom(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, &client_len);

				if(ack.body.ack_packet.block_number == -1)
				{
					printf("Packet not recieved correctly\n");
					goto RESEND;
				}
				else
				{
					printf("ACK for Block: [%d] recieved [Transfer Success]\n",data.body.data_packet.block_number);
					index = 0;
					data.body.data_packet.block_number++;
				}
			}				
		}
	}

	else if(mode == OCTET)
	{
		while(1)
		{
				if (is_recieved)
					bytes_read = read(fd, data.body.data_packet.data, 1);
				
				data.body.data_packet.size = bytes_read;
				sendto(sock_fd, &data, sizeof(data), 0, (struct sockaddr *)&client_addr, client_len);

				if (bytes_read == 0)  // EOF
					break;

				recvfrom(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&client_addr, &client_len);

				if (ack.body.ack_packet.block_number == -1)
				{
					printf("Packet not received correctly\n");
					is_recieved = 0;
				}
				else
				{
					printf("ACK for Block: [%d] recieved [Transfer Success]\n",data.body.data_packet.block_number);
					data.body.data_packet.block_number++;
					is_recieved = 1;
				}

		}
		strcpy(err.body.error_packet.error_msg, "File Reacned End\n");
		sendto(sock_fd, &err, sizeof(err), 0, (struct sockaddr *) &client_addr, client_len);
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
				printf("File Recieved\n");
				break;
			}
		}
	}

	else if(mode == OCTET)
	{
		while(1)
		{
			bytes_received = recvfrom(sock_fd, &data, sizeof(data), 0, (struct sockaddr *)&client_addr, &client_len);
			if(data.body.data_packet.size == 1)
			{
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
			}
			else if(data.body.data_packet.size == 0)
			{
				printf("File Recieved\n");
				break;
			}
		}
	}

}
