#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <limits.h>


#include "main_manager.h"
#include "communication_handling.h"
#include "network_apps.h"

#define INVALID_NUMBER -1

int fd_udp, fd_tcp_listen; // Sockets e endereços globais
int fd_edges[100]; // fd de conexões TCP ativas, max 100 conexões
struct addrinfo *address_udp; // Endereços globais para UDP e TCP
bool routing_monitor_active = false; // Por omissão, as mensagens de encaminhamento não são mostradas



bool word_processor(NodeState *my_node, ParsedCommand *current_command); 


/**
 * @brief Controlador principal do nó P2P: gerencia sockets UDP/TCP, processa comandos e roteia mensagens.
 *
 * Inicializa o nó com estado vazio, cria sockets UDP e TCP para comunicação com servidor
 * e vizinhos. Usa `select()` para monitorizar simultaneamente: input do teclado, conexões
 * TCP de entrada e mensagens de vizinhos. Suporta ciclo de vida completo: join -> add edges
 * -> mensagens -> leave -> exit.
 *
 * @param myIP endereço local para bind do socket TCP de escuta.
 * @param myTCP porta TCP local do nó para aceitar conexões.
 * @param regIP endereço do servidor de registro (UDP central).
 * @param regUDP porta UDP do servidor de registro.
 */
void manager_of_all(char *myIP, char *myTCP, char *regIP, char *regUDP) {
    int counter, maxfd;
    bool exit_failure;
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

    NodeState_inicialization(my_node, false, INVALID_NUMBER, INVALID_NUMBER); // Inicializa o estado do nó como não registado

    address_udp = udp_starter(regIP, regUDP);
    tcp_starter(myIP, myTCP); 

    for (int i = 0; i < 100; i++) fd_edges[i] = INVALID_NUMBER;

    while (1) {
        FD_ZERO(&rfds);

        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd_tcp_listen, &rfds);

        exit_failure = false;

        maxfd = fd_tcp_listen; // conexões podem morrer

        // Adicionar conexões TCP ativas (fd_edges)
        for (int n = 0; n < 100; n++) { 
            if (fd_edges[n] != INVALID_NUMBER) {
                FD_SET(fd_edges[n], &rfds);
                if (fd_edges[n] > maxfd) maxfd = fd_edges[n];
            }
        }

        counter = select(maxfd + 1, &rfds, (fd_set *)NULL, (fd_set *)NULL, (struct timeval *) NULL);
        if (counter < 0) /*error*/ exit(1);
        if (counter == 0) continue; // Timeout, volta a esperar


        // ==========================================
        // A. TECLADO (Stdin)
        // ==========================================
        if (FD_ISSET(STDIN_FILENO, &rfds)) { // teclado, envia msg UDP

            exit_failure = word_processor(my_node, current_command);
            
            if (exit_failure) { 
                continue; // Se houve um erro no processamento do comando, volta para o início do loop
            }

            exit_failure = false;

            if (strcmp(current_command->command, "x") == 0) { // condicao que executa break no final, apenas para exit

                if(my_node->is_registered) {
                    // Se estiver registado, faz o leave final
                    strcpy(current_command->command, "l"); // Executa leave final
                }
                
                // nao existe opcao para x nas seguintes funcoes, logo nao ha risco
                handle_udp_commands(my_node, current_command, myIP, myTCP); // faz o leave final

                handle_tcp_commands(my_node, current_command); // faz o remove edge final (loop de todos os vizinhos ativos)
                
                printf("A sair...\n");
                break;
            }


            exit_failure = handle_udp_commands(my_node, current_command, myIP, myTCP);

            
            if(exit_failure) {
                // Se o contacto falhou, não vamos tentar processar o segmento TCP porque não temos os dados necessários
                // Medida necessária para evitar segfault de ler a variável vazia, de má resposta de CONTACT
                continue;
            }

            handle_tcp_commands(my_node, current_command);
        }


        // ==========================================
        // B. PEDIDO DE CONEXÃO TCP (fd_tcp_listen)
        // ==========================================
        if (FD_ISSET(fd_tcp_listen, &rfds)) {

            accept_connection(my_node);

        }


        // ==========================================
        // C. LER MENSAGENS DOS VIZINHOS JÁ CONECTADOS
        // ==========================================
        for (int i = 0; i < 100; i++) {
            if (fd_edges[i] != INVALID_NUMBER && FD_ISSET(fd_edges[i], &rfds)) {
                
                process_tcp_message(my_node, current_command, i);
            }
        }
    }

    free(current_command);
    free(my_node);

    for (int i = 0; i < 100; i++) {
        if (fd_edges[i] != INVALID_NUMBER) {
            close(fd_edges[i]);
            fd_edges[i] = INVALID_NUMBER;
        }
    }

    freeaddrinfo(address_udp);

    close(fd_udp);
    close(fd_tcp_listen);

    return;
}



