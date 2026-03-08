#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
    
#include "helper.h"


int fd_udp, fd_tcp_listen; // Sockets e endereços globais
int fd_edges[100]; // fd de conexões TCP ativas, max 100 conexões
struct addrinfo *address_udp; // Endereços globais para UDP e TCP



int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP) 
{
    if (argc < 3 || argc > 5) {
        printf("Uso: %s IP TCP [regIP] [regUDP]\n", argv[0]);
        exit(1);
    }

    *myIP = argv[1];
    *myTCP = argv[2];

    if (argv[3] != NULL && argv[4] != NULL) {
        *regIP = argv[3];
        *regUDP = argv[4];
    }

    return 0;
}



/// @brief Função bloqueante que envia e recebe a mensagem UDP
/// @param udp_message
void send_and_receiveUDP(char *udp_message) {
    int n;

    struct sockaddr addr;
    socklen_t addrlen;

    // printf("Enviando mensagem UDP: %s\n", udp_message); // Debug: mostra a mensagem que será enviada
    n=sendto(fd_udp, udp_message, strlen(udp_message), 0, address_udp->ai_addr, address_udp->ai_addrlen);
    if(n == -1) {
        printf("Erro ao enviar UDP.\n");
        return;
    }


    // Espera de resposta
    addrlen = sizeof(addr);
    n = recvfrom(fd_udp, udp_message, 128, 0, (struct sockaddr*)&addr, &addrlen);
    if(n == -1) {
        // O timeout disparou! O pacote perdeu-se ou o servidor está em baixo.
        printf("Erro: O servidor não respondeu (Timeout)\n");
        udp_message[0] = '\0'; // Limpa a string para o sscanf falhar em segurança
        return;
    }
    udp_message[n] = '\0';
    
    // printf("echo: %s\n", udp_message); // Debug: mostra a resposta recebida do servidor
}



/// @brief Cria o FD UDP, e a estrutura que contém informação do endereço UDP, que ficará numa variável global
struct addrinfo *udp_starter(char *regIP, char *regUDP) { 
    struct addrinfo hints, *address;
    int errcode;

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);

    struct timeval read_timeout;
    read_timeout.tv_sec = 5; // Espera no máximo 5 segundos pela resposta do professor
    read_timeout.tv_usec = 0;
    
    if (setsockopt(fd_udp, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0) {
        perror("Erro a definir timeout do UDP");
        exit(1);
    }

    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    errcode = getaddrinfo(regIP, regUDP, &hints, &address);
    if (errcode != 0) /*error*/ exit(1);


    return address;
}



/// @brief Cria o FD TCP de escuta, e efetua o bind e listen com a porta fornecida pelo utilizador
void tcp_starter(char *myIP, char *myTCP) {
    struct addrinfo hints, *address;
    int errcode;

    fd_tcp_listen = socket(AF_INET, SOCK_STREAM, 0); // Socket TCP para escuta
    if (fd_tcp_listen == -1) /*error*/ exit(1);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(myIP, myTCP, &hints, &address);
    if (errcode != 0) /*error*/ exit(1);

    if (bind(fd_tcp_listen, address->ai_addr, address->ai_addrlen) == -1)
        exit(1);

    if (listen(fd_tcp_listen, 5) == -1) // max 5 pending connections
        exit(1);

    freeaddrinfo(address);
    
    return;
}

