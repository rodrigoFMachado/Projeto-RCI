#include <stdio.h>
#include <stdlib.h>
    
#include "interface.h"


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