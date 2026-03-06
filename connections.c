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

    int dist[100];       // Distância para cada destino (inicia tudo a infinito/999)
    int succ[100];       // Vizinho de expedição (inicia tudo a -1)
    bool state[100];      // Estado: 0 (expedição) ou 1 (coordenação)
    
    int succ_coord[100]; // Quem causou a minha coordenação
    int coord[100][100]; // Matriz de coordenação: coord[destino][vizinho] = 1 se o vizinho é coordenador para o destino
} NodeState;

typedef struct ParsedCommand_{
    char command[4]; // max 3 letras
    int net; // max 3 digitos
    int id;  // max 2 digitos
    int dest;

    char tempTCP_IP[16]; // Para guardar o IP temporário recebido do servidor para um contacto
    char tempTCP_Port[6]; // Para guardar o porto temporário recebido do servidor para um contacto

} ParsedCommand;




void send_udp_message(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP); 

int word_processor(NodeState *my_node, ParsedCommand *current_command); 

void connect_to_node(NodeState *my_node, ParsedCommand *current_command);

void accept_connection(void);



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

    current_command->tempTCP_IP[0] = '\0';

    address_udp = udp_starter(regIP, regUDP);
    tcp_starter(myIP, myTCP); 

    for (int i = 0; i < 100; i++) fd_edges[i] = -1;

    while (1) {
        timeout.tv_sec = 5; timeout.tv_usec = 0; // Timeout de 5 segundos para o select
        FD_ZERO(&rfds);

        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_tcp_listen, &rfds);

        maxfd = fd_tcp_listen; // conexões podem morrer

        // 1. Adicionar conexões TCP ativas (fd_edges)
        for (int n = 0; n < 100; n++) { 
            if (fd_edges[n] != -1) {
                FD_SET(fd_edges[n], &rfds);
                if (fd_edges[n] > maxfd) maxfd = fd_edges[n];
            }
        }



        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, &timeout);
        if (counter < 0) /*error*/ exit(1);
        if (counter == 0) continue; // Timeout, volta a esperar



        // ==========================================
        // A. TECLADO (Stdin)
        // ==========================================
        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP

            result = word_processor(my_node, current_command);
            
            if (result == 1) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            send_udp_message(my_node, current_command, myIP, myTCP);

            if (result == 2) {
                // Se o comando foi "exit", o send_udp_message já tratou do "leave" se necessário, então só precisamos de sair do loop
                printf("A sair...\n");
                break;
            }

            if (strcmp(current_command->command, "ae") == 0 && current_command->tempTCP_IP[0] != '\0') {
                
                connect_to_node(my_node, current_command);

            }else if(strcmp(current_command->command, "sg") == 0){
                // usamos a estrutura local de fd_edges para mostrar as ligações ativas
                // index de fd_edges com alguma coisa diferente de -1 é um vizinho ativo
                bool exists = false;
                printf("Vizinhos ativos:\n");
                for (int i = 0; i < 100; i++) {
                    if (fd_edges[i] != -1) {
                        exists = true;
                        printf("%d\n", i);
                    }
                }
                if (!exists) {
                    printf("Nenhum vizinho ativo.\n");
                }
            }
        }


        // ==========================================
        // B. PEDIDO DE CONEXÃO TCP (fd_tcp_listen)
        // ==========================================
        if (FD_ISSET(fd_tcp_listen, &rfds)) {

            accept_connection();

        }

    }

    free(current_command);
    free(my_node);

    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != -1) {
            close(fd_edges[i]);
            fd_edges[i] = -1;
        }
    }

    freeaddrinfo(address_udp);

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

                } else {
                    strcpy(current_command->command, "x");
                    
                }

                return 2; // Código especial: dá sempre break
            }



            else if(strcmp(command_first_w, "add") == 0) {
                strcpy(current_command->command, "ae");

                if (sscanf(buffer_teclado, "%*s %*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: add edge id\n");
                    return 1; // Retorna 1 para continuar
                }
            }

            else if(strcmp(command_first_w, "ae") == 0) {
                strcpy(current_command->command, "ae");

                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: ae id\n");
                    return 1; // Retorna 1 para continuar
                }
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


