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


void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int n);

void process_COORD(NodeState *my_node, int neighbor_id, int dest);

void process_UNCOORD(NodeState *my_node, int neighbor_id, int dest);

void send_tcp_to_neighbor(int neighbor_id, const char *message) {
    if (neighbor_id < 0 || neighbor_id >= 100) {
        return;
    }

    if (fd_edges[neighbor_id] == -1) {
        return;
    }

    write(fd_edges[neighbor_id], message, strlen(message));
}


void send_tcp_to_all_neighbors(const char *message) {
    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != -1) {
            write(fd_edges[i], message, strlen(message));
        }
    }
}


bool coord_finished_for_dest(NodeState *my_node, int dest) {
    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != -1 && my_node->coord[dest][i] != 0) {
            return false;
        }
    }

    return true;
}



void process_tcp_message(NodeState *my_node, ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message) {
    int origin, dest, n;
    char *saveptr; // Ponteiro de estado obrigatório para o strtok_r funcionar bem

    // 1. A primeira chamada ao strtok_r corta a primeira fatia até ao primeiro '\n'
    char *line = strtok_r(raw_tcp_message, "\n", &saveptr);

    // 2. O ciclo while garante que processamos TODAS as fatias coladas no buffer
    while (line != NULL) {

        // Limpeza de segurança: Se estiveres a testar com telnet/netcat no Windows, 
        // às vezes fica um '\r' (carriage return) no final da linha. Isto limpa-o.
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }

        // AGORA USAMOS A VARIÁVEL 'line' EM VEZ DO 'raw_tcp_message' GIGANTE!

        if (sscanf(line, "ROUTE %d %d", &dest, &n) == 2) {
            if (dest < 0 || dest >= 100) {
                printf("Aviso: ROUTE com destino inválido recebida do nó %d.\n", neighbor_id);
            } else if (n < 0 || n >= INFINITO) { // Assumindo que INFINITO é 999
                printf("Aviso: ROUTE com distância inválida recebida do nó %d.\n", neighbor_id);
            } else {
                if (routing_monitor_active) {
                    printf("Recebido do nó %d: %s\n", neighbor_id, line);
                }
                process_ROUTE(my_node, neighbor_id, dest, n);
            }

        } else if (sscanf(line, "COORD %d", &dest) == 1) {
            if (dest < 0 || dest >= 100) {
                printf("Aviso: COORD com destino inválido recebida do nó %d.\n", neighbor_id);
            } else {
                if (routing_monitor_active) {
                    printf("Recebido do nó %d: %s\n", neighbor_id, line);
                }
                process_COORD(my_node, neighbor_id, dest);
            }

        } else if (sscanf(line, "UNCOORD %d", &dest) == 1) {
            if (dest < 0 || dest >= 100) {
                printf("Aviso: UNCOORD com destino inválido recebida do nó %d.\n", neighbor_id);
            } else {
                if (routing_monitor_active) {
                    printf("Recebido do nó %d: %s\n", neighbor_id, line);
                }
                process_UNCOORD(my_node, neighbor_id, dest);
            }

        // ATENÇÃO AQUI: Usei %[^\n] em vez de %s para poder ler frases com espaços!
        // O guião diz que a mensagem de CHAT tem no máximo 128 caracteres [cite: 96]
        } else if (sscanf(line, "CHAT %d %d %[^\n]", &origin, &dest, current_command->message) == 3) {
            if (origin < 0 || origin >= 100) {
                printf("Aviso: CHAT com origem inválida recebida do nó %d.\n", neighbor_id);
            } else if (dest < 0 || dest >= 100) {
                printf("Aviso: CHAT com destino inválido recebida do nó %d.\n", neighbor_id);
            } else {
                // Mensagem TCP válida do tipo CHAT recebida.
                printf("Mensagem recebida do nó %d: %s\n", origin, current_command->message);
                // process_CHAT(my_node, neighbor_id, origin, dest, current_command->message);
            }

        } else if (strcmp(line, "") != 0) {
            // Só imprime aviso se a linha não for puramente vazia
            printf("Aviso: Mensagem TCP mal formatada recebida do nó %d: %s\n", neighbor_id, line);
        }

        // 3. Pedimos ao strtok_r a PRÓXIMA fatia do mesmo buffer original
        line = strtok_r(NULL, "\n", &saveptr);
    }
}


