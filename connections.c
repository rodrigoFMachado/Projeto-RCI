#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>


#include "connections.h"


// COMANDOS UDP para o servidor
#define UDP_NODES   "NODES"
#define UDP_CONTACT "CONTACT"
#define UDP_REG     "REG"

// NODES opcodes
#define OP_NODES_REQ    0  // Pede lista de nós 
#define OP_NODES_RES    1  // Resposta com a lista 

// CONTACT opcodes
#define OP_CONTACT_REQ      0  // Pede contacto de um nó 
#define OP_CONTACT_RES      1  // Resposta com o contacto 
#define OP_CONTACT_NO_REG   2  // Erro: Nó não registado 

// REG opcodes
#define OP_REG_REQ          0  // Pede registo do nó 
#define OP_REG_RES_OK       1  // Confirmação do registo ou
#define OP_REG_RES_FULL     2  // Erro: Base de dados cheia 
#define OP_UNREG_REQ        3  // Pede remoção do registo (Leave) 
#define OP_UNREG_RES_OK     4  // Confirmação da remoção 


int fd_udp, fd_tcp_listen; // Socket UDP global para ser usado em várias funções


struct processed_command_{
    char command[4]; // max 3 letras
    char net[4]; // max 3 digitos
    char id[3];  // max 2 digitos
};


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    int counter, maxfd;
    fd_set rfds;


    processed_command *command_arg = malloc(sizeof(processed_command)); // Alocar memória para a estrutura
    if(command_arg == NULL) {
        fprintf(stderr, "Erro ao alocar memória para processed_command\n");
        exit(1);
    }
    

    struct addrinfo *address_udp = udp_starter(regIP, regUDP);
    struct addrinfo *address_tcp = tcp_starter(myIP, myTCP); 

    maxfd = fd_tcp_listen; // por agora maior que 0


    while (1) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_tcp_listen, &rfds); // Adiciona o socket TCP à lista de descritores a monitorizar

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);
        if (counter <= 0) /*error*/
            exit(1);

        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP

            if (word_processor(command_arg) != 0) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            send_udp_message(myIP, myTCP, address_udp, command_arg);
        }


        if (FD_ISSET(fd_tcp_listen, &rfds)) { // socket TCP de escuta, recebe mensagens do servidor
            ; // Função para tratar as mensagens recebidas por TCP
        }
        
    }

    free(command_arg);
}




int word_processor(processed_command *arguments) {
    char buffer_teclado[256] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command[32] = {0}; // Para guardar a primeira palavra (o comando)

        // Lemos apenas a primeira palavra da linha para a variável 'command'
        if (sscanf(buffer_teclado, "%s", command) == 1) {

            // Verificamos se é "join" OU "j"
            if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0) {
                strcpy(arguments->command, "j"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s %s", arguments->net, arguments->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return 1; // Retorna 1 para indicar erro
                }

            // Verificamos se é "leave" OU "l"
            } else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
                strcpy(arguments->command, "l"); // Armazena o comando abreviado
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave \n");
                    return 1; // Retorna 1 para indicar erro
                }

            } else {
                printf("Comando desconhecido: %s\n", command);
                return 1; // Retorna 1 para indicar erro
            }
            
        } else {
            // Entrada vazia apenas enter
            return 1; // Retorna 1 para indicar erro
        }
    }

    return 0; // Retorna 0 para indicar sucesso
}



void send_udp_message(char *myIP, char *myTCP, struct addrinfo *address_udp, processed_command *arguments) {
    int n, tid = rand() % 1000; // Gerar um TID aleatório entre 0 e 999

    char udp_message[128+1];

    struct sockaddr addr;
    socklen_t addrlen;


    if (strcmp(arguments->command, "j") == 0) { // join

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s %s %s", UDP_REG, tid, OP_REG_REQ, arguments->net, arguments->id, myIP, myTCP);

    }
    else if (strcmp(arguments->command, "l") == 0) { // leave

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s", UDP_REG, tid, OP_UNREG_REQ, arguments->net, arguments->id);

    }


    printf("Enviando mensagem UDP: %s\n", udp_message); // Debug: mostra a mensagem que será enviada

    n=sendto(fd_udp, udp_message, strlen(udp_message), 0, address_udp->ai_addr, address_udp->ai_addrlen);
    if(n==-1)/*error*/exit(1);

    addrlen = sizeof(addr);

    n = recvfrom(fd_udp, udp_message, 128, 0, &addr, &addrlen);
    if(n==-1)/*error*/exit(1);
    udp_message[n] = '\0';
    
    printf("echo: %s\n", udp_message); // Debug: mostra a resposta recebida do servidor
}




struct addrinfo *udp_starter(char *regIP, char *regUDP) { 
    struct addrinfo hints, *address;
    int errcode;

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);

    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    errcode = getaddrinfo(regIP, regUDP, &hints, &address);
    if (errcode != 0) /*error*/ exit(1);


    return address;
}




struct addrinfo *tcp_starter(char *myIP, char *myTCP) {
    struct addrinfo hints, *address;
    int errcode;

    fd_tcp_listen = socket(AF_INET, SOCK_STREAM, 0); // Socket TCP para escuta
    if (fd_tcp_listen == -1) /*error*/ exit(1);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(myIP, myTCP, &hints, &address);
    if (errcode != 0) /*error*/ exit(1);

    if (bind(fd_tcp_listen, address->ai_addr, address->ai_addrlen) == -1)
        exit(1);

    if (listen(fd_tcp_listen, 5) == -1) // max 5 pending connections
        exit(1);


    return address;
}