/// @brief Processa o comando do usuário, constrói a mensagem UDP adequada, envia para o servidor e trata a resposta.
/// @param my_node - Contém estado atual do nó, registado(?), net, id, etc
/// @param current_command - Contém o comando processado do usuário, com os campos preenchidos (command, net, id, etc)
/// @param myIP - Endereço IP do nó
/// @param myTCP - Porta TCP do nó
void send_udp_message(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP) {
    int tid = rand() % 1000; // Gerar um TID aleatório entre 0 e 999

    char udp_message[128+1]; // mensagem UDP com máximo de 128 caracteres

    int received_tid, received_op;

    if (strcmp(current_command->command, "j") == 0) { // join

        if(my_node->is_registered) {
            printf("Erro: Já está registado. A cada contacto pode estar associado apenas um nó.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d %s %s", UDP_REG, tid, OP_REG_REQ, current_command->net, current_command->id, myIP, myTCP);


        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_REG_RES_OK) { // Expected: 1
                    my_node->is_registered = true;

                    my_node->net = current_command->net;
                    my_node->id = current_command->id;

                    printf("Registo bem-sucedido do nó %d na rede %d.\n", my_node->id, my_node->net);

                } else if (received_op == OP_REG_RES_FULL) { // Expected: 2
                    printf("Erro: Base de dados cheia. Não foi possível registar o nó.\n");
                } 
                
                else {
                    printf("Erro: Resposta inesperada do servidor.");
                }
            }
        }
    }


    else if (strcmp(current_command->command, "l") == 0) { // leave

        if(!my_node->is_registered) {
            printf("Erro: Não está registado. Não pode executar 'leave'.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d", UDP_REG, tid, OP_UNREG_REQ, my_node->net, my_node->id);


        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_UNREG_RES_OK) { // Expected: 4
                    my_node->is_registered = false;

                    printf("Remoção do bem-sucedida do nó %d da rede %d\n", my_node->id, my_node->net);
                }
                else {
                    printf("Erro: Resposta inesperada do servidor.");
                }
            }
        }
    }


    else if (strcmp(current_command->command, "n") == 0) { // show nodes

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d", UDP_NODES, tid, OP_NODES_REQ, current_command->net);
        

        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if(sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if(received_tid == tid) {

                if(received_op == OP_NODES_RES) { // Expected: 1
                    printf("Nós na rede %d: %s\n", current_command->net, udp_message + 16); // Imprime a lista de nós (tudo depois dos 16 primeiros caracteres "NODES TID OP NET ")
                } else {
                    printf("Erro: Resposta inesperada do servidor.");
                }
            }
        }
    }


    else if (strcmp(current_command->command, "ae") == 0) { // add edge

        if(!my_node->is_registered) {
            printf("Erro: Não está registado. Não pode executar 'add'.\n");
            return;
        }

        if(current_command->id == my_node->id) {
            printf("Erro: Não pode criar uma aresta para si mesmo.\n");
            return;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d", UDP_CONTACT, tid, OP_CONTACT_REQ, my_node->net, current_command->id);
        

        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_CONTACT_RES) { // Expected: 1
                    sscanf(udp_message, "%*s %*s %*s %*s %*s %s %s", current_command->tempTCP_IP, current_command->tempTCP_Port);
                    // em caso de sucesso tenta fazer a conexão TCP
                } 
                
                else if (received_op == OP_CONTACT_NO_REG) { // Expected: 2
                    printf("Erro: Nó %d não registado. Não foi possível obter o contacto.\n", current_command->id);
                } 
                
                else {
                    printf("Erro: Resposta inesperada do servidor.");
                }
            }
        }

    }   


    else if (strcmp(current_command->command, "x") == 0) {
        return;
    }

    return;

}



void connect_to_node(NodeState *my_node, ParsedCommand *current_command) {
    int fd_out = socket(AF_INET, SOCK_STREAM, 0);
    
    // 2. Preparar a morada do destino com base no que recebemos do UDP
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    getaddrinfo(current_command->tempTCP_IP, current_command->tempTCP_Port, &hints, &res);
    
    // 3. FAZER O CONNECT (Bater à porta do vizinho)
    if (connect(fd_out, res->ai_addr, res->ai_addrlen) == -1) {
        printf("Erro ao conectar ao nó %d.\n", current_command->id);
        close(fd_out);
    } else {
        // 4. LIGOU COM SUCESSO! Guardar no nosso array mágico
        fd_edges[current_command->id] = fd_out;
        
        // 5. OBRIGATÓRIO: Dizer "Olá, eu sou o nó X!"
        char msg_neighbor[32];
        sprintf(msg_neighbor, "NEIGHBOR %02d\n", my_node->id);
        write(fd_out, msg_neighbor, strlen(msg_neighbor));
        
        printf("Aresta criada com sucesso com o nó %d\n", current_command->id);
    }

    freeaddrinfo(res);
}



void accept_connection(void) {
    struct sockaddr addr;
    socklen_t addrlen;
    addrlen=sizeof(addr);
    int new_fd = accept(fd_tcp_listen, (struct sockaddr*)&addr, &addrlen);

    char buffer[64];
        // Lemos os dados. Como o select avisou, o read() é instantâneo!
    int bytes_read = read(new_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read == -1) {
        printf("Erro ao ler do socket de conexão TCP.\n");
        close(new_fd);
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        int vizinho_id;
            
        // Verificamos se ele nos disse o ID corretamente
        if (sscanf(buffer, "NEIGHBOR %d", &vizinho_id) == 1) {
                
            // SUCESSO!
            fd_edges[vizinho_id] = new_fd;
                
            printf("Nó %d ligou-se a nós com sucesso!\n", vizinho_id);
        }
    }
}