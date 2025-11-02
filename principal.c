#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "interprete.h"

/*
Laboratorio Sistemas Operativos
Modulo 2: Simulador de CPU con multiples procesos.
Equipo #6: Claudio Castro Murillo, Oscar Amador Aguilar Calvillo, Brandon Hernandez Vargas.
Semestre: 2025-2026
*/

/*
en mi funcion main hace la consola interactiva y ejecuta comandos
*/
int main(void)
{
    char cmd[512];

    puts("Laboratorio SO - consola");
    puts("Comandos: ejecuta <archivo1> <archivo2> ... | kill |salir");

    for (;;)
    {
        printf("$ ");
        if (!fgets(cmd, sizeof(cmd), stdin))
            break;
        rtrim(cmd);

        if (strncmp(cmd, "salir", 5) == 0)
        {
            puts("Saliendo...");
            break;
        }

        if (strncmp(cmd, "ejecuta ", 8) == 0)
        {
            const char *rest = cmd + 8;
            while (*rest == ' ')
                rest++;
            if (*rest == '\0')
            {
                puts("Uso: ejecuta <archivo1> <archivo2> ...");
                continue;
            }
            int rc = ejecutar_archivo(rest); // ya acepta varios archivos separados por espacios
            if (rc != 0)
            {
                printf("Fallo al ejecutar (codigo %d)\n", rc);
            }
            continue;
        }
        //nuevo comando kill
        if (strncmp(cmd, "kill ", 5) == 0)
        {
            const char *rest = cmd + 5;
            while (*rest == ' ')
                rest++;
            if (!*rest)
            {
                puts("Uso: kill <id>");
                continue;
            }
            int id = atoi(rest);
            if (id <= 0)
            {
                puts("ID invalido.");
                continue;
            }
            kill_proceso(id);
            continue;
        }

        puts("Comando no reconocido. Usa: ejecuta <archivo1> <archivo2> ... | kill <id> | salir");
    }

    return 0;
}
