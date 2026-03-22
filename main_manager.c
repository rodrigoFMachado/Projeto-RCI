#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <limits.h>


#include "main_manager.h"
#include "communication_handling.h"
#include "network_apps.h"

#define INVALID_NUMBER -1

int fd_udp, fd_tcp_listen; // Sockets e endereços globais
int fd_edges[100]; // fd de conexões TCP ativas, max 100 conexões
struct addrinfo *address_udp; // Endereços globais para UDP e TCP



bool word_processor(NodeState *my_node, ParsedCommand *current_command); 



void manager_of_all(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    int counter, maxfd;
    bool exit_failure;
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

    NodeState_inicialization(my_node, false, INVALID_NUMBER, INVALID_NUMBER); // Inicializa o estado do nó como não registado

    address_udp = udp_starter(regIP, regUDP);
    tcp_starter(myIP, myTCP); 

    for (int i = 0; i < 100; i++) fd_edges[i] = INVALID_NUMBER;

    while (1) {
        FD_ZERO(&rfds);

        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_tcp_listen, &rfds);

        exit_failure = false;

        maxfd = fd_tcp_listen; // conexões podem morrer

        // Adicionar conexões TCP ativas (fd_edges)
        for (int n = 0; n < 100; n++) { 
            if (fd_edges[n] != INVALID_NUMBER) {
                FD_SET(fd_edges[n], &rfds);
                if (fd_edges[n] > maxfd) maxfd = fd_edges[n];
            }
        }

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *) NULL);
        if (counter < 0) /*error*/ exit(1);
        if (counter == 0) continue; // Timeout, volta a esperar


        // ==========================================
        // A. TECLADO (Stdin)
        // ==========================================
        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP


            exit_failure = word_processor(my_node, current_command);
            
            if (exit_failure) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            exit_failure = false;

            if (strcmp(current_command->command, "x") == 0) { // condicao que executa break no final, apenas para exit

                if(my_node->is_registered) {
                    // Se estiver registado, faz o leave final
                    strcpy(current_command->command, "l"); // Executa leave final
                }
                
                // nao existe opcao para x nas seguintes funcoes, logo nao ha risco
                handle_udp_commands(my_node, current_command, myIP, myTCP); // faz o leave final

                handle_tcp_commands(my_node, current_command); // faz o remove edge final (loop de todos os vizinhos ativos)
                
                printf("A sair...\n");
                break;
            }


            exit_failure = handle_udp_commands(my_node, current_command, myIP, myTCP);

            
            if(exit_failure) {
                // Se o contacto falhou, não vamos tentar processar o segmento TCP porque não temos os dados necessários
                // Medida necessária para evitar segfault de ler a variável vazia, de má resposta de CONTACT
                continue;
            }

            handle_tcp_commands(my_node, current_command);
        }


        // ==========================================
        // B. PEDIDO DE CONEXÃO TCP (fd_tcp_listen)
        // ==========================================
        if (FD_ISSET(fd_tcp_listen, &rfds)) {

            accept_connection(my_node);

        }


        // ==========================================
        // C. LER MENSAGENS DOS VIZINHOS JÁ CONECTADOS
        // ==========================================
        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER && FD_ISSET(fd_edges[i], &rfds)) {
                
                char buffer[2*BUFFER_TCP_SIZE];  // mensagem total (+que 128)
                int bytes = read(fd_edges[i], buffer, sizeof(buffer) - 1);

                if (bytes <= 0) { 
                    // Se bytes == 0, o vizinho desligou-se normalmente (remove edge ou exit).
                    // Se bytes == -1, a ligação caiu de forma bruta.
                    printf("O nó %d desconectou-se (aresta removida).\n", i);

                    handle_link_drop(my_node, i); // Processar a queda da ligação no protocolo de encaminhamento

                    close(fd_edges[i]);
                    fd_edges[i] = INVALID_NUMBER; // Limpamos a aresta do nosso lado
                    
                } else {
                    // Recebemos texto do vizinho!
                    buffer[bytes] = '\0';
                    printf("Recebido do nó %d: %s", i, buffer);
                    
                    process_tcp_message(my_node, current_command, i, buffer);
                }
            }
        }
    }

    free(current_command);
    free(my_node);

    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != INVALID_NUMBER) {
            close(fd_edges[i]);
            fd_edges[i] = INVALID_NUMBER;
        }
    }

    freeaddrinfo(address_udp);

    close(fd_udp);
    close(fd_tcp_listen);

    return;
}



