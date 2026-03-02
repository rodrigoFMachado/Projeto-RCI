#ifndef CONNECTIONS_H
#define CONNECTIONS_H


#include <netdb.h>


typedef struct processed_command_ processed_command; // Estrutura para armazenar o comando processado e seus argumentos


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

int word_processor(processed_command *arguments); // função auxiliar para processar palavras de uma string

void send_udp_message(char *myIP, char *myTCP, struct addrinfo *address_udp, processed_command *arguments);

struct addrinfo *udp_starter(char *regIP, char *regUDP);

struct addrinfo *tcp_starter(char *myIP, char *myTCP); // função para tratar as mensagens recebidas por TCP


#endif