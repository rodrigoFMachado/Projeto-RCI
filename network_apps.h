#ifndef NETWORK_APPS_H
#define NETWORK_APPS_H


void process_tcp_message(ParsedCommand *current_command, int neighbor_id, char *raw_tcp_message);

void NodeState_inicialization(NodeState *my_node, bool joined_state, int net, int id);

#endif 