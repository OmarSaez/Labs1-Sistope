// desafio1.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

//funcion principal
int main(int argc, char *argv[]) {
    int n_procesos = -1;

    // Revisamos los argumentos en busca del numero de hijos (procesos)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            n_procesos = atoi(argv[i + 1]);
        }
    }

    //Si el numero de procesos es 0 o menor indicamos un error y detenemos todo
    if (n_procesos <= 0) {
        printf("Uso: %s -p <número de procesos>\n", argv[0]);
        exit(1);
    }

    //Preparamos cuantos hijos deberiamos de tener
    pid_t pids[n_procesos];

    //Generamos el ciclo que ira creando hijos
    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();
        if (pid < 0) {//Salida en caso de error
            perror("fork");
            exit(1);
        } else if (pid == 0) {//Mostramos que en cada ciclo indiquen quienes son
            // Proceso hijo
            printf("Soy el hijo %d con PID %d\n", i, getpid());
            exit(0);
        } else {
            // Proceso padre
            pids[i] = pid;
        }
    }

    // El padre espera a todos los hijos
    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    return 0;
}
