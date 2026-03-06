#ifndef INTERFACE_H
#define INTERFACE_H


extern int fd_udp, fd_tcp_listen; // Sockets e endereços globais

extern struct addrinfo *address_udp; // Endereços globais para UDP e TCP


int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP);

void send_and_receive(char *udp_message); // função auxiliar para enviar mensagem UDP e esperar pela resposta do servidor

struct addrinfo *udp_starter(char *regIP, char *regUDP); 

void tcp_starter(char *myIP, char *myTCP);


#endif