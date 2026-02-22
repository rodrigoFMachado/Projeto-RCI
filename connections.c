#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

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


void mother_of_all_manager(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    int counter, maxfd;
    fd_set rfds;

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);

    maxfd = fd_udp; //sempre maior que o STDIN_FILENO
    

    struct addrinfo *res_udp = udp_starter();
    

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_udp, &rfds);

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *)NULL);
        if (counter <= 0) /*error*/
            exit(1);

        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado
            char buffer_teclado[256];
            char command_arg[2] = {0}; 

            if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL)
            {
                char command[32]; // Para guardar a primeira palavra (o comando)

                // Lemos apenas a primeira palavra da linha para a variável 'command'
                if (sscanf(buffer_teclado, "%s", command) == 1) {

                    // Verificamos se é "join" OU "j"
                    if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0)
                    {
                        int net, id;
                        command_arg[0] = 'j';
                        if (sscanf(buffer_teclado, "%*s %d %d", &net, &id) == 2) { // get NET and ID
                            // funcao mae com arg j NET ID
                        }
                        else {
                            printf("Erro: Argumentos invalidos. Uso: join net id\n");
                        }
                    }

                    // Verificamos se é "leave" OU "l"
                    else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
                        command_arg[0] = 'l';
                        // funcao mae com arg l
                    }
                    else {
                        printf("Comando desconhecido: %s\n", command);
                    }
                }
            }
        }


        if (FD_ISSET(fd_udp, &rfds)) { // socket UDP ISTO VAI PARA O LXI

        }
        
    }
}


struct addrinfo *udp_starter() { 
    struct addrinfo hints, *res;
    int errcode;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP socket

    errcode = getaddrinfo("tejo.tecnico.ulisboa.pt", "58001", &hints, &res);
    if (errcode != 0) /*error*/
        exit(1);

    // sendto, recvfrom

    return res;
}



void handle_udp_message(char *message, int net, int id) {
    // Aqui vamos tratar as mensagens recebidas por UDP
    // Por exemplo, podemos verificar o conteúdo da mensagem e tomar ações específicas
}



void handle_tcp_connection() {
    // Aqui vamos tratar as mensagens recebidas por TCP
    // Por exemplo, podemos verificar o conteúdo da mensagem e tomar ações específicas
}