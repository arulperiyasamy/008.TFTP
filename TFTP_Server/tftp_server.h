#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#define SERVER_PORT 7777

void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
void respond_connreq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len);
void respond_rrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
void respond_wrq(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);

#endif
