#ifndef NETWORK_APPS_H
#define NETWORK_APPS_H


void process_tcp_message(NodeState *my_node, ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message);

void handle_link_drop(NodeState *my_node, int dropped_neighbor);

void NodeState_inicialization(NodeState *my_node, int joined_state, int net, int id);

#endif 
