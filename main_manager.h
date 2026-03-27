#ifndef MAIN_MANAGER_H
#define MAIN_MANAGER_H

#include <stdbool.h>


extern int fd_udp, fd_tcp_listen; // Sockets e endereços globais
extern int fd_edges[100]; // fd de conexões TCP ativas, max 100 conexões
extern struct addrinfo *address_udp; // Endereços globais para UDP e TCP
extern bool routing_monitor_active; // Monitorização das mensagens de encaminhamento

// Escolhidos 100 para tamanho dos arrays por simplicidade - não precisamos de algoritmos de
// procura no array, apesar de saber que nunca serao conectados 99 vizinhos (em contexto de aula)
typedef struct NodeState_{
    bool is_registered;
    int net; 
    int id;

    int dist[100];       // Distância para cada destino (inicializado tudo a infinito (999, 100 nós com maximo 99 ligações cada))
    int succ[100];       // Vizinho de expedição (inicia tudo a -1)
    bool state[100];      // Estado: 0 (expedição) ou 1 (coordenação)
    
    int succ_coord[100]; // Quem causou a minha coordenação
    bool coord[100][100]; // Matriz de coordenação: coord[destino][vizinho] = 1 se o vizinho é coordenador para o destino
} NodeState;

typedef struct ParsedCommand_{
    char command[4]; // max 3 letras
    int net; // max 3 digitos
    int id;  // max 2 digitos

    char tempTCP_IP[16]; // Para guardar o IP temporário recebido do servidor para um contacto
    char tempTCP_Port[6]; // Para guardar o porto temporário recebido do servidor para um contacto
    char message[129]; // Para guardar a mensagem a enviar no comando "message"

} ParsedCommand;


void manager_of_all(char *myIP, char *myTCP, char *regIP, char *regUDP);

int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP);


#endif