void handle_link_drop(NodeState *my_node, int dropped_neighbor) {
    char coord_msg[32];
    
    for (int dest = 0; dest < 100; dest++) {
        if (dest == my_node->id) continue;

        // CASO 1: Estávamos em expedição e perdemos o nosso sucessor para este destino
        if (my_node->state[dest] == 0 && my_node->succ[dest] == dropped_neighbor) {
            bool waiting_for_someone = false; 

            my_node->state[dest] = 1;         
            my_node->succ_coord[dest] = -1;   
            my_node->dist[dest] = INFINITO;        
            my_node->succ[dest] = -1;

            if (routing_monitor_active) {
                printf("Ligação caiu! Entramos em COORD para o destino %d.\n", dest);
            }

            snprintf(coord_msg, sizeof(coord_msg), "COORD %d\n", dest);
            
            for (int i = 0; i < 100; i++) {
                if (fd_edges[i] != -1) {
                    my_node->coord[dest][i] = 1; 
                    send_tcp_to_neighbor(i, coord_msg);
                    waiting_for_someone = true; // Temos vizinhos reais para esperar!
                } else {
                    my_node->coord[dest][i] = 0;
                }
            }

            // O FIX: Se não temos mais nenhum vizinho, terminamos a coordenação IMEDIATAMENTE!
            if (!waiting_for_someone) {
                my_node->state[dest] = 0;
                printf("Saída imediata da coordenação para o destino %d (sem vizinhos ativos).\n", dest);
            }
        }
        // CASO 2: Já estávamos em coordenação e o vizinho morreu
        else if (my_node->state[dest] == 1) {
            
            // Já não ficamos à espera da resposta de um morto
            my_node->coord[dest][dropped_neighbor] = 0; 
            
            // Se foi ele que nos mandou congelar, esquecemos isso
            if (my_node->succ_coord[dest] == dropped_neighbor) {
                my_node->succ_coord[dest] = -1;
            }

            // O facto de não esperarmos por ele pode significar que a coordenação acabou agora!
            // Reutilizamos a tua função que verifica se a coordenação terminou:
            if (coord_finished_for_dest(my_node, dest)) {
                
                my_node->state[dest] = 0; // Volta à expedição
                
                if (my_node->dist[dest] != INFINITO) { // INFINITO = Infinito
                    char route_msg[32];
                    snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
                    send_tcp_to_all_neighbors(route_msg);
                }

                if (my_node->succ_coord[dest] != -1) {
                    char uncoord_msg[32];
                    snprintf(uncoord_msg, sizeof(uncoord_msg), "UNCOORD %d\n", dest);
                    send_tcp_to_neighbor(my_node->succ_coord[dest], uncoord_msg);
                    my_node->succ_coord[dest] = -1;
                }
            }
        }
    }
}


void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int n) {
    char route_msg[32];

    // Se o vizinho me está a tentar ensinar como chegar a mim próprio, ignoro!
    if (dest == my_node->id) {
        return; 
    }

    int nova_dist = n + 1;

    // REGRA DE OURO DO ROUTING: 
    // Atualizamos se for um atalho (nova_dist < dist atual) 
    // OU se a mensagem veio do vizinho que já usamos para chegar lá (succ == neighbor_id)
    if (nova_dist < my_node->dist[dest] || my_node->succ[dest] == neighbor_id) {
        
        // Só propagamos se a distância tiver EFETIVAMENTE mudado!
        if (my_node->dist[dest] != nova_dist) {
            my_node->dist[dest] = nova_dist;
            my_node->succ[dest] = neighbor_id;
        }

        if (my_node->state[dest] == 0) {
            snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            send_tcp_to_all_neighbors(route_msg);


            if (my_node->state[dest] == 0) {
                snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
                
                // Enviar para todos os vizinhos EXCETO quem nos enviou a rota (Split Horizon)
                for (int i = 0; i < 100; i++) {
                    if (fd_edges[i] != -1 && i != neighbor_id) {
                        write(fd_edges[i], route_msg, strlen(route_msg));
                    }
                }
                
                if (routing_monitor_active) {
                printf("ROUTE melhorou para o destino %d via nó %d com distância %d.\n",
                       dest, neighbor_id, my_node->dist[dest]);
                }
            }
        }
    }
}


