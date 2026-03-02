#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <sys/socket.h>


typedef struct processed_command_ processed_command; // Estrutura para armazenar o comando processado e seus argumentos

void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

int word_processor(processed_command *arguments); // função auxiliar para processar palavras de uma string

struct addrinfo *udp_starter(char *regIP, char *regUDP); 

void send_udp_message(char *myIP, char *myTCP, struct addrinfo *res_udp, processed_command *arguments); // mais args posteriorment

void handle_tcp_connection(); // função para tratar as mensagens recebidas por TCP
#endif