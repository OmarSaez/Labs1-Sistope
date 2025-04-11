#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

//Ojo no se hizo que siguiera el proceso, solo sapan token hasta que alguno muera y queda colgada la consola

//--Idea: Se tienen dos tipos de señales, una es la via para pasar el PID(SIGUSR1) y el segundo es la via(SIGUSR2) para pasar el Token con el primer pid enviado
//La cantidad de hijos, token y maximo numero aleatorio se deben pasar por consola como: -t token -M maximoNumero -p CantidadPorcesos

pid_t next_pid = -1;       // PID del siguiente proceso en el anillo
int token = -1;            // Token que será decrementado
int max_decremento = 0;    // Valor máximo para el decremento aleatorio

// Manejador para recibir el PID del siguiente proceso (fase inicial)
void recibir_siguiente(int sig, siginfo_t *info, void *context) {
    next_pid = info->si_value.sival_int;
}

// Manejador para recibir el token
void manejar_token(int sig, siginfo_t *info, void *context) {
    token = info->si_value.sival_int;

    printf("\nProceso %d ; Token recibido: %d ;", getpid(), token);

    // Generar decremento aleatorio
    int decremento = rand() % (max_decremento + 1); //Se crea el numero aleatorio
    token -= decremento;

    printf("Proceso %d le resta %d al token ; Token resultante: %d\n", getpid(), decremento, token);

    if (token < 0) {
        printf("(Proceso %d: el token es negativo. Eliminado)", getpid());
        exit(0);
    } else {
        sigqueue(next_pid, SIGUSR2, (union sigval){ .sival_int = token });
    }
}

int main(int argc, char *argv[]) {
    int n_procesos = -1;

    int token_inicial;
    // Buscar argumentos -p (cantidad de procesos), -M (máximo decremento) y -t (token)
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            n_procesos = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-M") == 0) {
            max_decremento = atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "-t") == 0) {
            token_inicial = atoi(argv[i + 1]);
        }
    }

    if (n_procesos <= 0 || max_decremento <= 0) {
        printf("Cantidad de procesos no especificada o numero no valido\n");
        exit(1);
    }

    pid_t pids[n_procesos];
    srand(time(NULL)); // Semilla para números aleatorios

    // Configurar recepción del siguiente PID (fase 1)
    struct sigaction sa_siguiente;
    sa_siguiente.sa_flags = SA_SIGINFO;
    sa_siguiente.sa_sigaction = recibir_siguiente;
    sigemptyset(&sa_siguiente.sa_mask);
    sigaction(SIGUSR1, &sa_siguiente, NULL);

    // Configurar recepción del token (fase 2)
    struct sigaction sa_token;
    sa_token.sa_flags = SA_SIGINFO;
    sa_token.sa_sigaction = manejar_token;
    sigemptyset(&sa_token.sa_mask);
    sigaction(SIGUSR2, &sa_token, NULL);

    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();

        //Hijo
        if (pid == 0) {
            pause(); // Espera el PID del siguiente proceso
            while (1) {//Hacemos que el while se ejecute de manera infinita y el menjetador fase 2 sea el que contenga el exit(0)
                pause(); // Espera el token
            }
        //Padre    
        } else if (pid > 0) {
            pids[i] = pid;
        //Error de creacion
        } else {
            perror("Error en fork, no se pudo crear proceso hijo se detuvo todo");
            exit(1);
        }
    }

    // Enviar a cada hijo el PID de su siguiente en el anillo
    for (int i = 0; i < n_procesos; i++) {
        pid_t siguiente = pids[(i + 1) % n_procesos];
        sigqueue(pids[i], SIGUSR1, (union sigval){ .sival_int = siguiente });
    }

    sleep(1); // Esperar un momento para asegurar que todos los procesos hijos hayan recibido su siguiente PID

    // Enviar el token inicial al primer proceso
    printf("El padre envía el token con valor %d al proceso %d\n", token_inicial, pids[0]);//Print para vizualizar el recorrido
    sigqueue(pids[0], SIGUSR2, (union sigval){ .sival_int = token_inicial });//Envio del token ingresado por consola

    // Esperar a que todos los hijos terminen
    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    return 0;
}
