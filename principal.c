#include <stdio.h>
#include "interprete.h"
#include "operaciones.h"
#include <string.h>
#include <stdlib.h>


/* === NUEVO ===
   Consola interactiva: 'ejecuta <archivo>' y 'salir'.
   - Esto reemplaza el main que antes abría de golpe "a.asm".
*/
int main(void) {
    char cmd[256];

    puts("Laboratorio SO - consola");
    puts("Comandos: ejecuta <archivo> | salir");

    for (;;) {
        printf("$ ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;

        // quita salto(s) de línea
        rtrim(cmd);

        // salir
        if (strncmp(cmd, "salir", 5) == 0) {
            puts("Saliendo...");
            break;
        }

        // ejecuta <archivo>
        if (strncmp(cmd, "ejecuta ", 8) == 0) {
            const char *ruta = cmd + 8;
            while (*ruta == ' ') ruta++; // salta espacios extra
            if (*ruta == '\0') {
                puts("Uso: ejecuta <archivo.asm>");
                continue;
            }
            int rc = ejecutar_archivo(ruta);
            if (rc != 0) {
                printf("Fallo al ejecutar '%s' (codigo %d)\n", ruta, rc);
            }
            continue;
        }

        puts("Comando no reconocido. Usa: ejecuta <archivo> | salir");
    }

    return 0;
}