bool word_processor(NodeState *my_node, ParsedCommand *current_command) {
    char buffer_teclado[64] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command_first_w[16] = {0}; // Para guardar a primeira palavra do comando


        if (sscanf(buffer_teclado, "%s", command_first_w) == 1) {

            // Verificar join
            if (strcmp(command_first_w, "j") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %d", &current_command->net, &current_command->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return true; 
                }
                strcpy(current_command->command, "j"); 
            }

            // Verificar show nodes
            else if (strcmp(command_first_w, "n") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->net) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: show neighbors net\n");
                    return true;
                }
                strcpy(current_command->command, "n");
            }

            // Verificar leave
            else if (strcmp(command_first_w, "l") == 0) {

                if (!my_node->is_registered) {
                    printf("Erro: Não está registado. Não pode executar leave.\n");
                    return true; 
                }
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave\n");
                    return true; 
                }

                strcpy(current_command->command, "l"); 
            }

            // Verificar exit
            else if (strcmp(command_first_w, "x") == 0) {
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: exit\n");
                    return true; 
                }

                strcpy(current_command->command, "x");
            }

            // Verificar add edge
            else if(strcmp(command_first_w, "ae") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: add edge id\n");
                    return true; 
                }
                strcpy(current_command->command, "ae");
            }

            // Verificar remove edge
            else if(strcmp(command_first_w, "re") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: remove edge id\n");
                    return true; 
                }
                strcpy(current_command->command, "re");
            }

            // Verificar show neighbors
            else if (strcmp(command_first_w, "sg") == 0) {

                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: show neighbors\n");
                    return true; 
                }
                strcpy(current_command->command, "sg");
            }

            // Verificar announce
            else if (strcmp(command_first_w, "a") == 0) {

                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: announce\n");
                    return true; 
                }
                strcpy(current_command->command, "a");
            }

            // Verificar show routing
            else if (strcmp(command_first_w, "sr") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: show routing dest\n");
                    return true; 
                }
                strcpy(current_command->command, "sr");
            }

            // Verificar start monitor 
            else if (strcmp(command_first_w, "sm") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: start monitor\n");
                    return true; 
                }
                strcpy(current_command->command, "sm");
            }

            // Verificar end monitor 
            else if (strcmp(command_first_w, "em") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: end monitor\n");
                    return true; 
                }
                strcpy(current_command->command, "em");
            }

            // Verificar message 
            else if (strcmp(command_first_w, "m") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %s", &current_command->id, current_command->message) != 2) {
                    printf("Erro: Argumentos inválidos. Uso: message dest message\n");
                    return true; 
                }
                strcpy(current_command->command, "m");
            }

            // Verificar direct join
            else if (strcmp(command_first_w, "dj") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %d", &current_command->net, &current_command->id) != 2) {
                    printf("Erro: Argumentos inválidos. Uso: direct join net id\n");
                    return true; 
                } else {
                    my_node->net = current_command->net;
                    my_node->id = current_command->id;

                    my_node->is_registered = true;
                }              
            }

            // Verificar direct add edge
            else if (strcmp(command_first_w, "dae") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %s %s", &current_command->id, current_command->tempTCP_IP, current_command->tempTCP_Port) != 3) {
                    printf("Erro: Argumentos inválidos. Uso: direct add edge id idIP idTCP\n");
                    return true; 
                }

                if(my_node->is_registered) {
                    connect_to_node(my_node, current_command);
                } else {
                    printf("Erro: Não é um nó. Não pode executar 'direct add edge'.\n");
                    return true; 
                }
   
            }

            else {
                printf("Comando desconhecido: %s\n", command_first_w);
                    return true; 
            }  

        } else {
            // Entrada vazia apenas enter
                    return true; 
        }
    }
    return false; // Retorna 0 para indicar sucesso
}


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