void process_COORD(NodeState *my_node, int neighbor_id, int dest) {
    char route_msg[32];
    char uncoord_msg[32];
    char coord_msg[32];

    if (my_node->state[dest] == 1) {
        my_node->coord[dest][neighbor_id] = 0;
        snprintf(uncoord_msg, sizeof(uncoord_msg), "UNCOORD %d\n", dest);
        send_tcp_to_neighbor(neighbor_id, uncoord_msg);
        return;
    }

    if (my_node->state[dest] == 0 && neighbor_id != my_node->succ[dest]) {
        if (my_node->dist[dest] != INFINITO) {
            snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            send_tcp_to_neighbor(neighbor_id, route_msg);
        }

        snprintf(uncoord_msg, sizeof(uncoord_msg), "UNCOORD %d\n", dest);
        send_tcp_to_neighbor(neighbor_id, uncoord_msg);
        return;
    }

    if (my_node->state[dest] == 0 && neighbor_id == my_node->succ[dest]) {
        my_node->state[dest] = 1;
        my_node->succ_coord[dest] = my_node->succ[dest];
        my_node->dist[dest] = INFINITO;
        my_node->succ[dest] = -1;

        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != -1) {
                my_node->coord[dest][i] = 1;
            } else {
                my_node->coord[dest][i] = 0;
            }
        }

        snprintf(coord_msg, sizeof(coord_msg), "COORD %d\n", dest);
        send_tcp_to_all_neighbors(coord_msg);
        if (routing_monitor_active) {
            printf("Nó entrou em coordenação relativamente ao destino %d.\n", dest);
        }
    }
}


void process_UNCOORD(NodeState *my_node, int neighbor_id, int dest) {
    char route_msg[32];
    char uncoord_msg[32];

    if (my_node->state[dest] == 1) {
        my_node->coord[dest][neighbor_id] = 0;
    }

    if (coord_finished_for_dest(my_node, dest)) {
        my_node->state[dest] = 0;

        if (my_node->dist[dest] != INFINITO) {
            snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            send_tcp_to_all_neighbors(route_msg);
        }

        if (my_node->succ_coord[dest] != -1) {
            snprintf(uncoord_msg, sizeof(uncoord_msg), "UNCOORD %d\n", dest);
            send_tcp_to_neighbor(my_node->succ_coord[dest], uncoord_msg);
            my_node->succ_coord[dest] = -1;
        }
    }
}


void NodeState_inicialization(NodeState *my_node, bool joined_state, int net, int id) {
    // 1. Limpar a casa primeiro para TODOS os 100 destinos
    for (int i = 0; i < 100; i++) {
        my_node->dist[i] = INFINITO; 
        my_node->succ[i] = INVALID_NUMBER;
        my_node->state[i] = 0;
        my_node->succ_coord[i] = INVALID_NUMBER;
        for (int j = 0; j < 100; j++) {
            my_node->coord[i][j] = INVALID_NUMBER;
        }
    }

    // 2. Configurar o nó
    if (!joined_state){
        my_node->is_registered = false;
        my_node->net = INVALID_NUMBER;
        my_node->id = INVALID_NUMBER;        
    } else {
        my_node->is_registered = true;
        my_node->net = net;
        my_node->id = id;
        // Já NÃO metemos dist = 0 aqui! Ele fica a INFINITO até haver o comando "announce".
    }
}