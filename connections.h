#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <sys/socket.h>

extern int fd_udp;


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

struct addrinfo *udp_starter(); 

void handle_udp_message(char *message, int net, int id); // mais args posteriormente



#endif