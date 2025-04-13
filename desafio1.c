#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
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


 pid_t next_pid = -1;
 int token = -1;
 int max_decremento = 0;
 pid_t mi_pid;
 pid_t padre_pid;
 int token_inicial = -1;
 
 // Manejador para recibir el PID del siguiente proceso (fase inicial)
 void recibir_siguiente(int sig, siginfo_t *info, void *context) {
     next_pid = info->si_value.sival_int;
 }
 
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
 
 void padre_maneja_token_negativo(int sig, siginfo_t *info, void *context) {
     pid_t muerto = info->si_pid;
     int token_negativo = info->si_value.sival_int;
 
     printf("(Proceso %d es eliminado)", muerto);
     fflush(stdout);
 
     extern pid_t *pids;
     extern int n_procesos;
 
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
 
     // Reenviar token inicial al primero de la lista
     sigqueue(pids[0], SIGUSR2, (union sigval){ .sival_int = token_inicial });
 }
 
 pid_t *pids;
 int n_procesos = -1;
 
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
     // Enviar a cada hijo el PID de su siguiente en el anillo
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
 