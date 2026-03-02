#ifndef CONNECTIONS_H
#define CONNECTIONS_H


#include <netdb.h>


typedef struct NodeState_ NodeState; // Estrutura para armazenar o comando processado e seus argumentos

typedef struct ParsedCommand_ ParsedCommand; // Estrutura para armazenar o comando processado e seus argumentos


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP);

int word_processor(ParsedCommand *current_command); // função auxiliar para processar palavras de uma string

void send_udp_message(NodeState *my_node, ParsedCommand *current_command);

struct addrinfo *udp_starter(char *regIP, char *regUDP);

struct addrinfo *tcp_starter(NodeState *my_node); // função para tratar as mensagens recebidas por TCP


#endif