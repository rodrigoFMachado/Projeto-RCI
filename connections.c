#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>


#include "connections.h"
#include "helper.h"



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



typedef struct NodeState_{
    bool is_registered;
    int net; 
    int id;
} NodeState;

typedef struct ParsedCommand_{
    char command[4]; // max 3 letras
    int net; // max 3 digitos
    int id;  // max 2 digitos
    int dest;
} ParsedCommand;



void send_udp_message(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP); 

int word_processor(NodeState *my_node, ParsedCommand *current_command); 



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

    
    address_udp = udp_starter(regIP, regUDP);
    address_tcp = tcp_starter(myIP, myTCP); 

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

            result = word_processor(my_node, current_command);
            
            if (result == 1) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            send_udp_message(my_node, current_command, myIP, myTCP);
            
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



int word_processor(NodeState *my_node, ParsedCommand *current_command) {
    char buffer_teclado[64] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command_first_w[16] = {0}; // Para guardar a primeira palavra do comando
        char command_second_w[32] = {0}; // Segunda palavra (nem sempre usado)


        if (sscanf(buffer_teclado, "%s", command_first_w) == 1) {


            // Verificar join OU j
            if (strcmp(command_first_w, "join") == 0 || strcmp(command_first_w, "j") == 0) {
                strcpy(current_command->command, "j"); 

                if (sscanf(buffer_teclado, "%*s %d %d", &current_command->net, &current_command->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return 1; // Retorna 1 para continuar
                }
            }



            // Verificar leave OU l
            else if (strcmp(command_first_w, "leave") == 0 || strcmp(command_first_w, "l") == 0) {
                strcpy(current_command->command, "l"); 
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave \n");
                    return 1; // Retorna 1 para continuar
                }
            }



            // Verificar exit OU x
            else if (strcmp(command_first_w, "exit") == 0 || strcmp(command_first_w, "x") == 0) {
                if(my_node->is_registered) {
                    strcpy(current_command->command, "l"); // Executa leave antes de sair

                    printf("Processando comando 'leave' antes de sair...\n");

                } else {
                    strcpy(current_command->command, "x");
                    
                    printf("Comando 'leave' já foi processado, saindo...\n");
                }

                return 2; // Código especial: dá sempre break
            }


            // show por extenso
            else if (strcmp(command_first_w, "show") == 0) {

                if(sscanf(buffer_teclado, "%*s %s", command_second_w) == 1) { // Lemos a 2ª palavra

                    if (strcmp(command_second_w, "nodes") == 0) {
                        strcpy(current_command->command, "n"); 

                        if (sscanf(buffer_teclado, "%*s %*s %d", &current_command->net) != 1) {
                            printf("Erro: Argumentos inválidos. Uso: show nodes net\n");
                            return 1;
                        }

                    } else if (strcmp(command_second_w, "neighbors") == 0) {
                        strcpy(current_command->command, "sg");
                        
                        if (sscanf(buffer_teclado, "%*s %*s") != 0) {
                            printf("Erro: Argumentos inválidos. Uso: show neighbors\n");
                            return 1;
                        }

                    } else if (strcmp(command_second_w, "routing") == 0) {
                        strcpy(current_command->command, "sr");

                        if (sscanf(buffer_teclado, "%*s %*s %d", &current_command->dest) != 1) {
                            printf("Erro: Argumentos inválidos. Uso: show routing dest\n");
                            return 1;
                        }

                    } else {
                        printf("Erro: Opção inválida. Uso: show [nodes|neighbors|routing]\n");
                        return 1;
                    }
                } else {
                    printf("Erro: O comando 'show' está incompleto.\n");
                    return 1;
                }
            } 
            
            // show por abreviatura
            else if (strcmp(command_first_w, "n") == 0) {
                strcpy(current_command->command, "n"); 

                // Saltamos só 1 palavra ("n") e lemos o argumento
                if (sscanf(buffer_teclado, "%*s %d", &current_command->net) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: n net\n");
                    return 1;
                }
            } 
            
            else if (strcmp(command_first_w, "sg") == 0) {
                strcpy(current_command->command, "sg");

                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: sg\n");
                    return 1;
                }
            } 
            
            else if (strcmp(command_first_w, "sr") == 0) {
                strcpy(current_command->command, "sr");

                // Saltamos só 1 palavra ("sr") e lemos o destino
                if (sscanf(buffer_teclado, "%*s %d", &current_command->dest) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: sr dest\n");
                    return 1;
                }
            }

            else {
                printf("Comando desconhecido: %s\n", command_first_w);
                return 1; // Retorna 1 para indicar erro
            }  

        } else {
            // Entrada vazia apenas enter
            return 1; // Retorna 1 para indicar erro
        }
    }
    return 0; // Retorna 0 para indicar sucesso
}



void send_udp_message(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP) {
    int tid = rand() % 1000; // Gerar um TID aleatório entre 0 e 999

    char udp_message[128+1];

    int received_tid;
    int received_op;

    if (strcmp(current_command->command, "j") == 0) { // join

        if(my_node->is_registered) {
            printf("Erro: Já está registado. A cada contacto pode estar associado apenas um nó.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d %s %s", UDP_REG, tid, OP_REG_REQ, current_command->net, current_command->id, myIP, myTCP);


        send_and_receive(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_REG_RES_OK) { // Expected: 1
                    my_node->is_registered = true;

                    my_node->net = current_command->net;
                    my_node->id = current_command->id;

                    printf("Registo bem-sucedido na rede %d com ID %d!\n", my_node->net, my_node->id);

                } else if (received_op == OP_REG_RES_FULL) { // Expected: 2
                    printf("Erro: Base de dados cheia. Não foi possível registar o nó.\n");
                } 
                
                else {
                    printf("Erro: Resposta inesperada do servidor com op_code: %d\n", received_op);
                }
            } 
            
            else {
                printf("Aviso: Recebida resposta com TID diferente.");
            }
        } 
        
        else {
            printf("Erro: Formato da resposta UDP inválido ou incompleto.\n");
        }
    }

    else if (strcmp(current_command->command, "l") == 0) { // leave

        if(!my_node->is_registered) {
            printf("Erro: Não está registado. Não pode executar 'leave'.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d", UDP_REG, tid, OP_UNREG_REQ, my_node->net, my_node->id);

        send_and_receive(udp_message); // Envia a mensagem e espera pela resposta do servidor

        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_UNREG_RES_OK) { // Expected: 4
                    my_node->is_registered = false;

                    printf("Remoção do registo bem-sucedida da rede %d com ID %d!\n", my_node->net, my_node->id);
                }
                else {
                    printf("Erro: Resposta inesperada do servidor com op_code: %d\n", received_op);
                }
            }
            else {
                printf("Aviso: Recebida resposta com TID diferente.");
            }
        }

        else {
            printf("Erro: Formato da resposta UDP inválido ou incompleto.\n");
        }

    }

    else if (strcmp(current_command->command, "n") == 0) { // show nodes

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d", UDP_NODES, tid, OP_NODES_REQ, current_command->net);
        
        send_and_receive(udp_message); // Envia a mensagem e espera pela resposta do servidor


    }

    else if (strcmp(current_command->command, "x") == 0) {
        return;
    }

}