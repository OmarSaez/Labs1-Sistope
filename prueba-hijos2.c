#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {

    int n_procesos = -1; // Lo inicializamos como error

    // Buscamos el -p para saber la cantidad de procesos requeridos
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            n_procesos = atoi(argv[i + 1]); // Cantidad de procesos
        }
    }

    // Paso el fork y no encontró la cantidad de procesos
    if (n_procesos <= 0) {
        printf("Cantidad de procesos no especificada o numero no valido\n");
        exit(1);
    }

    pid_t pids[n_procesos]; // Arreglo para guardar los ids

    // Ciclos para crear procesos hijos
    for (int i = 0; i < n_procesos; i++) {

        pid_t pid = fork();

        // Verificamos que el hijo se haya creado bien
        if (pid < 0) {
            perror("Hubo un error al crear el hijo");
            exit(1);
        }

        // Para los procesos hijos
        if (pid == 0) {
            // Suspendemos el proceso hijo momentáneamente para poder rellenar la lista pids
            //sleep(2);

            // Calculamos el índice del siguiente proceso con el operador % para formar un anillo
            int indice = i;
            int indice_sig = (i + 1) % n_procesos;

            //Prueba para ver la lista
            //for (int j = 0; j < n_procesos; j++) {
            //    printf("%d- pids[%d] = %d\n", i, j, pids[j]);
            //}



            printf("Soy el hijo %d, mi PID es: %d, y el siguiente es %d con PID: %d\n", 
                   indice, getpid(), indice_sig, pids[indice_sig]);

            exit(0);
        }
        // Para el proceso padre
        else {
            // Guardamos el pid en la lista de pids
            pids[i] = pid;
        }
    }

    // Esperar a que terminen todos los hijos
    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    return 0;
}
