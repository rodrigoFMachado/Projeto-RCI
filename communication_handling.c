#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>


#include "main_manager.h"
#include "communication_handling.h"
#include "network_apps.h"


// COMANDOS UDP para o servidor
#define UDP_NODES   "NODES"
#define UDP_CONTACT "CONTACT"
#define UDP_REG     "REG"
#define INVALID_NUMBER -1
#define INFINITO 999

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



/// @brief Processa o comando do usuário, constrói a mensagem UDP adequada, envia para o servidor e trata a resposta.
/// @param my_node - Contém estado atual do nó, registado(?), net, id, etc
/// @param current_command - Contém o comando processado do usuário, com os campos preenchidos (command, net, id, etc)
/// @param myIP - Endereço IP do nó
/// @param myTCP - Porta TCP do nó
bool handle_udp_commands(NodeState *my_node, ParsedCommand *current_command, char *myIP, char *myTCP) {
    int tid = rand() % 1000; // Gerar um TID aleatório entre 0 e 999

    char udp_message[128+1]; // mensagem UDP com máximo de 128 caracteres

    int received_tid, received_op;

    if (strcmp(current_command->command, "j") == 0) { // join

        if(my_node->is_registered) {
            printf("Erro: Já está registado. A cada contacto pode estar associado apenas um nó.\n");
            return true;
        }

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d %s %s", UDP_REG, tid, OP_REG_REQ, current_command->net, current_command->id, myIP, myTCP);


        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_REG_RES_OK) { // Expected: 1
                    NodeState_inicialization(my_node, true, current_command->net, current_command->id); // Atualiza o estado do nó para registado com os dados corretos

                    printf("Registo bem-sucedido do nó %d na rede %d.\n", my_node->id, my_node->net);

                } else if (received_op == OP_REG_RES_FULL) { // Expected: 2
                    printf("Erro: Base de dados cheia. Não foi possível registar o nó.\n");
                    return true;
                
                } else {
                    printf("Erro: Resposta inesperada do servidor.\n");
                    return true;
                }
            }
        }
    }


    else if (strcmp(current_command->command, "l") == 0) { // leave

        snprintf(udp_message, sizeof(udp_message), "%s %03d %d %03d %02d", UDP_REG, tid, OP_UNREG_REQ, my_node->net, my_node->id);


        send_and_receiveUDP(udp_message); // Envia a mensagem e espera pela resposta do servidor


        if (sscanf(udp_message, "%*s %d %d", &received_tid, &received_op) >= 2) {
            if (received_tid == tid) {

                if (received_op == OP_UNREG_RES_OK) { // Expected: 4
                    my_node->is_registered = false;

                    printf("Remoção do bem-sucedida do nó %d da rede %d\n", my_node->id, my_node->net);
                }
                else {
                    printf("Erro: Resposta inesperada do servidor.\n");
                    return true;
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
                    printf("Nós na rede %d:\n%s\n", current_command->net, udp_message + 16); // Imprime tudo depois dos 16 primeiros caracteres "NODES TID OP NET "
                } else {
                    printf("Erro: Resposta inesperada do servidor.\n");
                    return true;
                }
            }
        }
    }


    else if (strcmp(current_command->command, "ae") == 0) { // add edge

        if(current_command->id == my_node->id) {
            printf("Erro: Não pode criar uma aresta para si mesmo.\n");
            return true;
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
                    return true;
                } 
                
                else {
                    printf("Erro inesperado ao tentar obter o contacto.\n");
                    return true;
                }
            }
        }

    }   

    return false; // Comando UDP processado sem erros fatais

}


