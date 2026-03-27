#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>


#include "main_manager.h"
#include "network_apps.h"

#define INVALID_NUMBER -1
#define INFINITO 999


void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int distance);

void process_COORD(NodeState *my_node, int neighbor_id, int dest);

void process_UNCOORD(NodeState *my_node, int neighbor_id, int dest);

void process_CHAT(NodeState *my_node, int neighbor_id, int origin, int dest, const char *message);


bool coord_finished_for_dest(NodeState *my_node, int dest) {
    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != INVALID_NUMBER && my_node->coord[dest][i] != 0) {
            return false;
        }
    }

    return true;
}


/// @brief Chamada quando existem mensagens TCP a processar de um vizinho.
/// @param my_node 
/// @param current_command 
/// @param neighbor_id 
/// @param raw_tcp_message 
void process_tcp_message(NodeState *my_node, ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message) {

    char buffer[2 * 128]; // mensagem total (+que 128)

    int total_bytes = 0;
    int bytes_read;

    while (total_bytes < (2 * 128 - 1)) {
        bytes_read = read(fd_edges[neighbor_id], buffer, sizeof(buffer) - 1);

        if (bytes <= 0) {
        // Se bytes == 0, o vizinho desligou-se normalmente (remove edge ou exit).
        // Se bytes == -1, a ligação caiu de forma bruta.
        printf("O nó %d desconectou-se (aresta removida).\n", neighbor_id);

        int dropped_neighbor = fd_edges[neighbor_id]; // Guardar o socket do vizinho
        fd_edges[neighbor_id] = INVALID_NUMBER;       // Limpamos a aresta do nosso lado
        handle_link_drop(my_node, neighbor_id);       // Processar a queda da ligação no protocolo de encaminhamento

        close(dropped_neighbor); // Fechar o socket da ligação caída;

        return;
        }
    }
    
    /*
    ACHO QUE ESTA PARTE EM BAIXO FICA DENTRO DO CICLO TAMBEM, NAO PRECISA DE ELSE PORQUE BYTES <= 0 DA RETURN

    E ESSA PARTE FICA AGORA A PARTE QUE VES NO ACCEPT CONECTIONS, DO COMMUN HANDLING

    CONTUDO NAO PODE SER IGUAL, PORQUE AO CONTRARIO DO ACCEPT CONNECTION, EM QUE SO CHEGA UMA MENSAGE DE NEIGHBOUR

    AQUI PODEM CHEGAR VARIAS CONCATENADAS, DAI SE TER O STRTOK_R PARA CORTAR AS MENSAGENS E PROCESSAR UMA A UMA

    OU SEJA NO ACCEPT LE SE ATE \n, AQUI LE SE TUDO O QUE CHEGOU (EI QUAL E O STOPPING POINT????) MAS O IMPORTANTE E TER EM MEMORIA O BUFFER TODO, TER A CERTEZA QUE JA CHEGOU TUDO E DEPOIS E QUE SE TOKENIZA? 

    OU SECALAHR DA PARA IR LENDO, A CADA \N MANDA SE PROCESS COORD, ROUTE, etc, E DEPOIS VOLTA AO INICIO DO CICLO WHLE, A PROCURA DE OUTRO \n

    YA ENT FICA ASSIM SEM STRTOK, PORQUE AGORA NOS FAZEMOS O NOSSO LOW LEVEL STRTOK, LENDO ATE \N E SO DEPOIS ENVIAR PARA PROCESSAR, E CONTINUAR A LER O BUFFER SEM SAIR DA FUNCAO, 
    
    
    
    
    */




    // Recebemos texto do vizinho!
    buffer[bytes] = '\0';
    process_tcp_message(my_node, current_command, neighbor_id);

    int origin, dest, distance;
    char *saveptr; 

    // A primeira chamada ao strtok_r corta a primeira fatia até ao primeiro '\n'
    char *line = strtok_r(raw_tcp_message, "\n", &saveptr);

    
    while (line != NULL) {

        if (sscanf(line, "ROUTE %d %d", &dest, &distance) == 2) {
            if (routing_monitor_active) {
                printf("[RECEIVED] %s\n", line);
            }
            process_ROUTE(my_node, neighbor_id, dest, distance);
    

        } else if (sscanf(line, "COORD %d", &dest) == 1) {
            if (routing_monitor_active) {
                printf("[RECEIVED] %s\n", line);
            }
            process_COORD(my_node, neighbor_id, dest);
       

        } else if (sscanf(line, "UNCOORD %d", &dest) == 1) {
            if (routing_monitor_active) {
                printf("[RECEIVED] %s\n", line);
            }
            process_UNCOORD(my_node, neighbor_id, dest);

        // O guião diz que a mensagem de CHAT tem no máximo 128 caracteres (le frases com espacos)
        } else if (sscanf(line, "CHAT %d %d %128[^\n]", &origin, &dest, current_command->message) == 3) {
            if (routing_monitor_active) {
                printf("[RECEIVED] %s\n", line);
            }
            process_CHAT(my_node, neighbor_id, origin, dest, current_command->message);

        } else if (strcmp(line, "") != 0) {
            // Só imprime aviso se a linha não for puramente vazia
            printf("Aviso: Mensagem TCP mal formatada recebida do nó %d: %s\n", neighbor_id, line);
        }

        // Pedimos ao strtok_r a PRÓXIMA fatia do mesmo buffer original
        line = strtok_r(NULL, "\n", &saveptr);
    }
}


