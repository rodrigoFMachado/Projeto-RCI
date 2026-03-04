#ifndef INTERFACE_H
#define INTERFACE_H


typedef struct ParsedCommand_{
    char command[4]; // max 3 letras
    char net[4]; // max 3 digitos
    char id[3];  // max 2 digitos
}ParsedCommand;


int interface(int argc, char *argv[], char **myIP, char **myTCP, char **regIP, char **regUDP);

int word_processor(ParsedCommand *current_command); // função auxiliar para processar palavras de uma string

#endif