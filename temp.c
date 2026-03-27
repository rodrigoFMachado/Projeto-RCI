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



/**
 * @brief Processa um comando do cliente e envia pedidos UDP para o servidor central de registo.
 *
 * Suporta os comandos:
 *   - "j" (join): regista o nó na rede especificada dentro do servidor.
 *   - "l" (leave): remove o registo do nó do servidor.
 *   - "n" (show nodes): mostra lista de nós na rede especificada.
 *   - "ae" (add edge): cria uma ligação TCP com outro nó.
 *
 * Esta função faz a gerencia de mensagens UDP conforme os commandos do usuário, envia via UDP
 * com `send_and_receiveUDP` e interpreta a resposta para atualizar `my_node`.
 *
 * @param my_node      estado local do nó com dados de registro e roteamento.
 * @param current_command comando parseado com atributos command/net/id/message.
 * @param myIP         endereço IPv4 local usado no registo (UDP REG).
 * @param myTCP        porta TCP local usada no registo (UDP REG).
 * @return true em caso de erro lógico ou comando inválido, false em sucesso.
 */
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


/**
 * @brief Envias e recebe um pacote UDP de/para o servidor de registro em loop de retransmissão.
 *
 * Comportamento:
 * - envia a mesma mensagem até 2 tentativas.
 * - faz timeout usando SO_RCVTIMEO configurado em `udp_starter`.
 * - recebe a resposta e atualiza o buffer `udp_message` com a mensagem recebida.
 *
 * @param udp_message buffer com mensagem de pedido, que também recebe a resposta.
 */
void send_and_receiveUDP(char *udp_message) {
    int n;

    struct sockaddr addr;
    socklen_t addrlen;

    // tentamos enviar a mensagem até 2 vezes, esperando resposta do servidor. Se não recebermos resposta, damos erro e saímos da função
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
        udp_message[0] = '\0'; // termina a string para nao ler lixo na ultima iteração
    }
    
    // DEBUGGING:
    // printf("Enviando mensagem UDP: %s\n", udp_message); // Mostra a mensagem que será enviada
    // printf("echo: %s\n", udp_message); // Mostra a resposta recebida do servidor

}


/**
 * @brief Inicializa socket UDP para comunicação com o servidor de registro.
 *
 * Cria o socket, ajusta timeout de recepção com `setsockopt(SO_RCVTIMEO)` e
 * resolve o `regIP:regUDP` com getaddrinfo.
 *
 * @param regIP  Endereço do registo (servidor central) em IPv4.
 * @param regUDP Porta UDP do servidor de registo.
 * @return ponteiro para struct addrinfo preenchida, usado em sendto/recvfrom.
 */
struct addrinfo *udp_starter(char *regIP, char *regUDP) { 
    struct addrinfo hints, *address;
    int errcode;

    fd_udp=socket(AF_INET,SOCK_DGRAM,0);//UDP socket
    if(fd_udp==-1)/*error*/exit(1);