void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int distance) {
    char msg_to_send[32];

    // Se o destino sou eu próprio, a minha distância é sempre 0, ignoro o que me tentam ensinar.
    if (dest == my_node->id) {
        return;
    }

    // quando em coord, nao propaga rotas
    if (my_node->state[dest]) {
        return;
    }
    
    // caso em que distancia melhorou ou não tínhamos rota para este destino
    if ((distance + 1) < my_node->dist[dest]) {
        my_node->dist[dest] = distance + 1;
        my_node->succ[dest] = neighbor_id;

        printf("ROUTE melhorou para o destino %d via nó %d com distância %d.\n", dest, neighbor_id, my_node->dist[dest]);

        // SÓ PROPAGA SE ESTIVER EM EXPEDIÇÃO NORMAL!
        if (my_node->state[dest] == 0) {
            snprintf(msg_to_send, sizeof(msg_to_send), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            for (int i = 0; i < 100; i++) {
                if (fd_edges[i] != INVALID_NUMBER) {
                    write(fd_edges[i], msg_to_send, strlen(msg_to_send));
                    if (routing_monitor_active) {
                        printf("[SENT] %s", msg_to_send);
                    }
                }
            }
        }
        return;
    }

}


void process_COORD(NodeState *my_node, int neighbor_id, int dest) {
    char msg_to_send[32];

    // Se state[t] = 1, então envia (exped, t) a j
    if (my_node->state[dest] == 1) {
        snprintf(msg_to_send, sizeof(msg_to_send), "UNCOORD %d\n", dest);
        write(fd_edges[neighbor_id], msg_to_send, strlen(msg_to_send));
        if(routing_monitor_active) {
            printf("[SENT] %s", msg_to_send);
        }
        return;
    }

    // Se state[t] = 0 e j != succ[t], então envia (route, t, dist) e (exped, t) a j
    if (my_node->state[dest] == 0 && neighbor_id != my_node->succ[dest]) {
        // Prevenir elementos com succ[t] = -1 de enviarem rotas inválidas (infinite)
        if (my_node->dist[dest] != INFINITO) {
            snprintf(msg_to_send, sizeof(msg_to_send), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            write(fd_edges[neighbor_id], msg_to_send, strlen(msg_to_send));
            if(routing_monitor_active) {
                printf("[SENT] %s", msg_to_send);
            }
        }

        snprintf(msg_to_send, sizeof(msg_to_send), "UNCOORD %d\n", dest);
        write(fd_edges[neighbor_id], msg_to_send, strlen(msg_to_send));
        if(routing_monitor_active) {
            printf("[SENT] %s", msg_to_send);
        }
        return;
    }

    // Se state[t] = 0 e j == succ[t], então state[t] = 1; succ_coord[t] = succ[t]; dist[t] = infinito; succ[t] = -1, e envia (coord, t) a todos os seus vizinhos
    if (my_node->state[dest] == 0 && neighbor_id == my_node->succ[dest]) {
        my_node->state[dest] = 1;
        my_node->succ_coord[dest] = my_node->succ[dest];
        my_node->dist[dest] = INFINITO;
        my_node->succ[dest] = INVALID_NUMBER;

        snprintf(msg_to_send, sizeof(msg_to_send), "COORD %d\n", dest);
        
        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER) {
                my_node->coord[dest][i] = 1; // Registamos que estamos à espera deste vizinho
                write(fd_edges[i], msg_to_send, strlen(msg_to_send));
                if (routing_monitor_active) {
                    printf("[SENT] %s", msg_to_send);
                }
            } else {
                my_node->coord[dest][i] = 0; // Slots vazios não contam
            }
        }


        printf("Recebido COORD do vizinho de expedição %d. A entrar em coordenação para o destino %d.\n", neighbor_id, dest);
    }
    return;
}


