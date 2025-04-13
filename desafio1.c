#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

/*
 * Autores: Omar Elias Saez Arias y Enzo Ivo San Martin Pavez
 * Proyecto: Laboratorio de procesos en anillo - Simulación de paso de token con señales
 *
 * Descripción general de la idea:
 *
 * 1. El proceso padre comienza creando todos los procesos hijos necesarios y guarda sus PIDs.
 *
 * 2. Una vez que todos los hijos están creados, el padre les envía a cada uno el PID a ellos. Cada hijo, al recibirla
 *    guardan en la variable global su "siguiente" en el anillo.
 *
 * 3. El primer proceso que fue creado recibe inicialmente el "token" (un número entero) entregado por el padre. Ese token es
 *    pasado de un proceso a otro en forma de señales, simulando el movimiento en anillo.
 *
 * 4. Cada vez que un proceso recibe el token, lo reduce en un número aleatorio. Si un proceso hijo
 *    genera un token negativo, ese proceso se "elimina" a sí mismo y avisa al padre que ha muerto.
 *
 * 5. El padre, al ser notificado, actualiza la lista de PIDs eliminando al proceso caído, y
 *    a la vez cual es el proceso antes del eliminado, para enviar el nuevo PID del proceso siguiente
 *    realizando asi la nueva conexion en el anillo.
 *
 * 6. Así, el ciclo continúa con cada ronda pasando el Token original por parte del padre al primero de la lista, 
 *    hasta que queda solamente un proceso activo en el anillo. Ese es el "ganador".
 *
 * 7. El proceso padre permanece vivo durante toda la ejecución, monitoreando todo el sistema, realizando nuevas conexiones y
 *    finaliza solo cuando el declara un ganador.
 *
 */

//Variables globales
pid_t next_pid = -1;
int token = -1;
int max_decremento = 0;
pid_t mi_pid;
pid_t padre_pid;
int token_inicial = -1;

pid_t *pids;
int n_procesos = -1;

// Entradas: señal SIGUSR1 con el valor del PID del siguiente proceso
// Salidas: ninguna
// Descripción: Manejador que asigna el PID del siguiente proceso en el anillo a la variable global next_pid
void recibir_siguiente(int sig, siginfo_t *info, void *context) {
    next_pid = info->si_value.sival_int;
}

// Entradas: señal SIGUSR2 con el valor del token
// Salidas: ninguna
// Descripción: Decrementa el token de forma aleatoria, lo imprime y lo pasa al siguiente proceso. Si el token es negativo, lo envía al padre y termina el proceso
void manejar_token(int sig, siginfo_t *info, void *context) {
    token = info->si_value.sival_int;

    printf("\nProceso %d ; Token recibido: %d ; ", getpid(), token);
    fflush(stdout);

    int decremento = rand() % (max_decremento + 1);
    token -= decremento;

    printf("Token resultante: %d ", token);
    fflush(stdout);

    if (token < 0) {
        sigqueue(padre_pid, SIGUSR2, (union sigval){ .sival_int = token });
        exit(0);
    } else {
        sigqueue(next_pid, SIGUSR2, (union sigval){ .sival_int = token });
    }
}

// Entradas: señal SIGUSR2 con valor del token negativo y el PID del proceso que murió
// Salidas: ninguna
// Descripción: El padre elimina el proceso que recibió un token negativo, reorganiza el anillo, le manda el nuevo PID al ante proceso eliminado y si solo queda uno, lo declara ganador y termina
void padre_maneja_token_negativo(int sig, siginfo_t *info, void *context) {
    pid_t muerto = info->si_pid;
    int token_negativo = info->si_value.sival_int;

    printf("(Proceso %d es eliminado)", muerto);
    fflush(stdout);

    int index_muerto = -1;
    for (int i = 0; i < n_procesos; i++) {
        if (pids[i] == muerto) {
            index_muerto = i;
            break;
        }
    }
    if (index_muerto == -1) return;

    pid_t anterior = pids[(index_muerto - 1 + n_procesos) % n_procesos];
    pid_t siguiente = pids[(index_muerto + 1) % n_procesos];

    if (anterior == siguiente) {
        printf("\nProceso %d es el ganador\n", anterior);
        fflush(stdout);
        kill(anterior, SIGTERM);
        free(pids);
        exit(0);
    }

    for (int i = index_muerto; i < n_procesos - 1; i++) {
        pids[i] = pids[i + 1];
    }
    n_procesos--;

    sigqueue(anterior, SIGUSR1, (union sigval){ .sival_int = siguiente });
    usleep(100000);

    sigqueue(pids[0], SIGUSR2, (union sigval){ .sival_int = token_inicial });
}

// Entradas: argumentos de línea de comandos -p (procesos), -M (máximo decremento), -t (token inicial)
// Salidas: retorna 0 si termina correctamente
// Descripción: Función principal que crea procesos hijos, establece manejadores, forma el anillo, y lanza el token inicial. Termina cuando queda un solo proceso.
int main(int argc, char *argv[]) {
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
        printf("Argumentos inválidos\n");
        exit(1);
    }

    pids = malloc(sizeof(pid_t) * n_procesos);
    padre_pid = getpid();
    srand(time(NULL));

    struct sigaction sa_negativo;
    sa_negativo.sa_flags = SA_SIGINFO;
    sa_negativo.sa_sigaction = padre_maneja_token_negativo;
    sigemptyset(&sa_negativo.sa_mask);
    sigaction(SIGUSR2, &sa_negativo, NULL);

    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            mi_pid = getpid();

            struct sigaction sa1, sa2;
            sa1.sa_flags = SA_SIGINFO;
            sa1.sa_sigaction = recibir_siguiente;
            sigemptyset(&sa1.sa_mask);
            sigaction(SIGUSR1, &sa1, NULL);

            sa2.sa_flags = SA_SIGINFO;
            sa2.sa_sigaction = manejar_token;
            sigemptyset(&sa2.sa_mask);
            sigaction(SIGUSR2, &sa2, NULL);

            pause();
            while (1) pause();
        } else if (pid > 0) {
            pids[i] = pid;
        } else {
            perror("Error creando procesos");
            exit(1);
        }
    }

    sleep(1);
    for (int i = 0; i < n_procesos; i++) {
        pid_t siguiente = pids[(i + 1) % n_procesos];
        sigqueue(pids[i], SIGUSR1, (union sigval){ .sival_int = siguiente });
    }

    sleep(1);
    printf("[Padre] Enviando token inicial %d al proceso %d\n", token_inicial, pids[0]);
    fflush(stdout);
    sigqueue(pids[0], SIGUSR2, (union sigval){ .sival_int = token_inicial });

    while (1) pause();

    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    free(pids);
    return 0;
}