/// @brief Função bloqueante que envia e recebe a mensagem UDP
/// @param udp_message
void send_and_receiveUDP(char *udp_message) {
    int n;

    struct sockaddr addr;
    socklen_t addrlen;

    for(int i = 0; i < 2; i++) {
        n=sendto(fd_udp, udp_message, strlen(udp_message), 0, address_udp->ai_addr, address_udp->ai_addrlen);
        if(n == -1) {
            printf("Erro ao enviar UDP.\n");
            return;
        }

        // Espera de resposta
        addrlen = sizeof(addr);
        n = recvfrom(fd_udp, udp_message, 128, 0, (struct sockaddr*)&addr, &addrlen);
        if(n != -1) {
            udp_message[n] = '\0';
            break; // Recebeu resposta, sai do ciclo
        }
        
        printf("Erro: O servidor não respondeu (Timeout)\n");
        printf("A retransmitir.\n");
        udp_message[0] = '\0'; // termina a string para nao ler lixo na ultima iteracao
    }
    

    // printf("Enviando mensagem UDP: %s\n", udp_message); // Debug: mostra a mensagem que será enviada

    
    // printf("echo: %s\n", udp_message); // Debug: mostra a resposta recebida do servidor
}


/// @brief Cria o FD UDP, e a estrutura que contém informação do endereço UDP, que ficará numa variável global
struct addrinfo *udp_starter(char *regIP, char *regUDP) { 
    struct addrinfo hints, *address;
    int errcode;

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);

    struct timeval read_timeout;
    read_timeout.tv_sec = 10; // Espera no máximo 10 segundos pela resposta do professor
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


void handle_tcp_commands(NodeState *my_node, ParsedCommand *current_command) {


    if (strcmp(current_command->command, "l") == 0) {

        // O nó vai sair da rede. Não há roteamento a fazer, apenas fechar portas.
        for(int i = 0; i < 100; i++) {
            if(fd_edges[i] != INVALID_NUMBER) {
                close(fd_edges[i]);
                fd_edges[i] = INVALID_NUMBER;
            }
        }
        printf("Arestas locais fechadas (Leave).\n");


    } else if (strcmp(current_command->command, "ae") == 0 || strcmp(current_command->command, "dae") == 0) {
        
        connect_to_node(my_node, current_command);


    } else if(strcmp(current_command->command, "sg") == 0){
        // usamos a estrutura local de fd_edges para mostrar as ligações ativas
        // index de fd_edges com alguma coisa diferente de -1 é um vizinho ativo
        bool exists = false;
        printf("Vizinhos ativos:\n");
        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER) {
                exists = true;
                printf("%d\n", i);
            }
        }
        if (!exists) {
            printf("Nenhum vizinho ativo.\n");
        }


    } else if (strcmp(current_command->command, "re") == 0) {
        
        if (fd_edges[current_command->id] != INVALID_NUMBER) {

            close(fd_edges[current_command->id]);
            fd_edges[current_command->id] = INVALID_NUMBER; // Liberta o slot!
            printf("Aresta com o nó %d removida.\n", current_command->id);

            handle_link_drop(my_node, current_command->id); // Processar a queda da ligação no protocolo de encaminhamento

        } else {
            printf("Erro: Não existe aresta ativa com o nó %d.\n", current_command->id);
        }


    } else if (strcmp(current_command->command, "a") == 0) {
        
        my_node->dist[my_node->id] = 0;
        my_node->succ[my_node->id] = my_node->id;

        char announce_msg[32];
        sprintf(announce_msg, "ROUTE %d %d\n", my_node->id, 0);

        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER) {
                write(fd_edges[i], announce_msg, strlen(announce_msg));
            }
        }
        printf("Nó %d anunciado na rede.\n", my_node->id);


    } else if (strcmp(current_command->command, "m") == 0) {
        int dest = current_command->id;
        int next_hop;
        char chat_msg[160];

        if (!my_node->is_registered) {
            printf("Erro: Não está registado. Não pode executar 'message'.\n");
            return;
        }

        if (dest < 0 || dest >= 100) {
            printf("Erro: Destino inválido.\n");
            return;
        }

        if (dest == my_node->id) {
            printf("Mensagem recebida do nó %d: %s\n", my_node->id, current_command->message);
            return;
        }

        if (my_node->state[dest] != 0 || my_node->dist[dest] == INFINITO) {
            printf("Erro: Não existe rota ativa para o nó %d.\n", dest);
            return;
        }

        next_hop = my_node->succ[dest];
        if (next_hop == INVALID_NUMBER || fd_edges[next_hop] == INVALID_NUMBER) {
            printf("Erro: Não existe next hop ativo para o nó %d.\n", dest);
            return;
        }

        snprintf(chat_msg, sizeof(chat_msg), "CHAT %d %d %s\n", my_node->id, dest, current_command->message);
        write(fd_edges[next_hop], chat_msg, strlen(chat_msg));
        printf("Mensagem enviada para o nó %d via nó %d.\n", dest, next_hop);

    } else if (strcmp(current_command->command, "sr") == 0) {
        int dest = current_command->id;

        if (dest < 0 || dest >= 100) {
            printf("Erro: Destino inválido.\n");
            return;
        }

        printf("Routing para o destino %d:\n", dest);

        if (my_node->state[dest] == 0) {
            printf("Estado: expedição\n");

            if (my_node->dist[dest] == INFINITO || my_node->succ[dest] == INVALID_NUMBER) {
                printf("Distância: infinito\n");
                printf("Vizinho de expedição: nenhum\n");
            } else {
                printf("Distância: %d\n", my_node->dist[dest]);
                printf("Vizinho de expedição: %d\n", my_node->succ[dest]);
            }

        } else if (my_node->state[dest] == 1) {
            printf("Estado: coordenação\n");

            if (my_node->succ_coord[dest] == INVALID_NUMBER) {
                printf("succ_coord: nenhum\n");
            } else {
                printf("succ_coord: %d\n", my_node->succ_coord[dest]);
            }

            printf("Coordenação em curso com: ");
            bool exists = false;
            for (int i = 0; i < 100; i++) {
                if (fd_edges[i] != INVALID_NUMBER && my_node->coord[dest][i] == 1) {
                    if (exists) {
                        printf(", ");
                    }
                    printf("%d", i);
                    exists = true;
                }
            }

            if (!exists) {
                printf("nenhum");
            }
            printf("\n");

        } else {
            printf("Estado desconhecido: %d\n", my_node->state[dest]);
        }
    }

    return;
}


