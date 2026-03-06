#ifndef INTERFACE_H
#define INTERFACE_H


extern int fd_udp, fd_tcp_listen; // Sockets e endereços globais
extern int fd_edges[100]; // fd de conexões TCP ativas, max 100 conexões
extern struct addrinfo *address_udp, *address_tcp; // Endereços globais para UDP e TCP


int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP);

struct addrinfo *udp_starter(char *regIP, char *regUDP); 

struct addrinfo *tcp_starter(char *myIP, char *myTCP);

void send_and_receiveUDP(char *udp_message); // função auxiliar para enviar mensagem UDP e esperar pela resposta do servidor


#endif