#ifndef CONNECTIONS_H
#define CONNECTIONS_H


#include <netdb.h>

#include "helper.h"


typedef struct NodeState_ NodeState; // Estrutura para armazenar o comando processado e seus argumentos


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

void send_udp_message(NodeState *my_node, ParsedCommand *current_command);

struct addrinfo *udp_starter(char *regIP, char *regUDP);

struct addrinfo *tcp_starter(NodeState *my_node); // função para tratar as mensagens recebidas por TCP


#endif