/* Common file for server & client*/

#ifndef TFTP_H
#define TFTP_H

#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

#define PORT 6969
#define BUFFER_SIZE 516  // TFTP data packet size (512 bytes data + 4 bytes header)
#define TIMEOUT_SEC 5    // Timeout in seconds

#pragma pack(push, 1)

// TFTP OpCodes
typedef enum 
{
	CONN = 0,  // Connection i.e.checking server availability
	RRQ = 1,  // Read Request
	WRQ = 2,  // Write Request
	DATA = 3, // Data Packet
	ACK = 4,  // Acknowledgment
	ERROR = 5 // Error Packet
} tftp_opcode;

typedef enum 
{
        NORMAL,
        OCTET,
        NET_ASCII
} Mode;

typedef enum
{
	FILE_NOT_FOUND = 404,
	FILE_FOUND,
	FOPEN_ERROR,
	FOPEN_SUCCESS,
	F_CONFLICT,
	O_RIDE_OK,
	O_RIDE_NO
} Ferror;


// TFTP Packet Structure
typedef struct 
{
	uint16_t opcode; // Operation code (CONN/RRQ/WRQ/DATA/ACK/ERROR)
	union 
	{
		struct
		{
			;
		}connection; // CONNECTION

		struct 
		{
			char filename[256];
			Mode mode;
		} request;  // RRQ and WRQ

		struct 
		{
			uint16_t block_number;
			uint16_t size;
			char data[512];
		} data_packet; // DATA

		struct 
		{
			uint16_t block_number;
		} ack_packet; // ACK

		struct 
		{
			uint16_t error_code;
			char error_msg[512];
		} error_packet; // ERROR
	} body;
} tftp_packet;

extern const char mode_strings[3][20];

void send_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode);
void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, int fd, int mode);

#endif // TFTP_H
