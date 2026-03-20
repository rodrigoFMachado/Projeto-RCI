#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>


#include "main_manager.h"
#include "network_apps.h"


static void send_tcp_to_neighbor(int neighbor_id, const char *message) {
    if (neighbor_id < 0 || neighbor_id >= 100) {
        return;
    }

    if (fd_edges[neighbor_id] == -1) {
        return;
    }

    write(fd_edges[neighbor_id], message, strlen(message));
}


static void send_tcp_to_all_neighbors(const char *message) {
    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != -1) {
            write(fd_edges[i], message, strlen(message));
        }
    }
}


static bool coord_finished_for_dest(NodeState *my_node, int dest) {
    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != -1 && my_node->coord[dest][i] != 0) {
            return false;
        }
    }

    return true;
}


void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int n) {
    char route_msg[32];

    if (n + 1 < my_node->dist[dest]) {
        my_node->dist[dest] = n + 1;
        my_node->succ[dest] = neighbor_id;

        if (my_node->state[dest] == 0) {
            snprintf(route_msg, sizeof(route_msg), "ROUTE %d %d\n", dest, my_node->dist[dest]);
            send_tcp_to_all_neighbors(route_msg);
            printf("ROUTE melhorou para o destino %d via nó %d com distância %d.\n",
                   dest, neighbor_id, my_node->dist[dest]);
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
        if (my_node->dist[dest] != INT_MAX) {
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
        my_node->dist[dest] = INT_MAX;
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
        printf("Nó entrou em coordenação relativamente ao destino %d.\n", dest);
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

        if (my_node->dist[dest] != INT_MAX) {
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


void process_tcp_message(NodeState *my_node, ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message) {
    int origin, dest, n;

    if (sscanf(raw_tcp_message, "ROUTE %d %d", &dest, &n) == 2)
    {
        if (dest < 0 || dest >= 100)
        {

            printf("Aviso: ROUTE com destino inválido recebida do nó %d.\n", neighbor_id);
        }
        else if (n < 0 || n == INT_MAX)
        {

            printf("Aviso: ROUTE com distância inválida recebida do nó %d.\n", neighbor_id);
        }
        else
        {
            process_ROUTE(my_node, neighbor_id, dest, n);
        }
    }
    else if (sscanf(raw_tcp_message, "COORD %d", &dest) == 1)
    {
        if (dest < 0 || dest >= 100)
        {

            printf("Aviso: COORD com destino inválido recebida do nó %d.\n", neighbor_id);
        }
        else
        {
            process_COORD(my_node, neighbor_id, dest);
        }
    }
    else if (sscanf(raw_tcp_message, "UNCOORD %d", &dest) == 1)
    {
        if (dest < 0 || dest >= 100)
        {

            printf("Aviso: UNCOORD com destino inválido recebida do nó %d.\n", neighbor_id);
        }
        else
        {
            process_UNCOORD(my_node, neighbor_id, dest);
        }
    }
    else if (sscanf(raw_tcp_message, "CHAT %d %d %s", &origin, &dest, current_command->message) == 3)
    {
        if (origin < 0 || origin >= 100)
        {

            printf("Aviso: CHAT com origem inválida recebida do nó %d.\n", neighbor_id);
        }
        else if (dest < 0 || dest >= 100)
        {

            printf("Aviso: CHAT com destino inválido recebida do nó %d.\n", neighbor_id);
        }
        else
        {

            // Mensagem TCP válida do tipo CHAT recebida do nó neighbor_id.
            // tratar de processo pos receber uma mensagem CHAT aqui. Por agora, só imprimimos a mensagem de debug.
        }
    }
    else
    {
        printf("Aviso: Mensagem TCP mal formatada recebida do nó %d: %s\n", neighbor_id, raw_tcp_message);
    }
}


void NodeState_inicialization(NodeState *my_node, int joined_state, int net, int id) {

    if (joined_state == 1){
        my_node->is_registered = true;
        my_node->net = net;
        my_node->id = id;
        my_node->dist[id] = 0;
        my_node->succ[id] = id;
        return;
    }else{
        my_node->is_registered = false;
        my_node->net = -1;
        my_node->id = -1;
    }

    for (int i = 0; i < 100; i++) {
        my_node->dist[i] = INT_MAX; // Infinito
        my_node->succ[i] = -1;
        my_node->state[i] = 0;
        my_node->succ_coord[i] = -1;

        for (int j = 0; j < 100; j++) {
            my_node->coord[i][j] = -1; // -1 para indicar "sem coordenador" ou "nenhum"
        }
    }

}
