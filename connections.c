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


int fd_udp, fd_tcp_listen; // Sockets e endereços globais
struct addrinfo *address_udp, *address_tcp; // Endereços globais para UDP e TCP


struct NodeState_{
    bool is_registered;
    char net[4]; // max 3 digitos
    char id[3];  // max 2 digitos

    char *myIP; // imutavel depois de set
    char *myTCP;
};



struct ParsedCommand_{
    char command[4]; // max 3 letras
    char net[4]; // max 3 digitos
    char id[3];  // max 2 digitos
};


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    struct timeval timeout;
    int counter, maxfd, result;
    fd_set rfds;

    NodeState *my_node = malloc(sizeof(NodeState)); // Alocar memória para a estrutura
    if(my_node == NULL) {
        fprintf(stderr, "Erro ao alocar memória para NodeState\n");
        exit(1);
    }

    ParsedCommand *current_command = malloc(sizeof(ParsedCommand)); // Alocar memória para a estrutura
    if(current_command == NULL) {
        fprintf(stderr, "Erro ao alocar memória para ParsedCommand\n");
        free(my_node); // Liberar a memória alocada para NodeState antes de sair
        exit(1);
    }


    my_node->is_registered = false;
    my_node->myIP = myIP;
    my_node->myTCP = myTCP;
    
    address_udp = udp_starter(regIP, regUDP);
    address_tcp = tcp_starter(my_node); 

    maxfd = fd_tcp_listen; // por agora maior que 0


    while (1) {
        timeout.tv_sec = 5; timeout.tv_usec = 0; // Timeout de 5 segundos para o select
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_tcp_listen, &rfds);

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);
        if (counter < 0) /*error*/
            exit(1);
        if (counter == 0)
            continue;


        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP

            result = word_processor(current_command);
            
            if (result == 1) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            send_udp_message(my_node, current_command);
            
            if (result == 2) { // Se era um comando exit
                printf("A sair...\n");
                break;
            }
        }


        if (FD_ISSET(fd_tcp_listen, &rfds)) { // socket TCP de escuta, recebe mensagens do servidor
            ; // Função para tratar as mensagens recebidas por TCP
        }
        
    }

    free(current_command);
    free(my_node);

    freeaddrinfo(address_udp);
    freeaddrinfo(address_tcp);

    close(fd_udp);
    close(fd_tcp_listen);

    return;
}




int word_processor(ParsedCommand *current_command) {
    char buffer_teclado[256] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command[32] = {0}; // Para guardar a primeira palavra (o comando)

        // Lemos apenas a primeira palavra da linha para a variável 'command'
        if (sscanf(buffer_teclado, "%s", command) == 1) {

            // Verificar join OU j
            if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0) {
                strcpy(current_command->command, "j"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s %s", current_command->net, current_command->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return 1; // Retorna 1 para indicar erro
                }
            }

            // Verificar leave OU l
            else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
                strcpy(current_command->command, "l"); // Armazena o comando abreviado
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave \n");
                    return 1; // Retorna 1 para indicar erro
                }
            }

            // Verificar exit OU x
            else if (strcmp(command, "exit") == 0 || strcmp(command, "x") == 0) {
                if(strcmp(current_command->command, "l") == 0) {
                    strcpy(current_command->command, "x");
                    
                    printf("Comando 'leave' já foi processado, saindo...\n");

                } else {
                    strcpy(current_command->command, "l"); // Executa leave antes de sair

                    printf("Processando comando 'leave' antes de sair...\n");
                }

                return 2; // Código especial: processa leave e depois sai
            }

            // Verificar show nodes OU s
            else if (strcmp(command, "show nodes") == 0 || strcmp(command, "s") == 0) {
                strcpy(current_command->command, "s"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s", current_command->net) != 1) {// get ID 

                    printf("Erro: Argumentos inválidos. Uso: show nodes net\n");
                    return 1; // Retorna 1 para indicar erro
                }
            }
                
            else {
                printf("Comando desconhecido: %s\n", command);
                return 1; // Retorna 1 para indicar erro
            }  
        } 
        
        else {
            // Entrada vazia apenas enter
            return 1; // Retorna 1 para indicar erro
        }
    }

    return 0; // Retorna 0 para indicar sucesso
}



void send_udp_message(NodeState *my_node, ParsedCommand *current_command) {
    int n, tid = rand() % 1000; // Gerar um TID aleatório entre 0 e 999

    char udp_message[128+1];

    struct sockaddr addr;
    socklen_t addrlen;


    if (strcmp(current_command->command, "j") == 0) { // join

        if(my_node->is_registered) {
            printf("Erro: Já está registado. A cada contacto pode estar associado apenas um nó.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s %s %s", UDP_REG, tid, OP_REG_REQ, current_command->net, current_command->id, my_node->myIP, my_node->myTCP);

    }

    else if (strcmp(current_command->command, "l") == 0) { // leave

        snprintf(udp_message, sizeof(udp_message), "%s %d %d %s %s", UDP_REG, tid, OP_UNREG_REQ, current_command->net, current_command->id);

    }

    else if (strcmp(current_command->command, "x") == 0) {
        return;
    }


    printf("Enviando mensagem UDP: %s\n", udp_message); // Debug: mostra a mensagem que será enviada
    n=sendto(fd_udp, udp_message, strlen(udp_message), 0, address_udp->ai_addr, address_udp->ai_addrlen);
    if(n==-1)/*error*/exit(1);


    // Espera de resposta
    addrlen = sizeof(addr);

    n = recvfrom(fd_udp, udp_message, 128, 0, &addr, &addrlen);
    if(n==-1)/*error*/exit(1);
    udp_message[n] = '\0';
    
    printf("echo: %s\n", udp_message); // Debug: mostra a resposta recebida do servidor

    //logica para processar a resposta do servidor e atualizar o estado do nó
    // Comparar string com a resposta esperada para registo bem-sucedido
    if(strcmp(current_command->command, "j") == 0) {
        char expected_response[128+1];
        snprintf(expected_response, sizeof(expected_response), "%s %d %d %s %s", UDP_REG, tid, OP_REG_RES_OK, current_command->net, current_command->id);

        if(strcmp(udp_message, expected_response) == 0) {
            my_node->is_registered = true;
            printf("Registo bem-sucedido!\n");

            snprintf(expected_response, sizeof(expected_response), "%s %d %d", UDP_REG, tid, OP_REG_RES_FULL);

        } else if (strcmp(udp_message, expected_response) == 0) {
            printf("Erro: Base de dados cheia. Não foi possível registar o nó.\n");

        } else {
            printf("Erro no registo: Resposta inesperada do servidor.\n");
        }
    }

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




struct addrinfo *tcp_starter(NodeState *my_node) {
    struct addrinfo hints, *address;
    int errcode;

    fd_tcp_listen = socket(AF_INET, SOCK_STREAM, 0); // Socket TCP para escuta
    if (fd_tcp_listen == -1) /*error*/ exit(1);
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP socket
    hints.ai_flags = AI_PASSIVE;

    errcode = getaddrinfo(my_node->myIP, my_node->myTCP, &hints, &address);
    if (errcode != 0) /*error*/ exit(1);

    if (bind(fd_tcp_listen, address->ai_addr, address->ai_addrlen) == -1)
        exit(1);

    if (listen(fd_tcp_listen, 5) == -1) // max 5 pending connections
        exit(1);


    return address;
}