#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "main_manager.h"


int main(int argc, char *argv[]) 
{
    char *myIP, *myTCP;
    char *regIP = "193.136.138.142"; 
    char *regUDP = "59000"; 

    srand((unsigned int)time(NULL)); // Inicializa a semente do gerador de números aleatórios

    interface(argc, argv, &myIP, &myTCP, &regIP, &regUDP);

    manager_of_all(myIP, myTCP, regIP, regUDP);
    
    
    return 0;
}