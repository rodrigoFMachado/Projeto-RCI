#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <sys/socket.h>

extern int fd_udp;

typedef struct processed_command_ processed_command; // Estrutura para armazenar o comando processado e seus argumentos

void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

struct addrinfo *udp_starter(char *regIP, char *regUDP); 

void send_udp_message(char *arg, char *myIP, char *myTCP, int net, int id, struct addrinfo *res_udp); // mais args posteriormente

int word_processor(processed_command *arguments); // função auxiliar para processar palavras de uma string

#endif