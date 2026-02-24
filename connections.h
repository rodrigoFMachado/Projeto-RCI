#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <sys/socket.h>

extern int fd_udp;


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

struct addrinfo *udp_starter(char *regIP, char *regUDP); 

void handle_udp_message(char *message, char *myIP, char *myTCP, int net, int id); // mais args posteriormente



#endif