void sync_new_neighbor(NodeState *my_node, int new_neighbor_id) {
    char route_msg[32];

    for (int dest = 0; dest < 100; dest++) {
        
        // 1. O nó está em expedição para este destino e tem rota válida
        if (my_node->state[dest] == 0) {
            if (my_node->dist[dest] != INFINITO) { 
                snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
                write(fd_edges[new_neighbor_id], route_msg, strlen(route_msg));
            }
        } 
        // 2. O nó está em coordenação para este destino
        else if (my_node->state[dest] == 1) {
            // O regresso à expedição não depende do novo vizinho
            my_node->coord[dest][new_neighbor_id] = 0; 
        }
    }
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

        sync_new_neighbor(my_node, current_command->id);
    }

    freeaddrinfo(res);
}


void accept_connection(NodeState *my_node) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int vizinho_id;
    char buffer[BUFFER_TCP_SIZE]; 

    int new_fd = accept(fd_tcp_listen, (struct sockaddr*)&addr, &addrlen);
    if (new_fd == -1) return;

    int total_bytes = 0;
    int bytes_read;

    // ler tcp até /n
    while (total_bytes < BUFFER_TCP_SIZE - 1) {
        
        // Ler 1 byte de cada vez para o buffer
        bytes_read = read(new_fd, buffer + total_bytes, 1); 
        
        // Se der erro ou a pessoa desligar o cabo a meio da palavra
        if (bytes_read <= 0) {
            printf("Erro ou ligação terminada antes de enviar ID completo.\n");
            close(new_fd);
            return; 
        }

        total_bytes += bytes_read;
        
        if (buffer[total_bytes - 1] == '\n') {
            break; // Saímos do ciclo de leitura
        }
    }

    buffer[total_bytes] = '\0'; // Fechamos a string em C
    
    // Processar mensagem que ja chegou de certeza ao LF
    if (sscanf(buffer, "NEIGHBOR %d", &vizinho_id) == 1) {
        fd_edges[vizinho_id] = new_fd;
        printf("Nova conexão estabelecida com o nó %d\n", vizinho_id);

        sync_new_neighbor(my_node, vizinho_id);
    } else {
        // Protocol Violation! (Mandou lixo)
        printf("Aviso: Violação de protocolo. A fechar ligação.\n");
        close(new_fd); // PREVINE O LEAK
    }
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
