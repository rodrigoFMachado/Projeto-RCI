#ifndef COMMUNICATION_HANDLING_H
#define COMMUNICATION_HANDLING_H


#define BUFFER_TCP_SIZE 128


int handle_udp_commands(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP); 

void send_and_receiveUDP(char *udp_message); // função auxiliar para enviar mensagem UDP e esperar pela resposta do servidor

struct addrinfo *udp_starter(char *regIP, char *regUDP);

void handle_tcp_commands(NodeState *my_node, ParsedCommand *current_command);

void connect_to_node(NodeState *my_node, ParsedCommand *current_command);

void accept_connection(void);

void tcp_starter(char *myIP, char *myTCP);



#endif 
