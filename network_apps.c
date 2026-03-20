#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>


#include "main_manager.h"
#include "network_apps.h"


void process_tcp_message(ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message) {
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

            // Mensagem TCP válida do tipo ROUTE recebida do nó neighbor_id.
            // tratar de processo pos receber uma mensagem ROUTE aqui, se necessário. Por agora, só imprimimos a mensagem de debug.
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

            // Mensagem TCP válida do tipo COORD recebida do nó neighbor_id.
            // tratar de processo pos receber uma mensagem COORD aqui.
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

            // Mensagem TCP válida do tipo UNCOORD recebida do nó neighbor_id.
            // tratar de processo pos receber uma mensagem UNCOORD aqui.
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