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
 * 2. Una vez que todos los hijos están creados, el padre les envía a cada uno la lista completa
 *    de PIDs. Cada hijo, al recibirla, identifica cuál es su propio PID y así determina quién es
 *    su "siguiente" en el anillo.
 *
 * 3. Solo uno de los procesos recibe inicialmente el "token" (un número entero). Ese token es
 *    pasado de un proceso a otro en forma de señales, simulando el movimiento en anillo.
 *
 * 4. Cada vez que un proceso recibe el token, lo reduce en un número aleatorio. Si el token se
 *    vuelve negativo, ese proceso se "elimina" a sí mismo y avisa al padre que ha muerto.
 *
 * 5. El padre, al ser notificado, actualiza la lista de PIDs eliminando al proceso caído, y
 *    a la vez cual es el proceso antes del eliminado, para enviar el nuevo PID del proceso siguiente
 *    realizando asi la nueva conexion en el anillo.
 *
 * 6. Así, el ciclo continúa con cada ronda pasando el Token original por parte del padre al primero de la lista, 
 *    hasta que queda solamente un proceso activo en el anillo. Ese es el "ganador".
 *
 * 7. El proceso padre permanece vivo durante toda la ejecución, monitoreando todo el sistema y
 *    finaliza solo cuando el declara un ganador.
 *
 */

// Variables globales
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

// Entrada: Ninguna
// Salidas: Ninguna
// Descripción: Función que muestra el correcto uso de argumentos para poder ejecutar el codigo y despues cierra el programa
void mostrar_uso() {
    printf("Uso correcto:\n");
    printf("./desafio1.exe -p <n_procesos> -M <max_decremento> -t <token_inicial>\n");
    printf("Ejemplo: ./desafio1.exe -p 5 -M 10 -t 50\n");
    exit(1);
}

// Entradas: argumentos de línea de comandos -p (procesos), -M (máximo decremento), -t (token inicial)
// Salidas: retorna 0 si termina correctamente
// Descripción: Función principal que crea procesos hijos, establece manejadores, forma el anillo, y lanza el token inicial. Termina cuando queda un solo proceso.
int main(int argc, char *argv[]) {

    // Verificar cantidad de argumentos
    if (argc != 7) {
        printf("Error: Número incorrecto de argumentos.\n");
        mostrar_uso();
    }

    // Se verifica que los argumentos sean validos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            n_procesos = atoi(argv[i + 1]);
            if (n_procesos <= 1) {
                printf("Error: El número de procesos (-p) debe ser mayor que 1.\n");
                mostrar_uso();
            }
        } else if (strcmp(argv[i], "-M") == 0) {
            max_decremento = atoi(argv[i + 1]);
            if (max_decremento <= 0) {
                printf("Error: El decremento máximo (-M) debe ser mayor que 0.\n");
                mostrar_uso();
            }
        } else if (strcmp(argv[i], "-t") == 0) {
            token_inicial = atoi(argv[i + 1]);
            if (token_inicial <= 0) {
                printf("Error: El valor inicial del token (-t) debe ser mayor que 0.\n");
                mostrar_uso();
            }
        }
    }

    // Verificación final de parámetros
    if (n_procesos == -1 || max_decremento == -1 || token_inicial == -1) {
        printf("Error: Faltan parámetros obligatorios.\n");
        mostrar_uso();
    }

    // Creacion de lista donde se guardan los PID's
    pids = malloc(sizeof(pid_t) * n_procesos);
    padre_pid = getpid();
    srand(time(NULL));

    // Configuracion de señales para el padre
    struct sigaction sa_negativo;
    sa_negativo.sa_flags = SA_SIGINFO;
    sa_negativo.sa_sigaction = padre_maneja_token_negativo;
    sigemptyset(&sa_negativo.sa_mask);
    sigaction(SIGUSR2, &sa_negativo, NULL);

    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            mi_pid = getpid();

            //Configuracion de señales para los hijos, via para mandar PID's
            struct sigaction sa1, sa2;
            sa1.sa_flags = SA_SIGINFO;
            sa1.sa_sigaction = recibir_siguiente;
            sigemptyset(&sa1.sa_mask);
            sigaction(SIGUSR1, &sa1, NULL);

            // Configuracion de señales para los hijos, via para mandar el token
            sa2.sa_flags = SA_SIGINFO;
            sa2.sa_sigaction = manejar_token;
            sigemptyset(&sa2.sa_mask);
            sigaction(SIGUSR2, &sa2, NULL);

            // Se entra los hijos en un While que nunca acabara para que siempre puedan recibir señales (Se termina en los manejadores)
            pause();
            while (1) pause();
        } else if (pid > 0) {
            pids[i] = pid;
        } else {
            perror("Error creando procesos");
            exit(1);
        }
    }

    // Se duerme el padre para asegurar que esten todos los hijos y despues se manda los PIDs del siguiente creando el anillo
    sleep(1);
    for (int i = 0; i < n_procesos; i++) {
        pid_t siguiente = pids[(i + 1) % n_procesos];
        sigqueue(pids[i], SIGUSR1, (union sigval){ .sival_int = siguiente });
    }

    // El padre pasa el primer token de todos y da inicio al desafio
    sleep(1);
    sigqueue(pids[0], SIGUSR2, (union sigval){ .sival_int = token_inicial });

    // Se queda en un while que nunca termina para poder manejar a los hijos que se vayan elimiando
    while (1) pause();

    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    free(pids);
    return 0;
}
