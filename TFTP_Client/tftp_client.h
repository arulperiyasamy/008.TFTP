#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

typedef struct 
{
	int sock_fd;
	struct sockaddr_in server_addr;
	socklen_t server_len;
	char server_ip[INET_ADDRSTRLEN];
	int conn_stat;
} tftp_client_t;

// Function prototypes

int isvalid_ipv4(char* ip_str);
int isvalid_command(char* cmd);

int create_socket(tftp_client_t *client);
void connect_to_server(tftp_client_t *client, char *ip, int port);
void put_file(tftp_client_t *client, char *filename);
void get_file(tftp_client_t *client, char *filename);
void disconnect(tftp_client_t *client);
void ch_mode(char* mode_str);
void display_man();

void process_command(tftp_client_t *client, char *command);

#endif
