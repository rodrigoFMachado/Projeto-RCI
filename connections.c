#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h> // Necessário para o gettimeofday


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


int fd_udp; // Socket UDP global para ser usado em várias funções
int index = 0; // Índice global para acessar a estrutura processed_command


struct processed_command_{
    int tid; // TID aleatório
    char command[4]; // max 3 letras
    char net[4]; // max 3 digitos
    char id[3];  // max 2 digitos
};


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    int counter, maxfd;
    fd_set rfds;


    processed_command **command_arg = malloc(sizeof(*processed_command)); // Alocar memória para a estrutura
    if(command_arg == NULL) {
        fprintf(stderr, "Erro ao alocar memória para processed_command\n");
        exit(1);
    }

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);


    maxfd = fd_udp; //sempre maior que o STDIN_FILENO
    

    struct addrinfo *res_udp = udp_starter(regIP, regUDP);
    

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_udp, &rfds);

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);
        if (counter <= 0) /*error*/
            exit(1);

        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP

            if (word_processor(command_arg) != 0) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            send_udp_message(myIP, myTCP, res_udp, command_arg); 
            
        }


        if (FD_ISSET(fd_udp, &rfds)) { // socket UDP, recebe mensagens do servidor
            receive_udp_message();
        }
        
    }

    foreach(command_arg, free); // Libera a memória alocada para a estrutura
    free(command_arg);
}




int word_processor(processed_command **arguments) {
    char buffer_teclado[256];

    arguments[index] = malloc(sizeof(processed_command)); // Aloca memória para a estrutura
    if(arguments[index] == NULL) {
        fprintf(stderr, "Erro ao alocar memória para processed_command\n");
        return 1; // Retorna 1 para indicar erro
    }
    index++; // Incrementa o índice para a próxima estrutura

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command[32]; // Para guardar a primeira palavra (o comando)

        // Lemos apenas a primeira palavra da linha para a variável 'command'
        if (sscanf(buffer_teclado, "%s", command) == 1) {

            // Verificamos se é "join" OU "j"
            if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0) {
                strcpy(arguments[index]->command, "j"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s %s", arguments[index]->net, arguments[index]->id) != 2) {// get NET and ID
                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return 1; // Retorna 1 para indicar erro
                }

            // Verificamos se é "leave" OU "l"
            } else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
                strcpy(arguments[index]->command, "l"); // Armazena o comando abreviado
                
                if (sscanf(buffer_teclado, "%*s") != 0) {// get NET and ID
                    printf("Erro: Argumentos inválidos. Uso: leave \n");
                    return 1; // Retorna 1 para indicar erro
                }


            } else {
                free(arguments[index]); // Libera a memória alocada para a estrutura em caso de comando desconhecido
                index--; // Decrementa o índice para não deixar um espaço vazio na estrutura
                printf("Comando desconhecido: %s\n", command);
                return 1; // Retorna 1 para indicar erro
            }
        }
    }

    return 0; // Retorna 0 para indicar sucesso
}



void send_udp_message(char *myIP, char *myTCP, struct addrinfo *res_udp, processed_command **arguments) {
    char udp_message[128];
    struct timeval tv;
    gettimeofday(&tv, NULL);

    int n;
    
    if (strcmp(arguments->command, "j") == 0) { // join
        arguments[index]->tid = (tv.tv_usec / 1000) % 1000;; // Atribui o TID gerado à estrutura

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s %s %s", UDP_REG, arguments[index]->tid, OP_REG_REQ, arguments->net, arguments->id, myIP, myTCP);

    }
    else if (strcmp(arguments->command, "l") == 0) { // leave
        arguments[index]->tid = (tv.tv_usec / 1000) % 1000;; // Atribui o TID gerado à estrutura

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s", UDP_REG, arguments[index]->tid, OP_UNREG_REQ, arguments->net, arguments->id);

    }

    printf("Enviando mensagem UDP: %s\n", udp_message); // Debug: mostra a mensagem que será enviada
    n=sendto(fd_udp, udp_message, strlen(udp_message), 0, res_udp->ai_addr, res_udp->ai_addrlen);
    if(n==-1)/*error*/exit(1);
}


void receive_udp_message() {
    struct sockaddr addr;
    socklen_t addrlen;
    char udp_buffer[128 + 1];
    int n;

    addrlen = sizeof(addr);
    n = recvfrom(fd_udp, udp_buffer, 128, 0, &addr, &addrlen);
    udp_buffer[n] = '\0';

    // encontrar o tid



    printf("echo: %s\n", udp_buffer);
}





struct addrinfo *udp_starter(char *regIP, char *regUDP) { 
    struct addrinfo hints, *res;
    int errcode;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    errcode = getaddrinfo(regIP, regUDP, &hints, &res);
    if (errcode != 0) /*error*/
        exit(1);

    // sendto, recvfrom

    return res;
}




void handle_tcp_connection() {
    // Aqui vamos tratar as mensagens recebidas por TCP
    // Por exemplo, podemos verificar o conteúdo da mensagem e tomar ações específicas
}