void process_UNCOORD(NodeState *my_node, int neighbor_id, int dest) {
    char msg_to_send[32];

    // Só atua em COORD
    if (my_node->state[dest] == 1) {
        
        // Se state = 1, então coord[j] := 0
        my_node->coord[dest][neighbor_id] = 0;

        // Se coord[j, t] = 0 para todo o vizinho j, então.
        if (coord_finished_for_dest(my_node, dest)) {
            
            // state[t] := 0
            my_node->state[dest] = 0; 

            if (routing_monitor_active) {
                printf("A coordenação para o destino %d terminou! Regressámos à expedição.\n", dest);
            }

            // Se dist[t] != infinito, então envia (route, t, dist) a todos vizinhos
            if (my_node->dist[dest] != INFINITO) {
                snprintf(msg_to_send, sizeof(msg_to_send), "ROUTE %d %d\n", dest, my_node->dist[dest]);
                for (int i = 0; i < 100; i++) {
                    if (fd_edges[i] != INVALID_NUMBER) {
                        write(fd_edges[i], msg_to_send, strlen(msg_to_send));
                        if (routing_monitor_active) {
                            printf("[SENT] %s", msg_to_send);
                        }
                    }
                }
            }

            // Se succ_coord[t] != -1, então envia (exped, t) a succ_coord[t]
            if (my_node->succ_coord[dest] != INVALID_NUMBER) {
                snprintf(msg_to_send, sizeof(msg_to_send), "UNCOORD %d\n", dest);
                write(fd_edges[my_node->succ_coord[dest]], msg_to_send, strlen(msg_to_send));
                if (routing_monitor_active) {
                    printf("[SENT] %s", msg_to_send);
                }

                my_node->succ_coord[dest] = INVALID_NUMBER;
            }
        }
    }
}


void process_CHAT(NodeState *my_node, int neighbor_id, int origin, int dest, const char *message) {
    char chat_msg[160];
    int next_hop;

    if (dest == my_node->id) {
        printf("Mensagem recebida do nó %d: %s\n", origin, message);
        return;
    }

    if (my_node->state[dest] != 0 || my_node->dist[dest] == INFINITO) {
        printf("Aviso: Sem rota ativa para reencaminhar CHAT de %d para %d.\n", origin, dest);
        return;
    }

    next_hop = my_node->succ[dest];
    if (next_hop == INVALID_NUMBER || fd_edges[next_hop] == INVALID_NUMBER) {
        printf("Aviso: Next hop inválido para reencaminhar CHAT de %d para %d.\n", origin, dest);
        return;
    }

    if (next_hop == neighbor_id) {
        printf("Aviso: Rota para %d aponta de volta para o nó %d. CHAT descartado para evitar loop.\n", dest, neighbor_id);
        return;
    }

    snprintf(chat_msg, sizeof(chat_msg), "CHAT %d %d %s\n", origin, dest, message);
    write(fd_edges[next_hop], chat_msg, strlen(chat_msg));

    if (routing_monitor_active) {
        printf("[SENT] %s", chat_msg);
    }
}

