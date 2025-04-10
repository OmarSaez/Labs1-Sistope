#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

//--Idea: se crea un manejador de señales, despues se configura SIGUSR1 para que use el manejador de señales
//Despues se recada la cantidad de hijos deseada de la linea de comandos, se crean y se ponen en pausa a la espera de UNA señal
//EL padre crea todos los hijos y recolecta los PIDs en una lista y despues le va mandando a cada proceso hijo la señal del siguiente proceso hijo en al anillo

pid_t next_pid = -1; //Seteamos en un pid inicial de error por si falla la señal

// Primer paso: Manejador de señales
void signal_handler(int sig, siginfo_t *info, void *context) {
    // El proceso hijo recibe la señal
    printf("Proceso %d recibió la señal. El siguiente proceso es %d.\n", getpid(), info->si_value.sival_int);
    
    // Al recibir la señal, pasamos el token al siguiente proceso
    next_pid = info->si_value.sival_int; // Guardamos el siguiente PID para la próxima señal
    if (next_pid != -1) {
        // Enviar el token al siguiente proceso
        sigqueue(next_pid, SIGUSR1, (union sigval) { .sival_int = next_pid });
    }
    
    // Terminamos el proceso hijo
    exit(0); 
}

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

    pid_t pids[n_procesos];//Lista que iremos guardando los PIDs

    // Segundo paso: Configuración de la señal
    struct sigaction sa; //Se declara que se usara señales
    sa.sa_flags = SA_SIGINFO; //Se indica que se usara la estrucutra sigqueue(pid, SIGUSR1, valor)
    sa.sa_sigaction = signal_handler; //Se asigna el manejador de señales
    sigemptyset(&sa.sa_mask); //Se limpia las señales
    sigaction(SIGUSR1, &sa, NULL); //Registramos el manejaor de señales para SIGUSR1 (señal dada para users)

    // Crear los procesos hijos
    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();

        if (pid == 0) { // Código para el hijo
            // El hijo debe esperar una señal
            pause();
        } else if (pid > 0) { // Código para el padre
            pids[i] = pid;  // Guardamos el PID
        } else {
            perror("Fork failed");
            exit(1);
        }
    }

    // Tercer paso: Enviar la señal al primer hijo, pasando el PID del siguiente proceso
    for (int i = 0; i < n_procesos; i++) {
        next_pid = pids[(i + 1) % n_procesos];  // El siguiente proceso es el siguiente en el anillo
        printf("El padre va a mandar a PID: %d, el PID: %d \n", pids[i], next_pid);//Prueba de que el padre manda señales
        sigqueue(pids[i], SIGUSR1, (union sigval) { .sival_int = next_pid });//Envio de señal con el formato ya declarado
    }

    // Esperar a que todos los procesos terminen
    for (int i = 0; i < n_procesos; i++) {
        wait(NULL);
    }

    return 0;
}