/**
 * @brief Parser de comandos digitados pelo utilizador: valida sintaxe e extrai parâmetros.
 *
 * Função faz a validação de um comando e lida com erros graciosamente, imprimindo mensagens
 * de erro claras e retornando true para sinalizar que o comando não deve ser processado.
 *
 * Le uma linha do stdin com `fgets()` e identifica o comando ("j", "ae", "m", etc).
 * Valida argumentos esperados usando `sscanf()`. Se a sintaxe for correta, atualiza
 * `current_command` e retorna false. Se comando desconhecido ou argumentos inválidos,
 * imprime erro e retorna true (sinaliza skip próximo processamento).
 *
 * Comandos suportados:
 *   - "j net id": join (registo no servidor)
 *   - "l": leave (sair da rede)
 *   - "x": exit (encerrar aplicação)
 *   - "ae id": add edge (criar ligação TCP)
 *   - "re id": remove edge (fechar ligação TCP)
 *   - "m dest msg": message (enviar CHAT)
 *   - "sr dest": show routing (mostrar rota para destino)
 *   - "dj net id": direct join (criar nó sem servidor)
 *   - "dae id ip port": direct add edge (conectar sem servidor)
 *
 * @param my_node estado local do nó (usado para validações).
 * @param current_command estrutura preenchida com comando parseado.
 * @return true se erro ou comando vazio, false em sucesso (comando válido parseado).
 */