void handle_link_drop(NodeState *my_node, int dropped_neighbor) {
    char msg_to_send[32];

    // VERIFICAÇÃO DE ISOLAMENTO
    bool has_neighbors = false;
    for (int k = 0; k < 100; k++) {
        if (fd_edges[k] != INVALID_NUMBER) {
            has_neighbors = true;
            break; 
        }
    }
    
    // Rever a tabela para cada destino
    for (int dest = 0; dest < 100; dest++) {
        if (dest == my_node->id) continue;

        // CASO 1: expedição, caminho desapareceu porque o vizinho caiu
        if (my_node->state[dest] == 0 && my_node->succ[dest] == dropped_neighbor) {

            // Não entra em coord se não existem vizinhos
            if (!has_neighbors) {
                my_node->dist[dest] = INFINITO;
                my_node->succ[dest] = INVALID_NUMBER; //vizinho desapareceu
                
                if (routing_monitor_active) {
                    printf("Isolado: Rota para %d apagada. Em expedição.\n", dest);
                }
                continue;
            }

            // Com outros vizinhos entramos em coord
            my_node->state[dest] = 1;         
            my_node->succ_coord[dest] = INVALID_NUMBER;   // cabo caiu
            my_node->dist[dest] = INFINITO;   
            my_node->succ[dest] = INVALID_NUMBER;         

            if (routing_monitor_active) {
                printf("Ligação caiu! Entramos em coordenação para o destino %d.\n", dest);
            }

            snprintf(msg_to_send, sizeof(msg_to_send), "COORD %d\n", dest);
            
            for (int i = 0; i < 100; i++) {
                if (fd_edges[i] != INVALID_NUMBER) {
                    my_node->coord[dest][i] = 1; 
                    write(fd_edges[i], msg_to_send, strlen(msg_to_send));
                } else {
                    my_node->coord[dest][i] = 0; 
                }
            }
        }

        // Em coord, o vizinho que caiu pode ser um dos coordenadores que estamos à espera
        else if (my_node->state[dest] == 1) {
            
            my_node->coord[dest][dropped_neighbor] = 0; // coord terminada
            
            if (my_node->succ_coord[dest] == dropped_neighbor) {
                my_node->succ_coord[dest] = INVALID_NUMBER; // esquecer sucessor que nos mandou coord
            }

            // Verificar se coord acabou
            if (coord_finished_for_dest(my_node, dest)) {
                
                // state[t] := 0
                my_node->state[dest] = 0;
                
                // Se dist[t] != infinito, então envia (route, t, dist) a todos vizinhos
                if (my_node->dist[dest] != INFINITO) {
                    snprintf(msg_to_send, sizeof(msg_to_send), "ROUTE %d %d\n", dest, my_node->dist[dest]);
                    for (int i = 0; i < 100; i++) {
                        if (fd_edges[i] != INVALID_NUMBER) {
                            write(fd_edges[i], msg_to_send, strlen(msg_to_send));
                        }
                    }
                }

                // Se succ_coord[t] != -1, então envia (exped, t) a succ_coord[t]
                if (my_node->succ_coord[dest] != INVALID_NUMBER) {
                    snprintf(msg_to_send, sizeof(msg_to_send), "UNCOORD %d\n", dest);
                    write(fd_edges[my_node->succ_coord[dest]], msg_to_send, strlen(msg_to_send));
                    my_node->succ_coord[dest] = INVALID_NUMBER; 
                }
            }
        }
    }
}

/// @brief Chamada no início. Depois, apenas chamada quando acontece o join, ou direct join. 
/// @param my_node 
/// @param joined_state 
/// @param net 
/// @param id 
void NodeState_inicialization(NodeState *my_node, bool joined_state, int net, int id) {

    // Configurar o nó
    if (!joined_state){
        my_node->is_registered = false;
        my_node->net = INVALID_NUMBER;
        my_node->id = INVALID_NUMBER;        
    } else {
        // Se juntar, limpar as tabelas todas
        for (int i = 0; i < 100; i++) {
            my_node->dist[i] = INFINITO; 
            my_node->succ[i] = INVALID_NUMBER;
            my_node->state[i] = 0; // expedição por omissão
            my_node->succ_coord[i] = INVALID_NUMBER;
            for (int j = 0; j < 100; j++) {
                my_node->coord[i][j] = 0; // Sem coordenação no início
            }
        }

        my_node->is_registered = true;
        my_node->net = net;
        my_node->id = id;
    }
}
