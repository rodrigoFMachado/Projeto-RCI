#include <stdio.h>
#include <stdlib.h>
#include <string.h>
    
#include "helper.h"


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


int word_processor(ParsedCommand *current_command) {
    char buffer_teclado[256] = {0}; // Buffer para ler a linha do teclado

    if (fgets(buffer_teclado, sizeof(buffer_teclado), stdin) != NULL) {
        char command[32] = {0}; // Para guardar a primeira palavra (o comando)

        // Lemos apenas a primeira palavra da linha para a variável 'command'
        if (sscanf(buffer_teclado, "%s", command) == 1) {

            // Verificar join OU j
            if (strcmp(command, "join") == 0 || strcmp(command, "j") == 0) {
                strcpy(current_command->command, "j"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s %s", current_command->net, current_command->id) != 2) {// get NET and ID 

                    printf("Erro: Argumentos inválidos. Uso: join net id\n");
                    return 1; // Retorna 1 para indicar erro
                }
            }

            // Verificar leave OU l
            else if (strcmp(command, "leave") == 0 || strcmp(command, "l") == 0) {
                strcpy(current_command->command, "l"); // Armazena o comando abreviado
                
                if (sscanf(buffer_teclado, "%*s") != 0) {
                    printf("Erro: Argumentos inválidos. Uso: leave \n");
                    return 1; // Retorna 1 para indicar erro
                }
            }

            // Verificar exit OU x
            else if (strcmp(command, "exit") == 0 || strcmp(command, "x") == 0) {
                if(strcmp(current_command->command, "l") == 0) {
                    strcpy(current_command->command, "x");
                    
                    printf("Comando 'leave' já foi processado, saindo...\n");

                } else {
                    strcpy(current_command->command, "l"); // Executa leave antes de sair

                    printf("Processando comando 'leave' antes de sair...\n");
                }

                return 2; // Código especial: processa leave e depois sai
            }

            // Verificar show nodes OU s
            else if (strcmp(command, "show nodes") == 0 || strcmp(command, "n") == 0) {
                strcpy(current_command->command, "n"); // Armazena o comando abreviado

                if (sscanf(buffer_teclado, "%*s %s", current_command->net) != 1) {// get ID 

                    printf("Erro: Argumentos inválidos. Uso: show nodes net\n");
                    return 1; // Retorna 1 para indicar erro
                }
            }
                
            else {
                printf("Comando desconhecido: %s\n", command);
                return 1; // Retorna 1 para indicar erro
            }  
        } 
        
        else {
            // Entrada vazia apenas enter
            return 1; // Retorna 1 para indicar erro
        }
    }

    return 0; // Retorna 0 para indicar sucesso
}