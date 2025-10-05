#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "interprete.h"

/*
Laboratorio Sistemas Operativos
Modulo 1: Simulador de CPU con instrucciones basicas.
Equipo #6: Claudio Castro Murillo, Oscar Amador Aguilar Calvillo, Brandon Hernandez Vargas.
Semestre: 2025-2026
*/

int main(void) {
    char cmd[512];

    puts("Laboratorio SO - consola");
    puts("Comandos: ejecuta <archivo1> <archivo2> ... | salir");

    for (;;) {
        printf("$ ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        rtrim(cmd);

        if (strncmp(cmd, "salir", 5) == 0) {
            puts("Saliendo...");
            break;
        }

        if (strncmp(cmd, "ejecuta ", 8) == 0) {
            const char *rest = cmd + 8;
            while (*rest == ' ') rest++;
            if (*rest == '\0') {
                puts("Uso: ejecuta <archivo1> <archivo2> ...");
                continue;
            }
            int rc = ejecutar_archivo(rest); // ahora acepta varios archivos separados por espacios
            if (rc != 0) {
                printf("Fallo al ejecutar (codigo %d)\n", rc);
            }
            continue;
        }

        puts("Comando no reconocido. Usa: ejecuta <archivo1> <archivo2> ... | salir");
    }

    return 0;
}