bool word_processor(NodeState *my_node, ParsedCommand *current_command) {
    char buffer_teclado[64] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command_first_w[16] = {0}; // Para guardar a primeira palavra do comando


        if (sscanf(buffer_teclado, "%s", command_first_w) == 1) {

            // Verificar join
            if (strcmp(command_first_w, "j") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %d", &current_command->net, &current_command->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return true; 
                }
                strcpy(current_command->command, "j"); 
            }

            // Verificar show nodes
            else if (strcmp(command_first_w, "n") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->net) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: show neighbors net\n");
                    return true;
                }
                strcpy(current_command->command, "n");
            }

            // Verificar leave
            else if (strcmp(command_first_w, "l") == 0) {

                if (!my_node->is_registered) {
                    printf("Erro: Não está registado. Não pode executar leave.\n");
                    return true; 
                }
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave\n");
                    return true; 
                }

                strcpy(current_command->command, "l"); 
            }

            // Verificar exit
            else if (strcmp(command_first_w, "x") == 0) {
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: exit\n");
                    return true; 
                }

                strcpy(current_command->command, "x");
            }

            // Verificar add edge
            else if(strcmp(command_first_w, "ae") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: add edge id\n");
                    return true; 
                }
                if(!my_node->is_registered) {
                    printf("Erro: Não é um nó.\n");
                    return true;
                }
                strcpy(current_command->command, "ae");
            }

            // Verificar remove edge
            else if(strcmp(command_first_w, "re") == 0) {

                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) !=1) {
                    printf("Erro: Argumentos inválidos. Uso: remove edge id\n");
                    return true; 
                }
                strcpy(current_command->command, "re");
            }

            // Verificar show neighbors
            else if (strcmp(command_first_w, "sg") == 0) {

                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: show neighbors\n");
                    return true; 
                }
                strcpy(current_command->command, "sg");
            }

            // Verificar announce
            else if (strcmp(command_first_w, "a") == 0) {

                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: announce\n");
                    return true; 
                }
                strcpy(current_command->command, "a");
            }

            // Verificar show routing
            else if (strcmp(command_first_w, "sr") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s %d", &current_command->id) != 1) {
                    printf("Erro: Argumentos inválidos. Uso: show routing dest\n");
                    return true; 
                }
                strcpy(current_command->command, "sr");
            }

            // Verificar start monitor 
            else if (strcmp(command_first_w, "sm") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: start monitor\n");
                    return true; 
                }
                routing_monitor_active = true;
                printf("Monitorização de encaminhamento ativada.\n");

                strcpy(current_command->command, "sm");
            }

            // Verificar end monitor 
            else if (strcmp(command_first_w, "em") == 0) {
                // usado id para capturar nó de destino
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: end monitor\n");
                    return true; 
                }
                routing_monitor_active = false;
                printf("Monitorização de encaminhamento desativada.\n");

                strcpy(current_command->command, "em");
            }

            // Verificar message 
            else if (strcmp(command_first_w, "m") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %128[^\n]", &current_command->id, current_command->message) != 2) {
                    printf("Erro: Argumentos inválidos. Uso: message dest message\n");
                    return true; 
                }
                strcpy(current_command->command, "m");
            }

            // Verificar direct join
            else if (strcmp(command_first_w, "dj") == 0) {

                if (sscanf(buffer_teclado, "%*s %d %d", &current_command->net, &current_command->id) != 2) {
                    printf("Erro: Argumentos inválidos. Uso: direct join net id\n");
                    return true; 
                } 
                
                strcpy(current_command->command, "dj");

                NodeState_inicialization(my_node, true, current_command->net, current_command->id);

                printf("Nó criado localmente com net %d e id %d.\n", my_node->net, my_node->id);
            }                    

            // Verificar direct add edge
            else if (strcmp(command_first_w, "dae") == 0) {

                if(!my_node->is_registered) { 
                    printf("Erro: Não é um nó.\n");
                    return true; 
                }

                if (sscanf(buffer_teclado, "%*s %d %s %s", &current_command->id, current_command->tempTCP_IP, current_command->tempTCP_Port) != 3) {
                    printf("Erro: Argumentos inválidos. Uso: direct add edge id idIP idTCP\n");
                    return true; 
                }
                
                strcpy(current_command->command, "dae"); 
            }

            else {
                printf("Comando desconhecido: %s\n", command_first_w);
                    return true; 
            }  

        } else {
            // Entrada vazia apenas enter
                    return true; 
        }
    }
    return false; // Retorna 0 para indicar sucesso
}


/**
 * @brief Parser de argumentos da linha de comandos.
 *
 * Valida numero de argumentos e atribui endereços/portas a pointers passados
 *
 * @param argc número de argumentos da linha de comandos.
 * @param argv array de strings com argumentos.
 * @param myIP pointer a preencher com IP local do nó.
 * @param myTCP pointer a preencher com porta TCP local.
 * @param regIP pointer a preencher com IP do servidor de registro (ou default).
 * @param regUDP pointer a preencher com porta UDP do servidor (ou default).
 * @return 0 em sucesso, exit(1) se argumentos inválidos.
 */
int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP) 
{
    if (argc < 3 || argc > 5) {
        printf("Uso: %s IP TCP [regIP] [regUDP]\n", argv[0]);
        exit(1);
    }

    *myIP = argv[1];
    *myTCP = argv[2];

    if (argv[3] != NULL && argv[4] != NULL) {
        *regIP = argv[3];
        *regUDP = argv[4];
    }

    return 0;
}
