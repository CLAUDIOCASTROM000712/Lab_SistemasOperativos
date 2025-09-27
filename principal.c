#include <stdio.h> //libreria de entrada/salida
#include "interprete.h" //incluyo la libreria de interprete
#include "operaciones.h" //incluyo la libreria de operaciones
#include <string.h> //libreria de strings osea cadenas
#include <stdlib.h> //libreria de utilidades generales(malloc, atoi, etc).
/*
Laboratorio Sistemas Operativos
Modulo 1: Simulador de CPU con instrucciones basicas.
Equipo #6: Claudio Castro Murillo, Oscar Amador Aguilar Calvillo, Brandon Hernandez Vargas.
Semestre: 2025-2026
*/

/* 
   Consola interactiva: 'ejecuta <archivo>' y 'salir'.
   - Esto reemplaza el main que antes abría de golpe "a.asm".
*/
int main(void) {
    /*
        Se declara un buffer cmd de 256 caracteres para almacenar los comandos que escribe el usuario en la consola.
    */
    char cmd[256];

    /*
        Muestra un mensaje de bienvenida.

        Explica los comandos disponibles al usuario:

        ejecuta <archivo> → ejecuta instrucciones de un archivo .asm. (ejemplo: ejecuta a.asm)

        salir → termina el programa.
    
    */
    puts("Laboratorio SO - consola");
    puts("Comandos: ejecuta <archivo> | salir");

    /*
     se use un bucle infinito para que el programa se ejecute hasta que el usuario escriba "salir".
     fgets lee lo que el usuario teclea en stdin y se almacena en cmd.
     Si fgets falla (EOF o error) → se sale del bucle.
     rtrim limpia saltos de línea al final de cmd.
     Si el comando es "salir" → se muestra "Saliendo..." y se sale del bucle.
        Si el comando empieza con "ejecuta " → se extrae
            El archivo a ejecutar se almacena en ruta.
            Se llama a ejecutar_archivo(ruta).
            Si falla → se muestra un mensaje de error.
            Si todo es correcto → se sale del bucle.
        Si el comando no es reconocido → se muestra un mensaje de ayuda.
        puts("Comando no reconocido. Usa: ejecuta <archivo> | salir");
    */
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