    struct timeval read_timeout;
    read_timeout.tv_sec = 10; // Espera no máximo 10 segundos pela resposta do servidor
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


/**
 * @brief Interpreta e executa comandos de nível de conexão TCP e roteamento entre vizinhos.
 *
 * Esta rotina é chamada quando o nó envia comandos internos que afetam
 * as arestas TCP existentes ou a lógica de roteamento (tabela dist/succ).
 *
 * - "l" : fecha todas as arestas e sai.
 * - "ae" / "dae" : cria/renova ligação TCP com nó remoto ou localmente.
 * - "sg" / "em" : liga/desliga monitor de roteamento.
 * - "m" : envia mensagem CHAT via next-hop conhecido.
 * - "sr" : mostra estado de roteamento para destino.
 *
 * @param my_node estado local do nó (tabela de distâncias, sucessores, etc).
 * @param current_command comando parseado contendo `id`, `message`, etc.
 */
void handle_tcp_commands(NodeState *my_node, ParsedCommand *current_command) {


    if (strcmp(current_command->command, "l") == 0) {

        // O nó vai sair da rede. Cada ligação deve passar pelo mesmo tratamento
        // de queda para manter o estado de encaminhamento consistente.
        for(int i = 0; i < 100; i++) {
            if(fd_edges[i] != INVALID_NUMBER) {
                int dropped_fd = fd_edges[i];
                fd_edges[i] = INVALID_NUMBER;
                close(dropped_fd);
            }
        }
        printf("Arestas locais fechadas (Leave).\n");

    // chamamos função de conexão para comandos "ae" e "dae"
    } else if (strcmp(current_command->command, "ae") == 0 || strcmp(current_command->command, "dae") == 0) {
        
        connect_to_node(my_node, current_command);
    
    // usamos a estrutura local de fd_edges para mostrar as ligações ativas
    // index de fd_edges com alguma coisa diferente de -1 é um vizinho ativo
    } else if(strcmp(current_command->command, "sg") == 0){
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

    // se commando for "re" (remove edge), verificamos se a aresta existe, fechamos o socket e chamamos handle_link_drop
    // para processar a queda da ligação no protocolo de encaminhamento
    } else if (strcmp(current_command->command, "re") == 0) {
        
        if (fd_edges[current_command->id] != INVALID_NUMBER) {

            close(fd_edges[current_command->id]);
            fd_edges[current_command->id] = INVALID_NUMBER; // Liberta o slot!
            printf("Aresta com o nó %d removida.\n", current_command->id);

            handle_link_drop(my_node, current_command->id); // Processar a queda da ligação no protocolo de encaminhamento

        } else {
            printf("Erro: Não existe aresta ativa com o nó %d.\n", current_command->id);
        }

    // processamento de commando "a" (announce) para anunciar o nó na rede e iniciar o processo de construção de rotas
    } else if (strcmp(current_command->command, "a") == 0) {
        
        my_node->dist[my_node->id] = 0;
        my_node->succ[my_node->id] = my_node->id;

        char announce_msg[32];
        sprintf(announce_msg, "ROUTE %d %d\n", my_node->id, 0);
        
        // Anunciar a rota para si mesmo a todos os vizinhos ativos
        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER) {
                write(fd_edges[i], announce_msg, strlen(announce_msg));
            }
        }
        printf("Nó %d anunciado na rede.\n", my_node->id);


    } else if (strcmp(current_command->command, "sm") == 0) {
        routing_monitor_active = true; // flag global ativada para monitorização de encaminhamento
        printf("Monitorização de encaminhamento ativada.\n");


    } else if (strcmp(current_command->command, "em") == 0) {
        routing_monitor_active = false;// flag global desativada para monitorização de encaminhamento
        printf("Monitorização de encaminhamento desativada.\n");

    //NOTA: deviamos fazer aqui o mesmo que foi feita na receção de uma mensagem?
    //se a ligação for perdida a meio do envio da mensagem, isto não pode dar erro? enviar msg para nó que não tem Ligações ativas?
    // processamento do comando "m" (message) para enviar uma mensagem de chat para um destino específico usando o next-hop conhecido.
    } else if (strcmp(current_command->command, "m") == 0) {
        int dest = current_command->id;
        int next_hop;
        char chat_msg[160];

        // graceful handling de erros comuns antes de tentar enviar a mensagem
        if (!my_node->is_registered) {
            printf("Erro: Não está registado. Não pode executar 'message'.\n");
            return;
        }

        if (dest < 0 || dest >= 100) {
            printf("Erro: Destino inválido.\n");
            return;
        }

        // verificar se a mensagem é para si mesmo
        if (dest == my_node->id) {
            printf("Mensagem recebida do nó %d: %s\n", my_node->id, current_command->message);
            return;
        }

        // impossivel mandar mensagem para um destino que não tem rota ativa
        if (my_node->state[dest] != 0 || my_node->dist[dest] == INFINITO) {
            printf("Erro: Não existe rota ativa para o nó %d.\n", dest);
            return;
        }

        next_hop = my_node->succ[dest];
        if (next_hop == INVALID_NUMBER || fd_edges[next_hop] == INVALID_NUMBER) {
            printf("Erro: Não existe next hop ativo para o nó %d.\n", dest);
            return;
        }

        // encaminhamento para destino pretendido usando o next-hop conhecido.
        snprintf(chat_msg, sizeof(chat_msg), "CHAT %d %d %s\n", my_node->id, dest, current_command->message);
        write(fd_edges[next_hop], chat_msg, strlen(chat_msg));
        printf("Mensagem enviada para o nó %d via nó %d.\n", dest, next_hop);

    // processamento do comando "sr" (show route) para mostrar o estado de roteamento para um destino específico.
    } else if (strcmp(current_command->command, "sr") == 0) {
        int dest = current_command->id;

        // graceful handling de erros comuns antes de tentar enviar a mensagem
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


/**
 * @brief Envia rotas conhecidas ao novo vizinho TCP.
 *
 * Quando uma aresta TCP é estabelecida com `new_neighbor_id`, suportamos
 * sincronização inicial enviando todas as rotas válidas na tabela local.
 *
 * @param my_node estado local do nó com dist/succ/coord.
 * @param new_neighbor_id ID do novo vizinho estabelecido em fd_edges.
 */
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


/**
 * @brief Estabelece ligação TCP ativa ao nó remoto e anuncia identidade via protocolo NEIGHBOR.
 *
 * Fluxo:
 * 1. Resolve o IP/porta TCP obtidos via consulta UDP de CONTACT.
 * 2. Connect no socket (TCP handshake)
 * 3. Salva `fd_out` em fd_edges[id]
 * 4. Envia "NEIGHBOR %02d\n" para notificar o vizinho do ID local.
 * 5. Chama sync_new_neighbor para enviar rotas de atualização inicial.
 *
 * @param my_node estado local contendo id e tabela de roteamento.
 * @param current_command contém `tempTCP_IP`/`tempTCP_Port`/`id`.
 */
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


/**
 * @brief Aceita uma nova conexão TCP de entrada e valida o handshake NEIGHBOR.
 *
 * O nó entra em modo não-blocking nesta função, aceita a conexão de `fd_tcp_listen`
 * e lê linha terminada em \n para obter o ID do vizinho. Esta abordagem evita
 * processar pacotes TCP parcialmente recebidos.
 *
 * @param my_node estado local para atualizar fd_edges e sincronizar rotas.
 */
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
    // ler byte a byte até encontrar "\n", para garantir que se todos os dados não chegarem de uma vez ou se ligação fosse fechada,
    // não ficamos com lixo no buffer ou a ler dados incompletos
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


/**
 * @brief Inicializa o socket TCP local de escuta e faz bind/listen.
 *
 * Este socket recebe conexões de novos vizinhos P2P
 * que se ligam pelo comando "ae".
 *
 * @param myIP Endereço local/INADDR_ANY (IPv4) para bind.
 * @param myTCP Porta TCP para bind/listen.
 */
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