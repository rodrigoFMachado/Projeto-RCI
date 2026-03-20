#ifndef NETWORK_APPS_H
#define NETWORK_APPS_H


void process_tcp_message(NodeState *my_node, ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message);

void process_ROUTE(NodeState *my_node, int neighbor_id, int dest, int n);

void process_COORD(NodeState *my_node, int neighbor_id, int dest);

void process_UNCOORD(NodeState *my_node, int neighbor_id, int dest);

void NodeState_inicialization(NodeState *my_node, int joined_state, int net, int id);

#endif 
