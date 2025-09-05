#include <stdio.h>
#include "interprete.h"
#include "operaciones.h"
#include <string.h>
#include <stdlib.h>

/* === NUEVO ===
   Utilidad para limpiar salto de línea al final de cada string leído.
*/
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}

/* === NUEVO ===
   Función que ejecuta un archivo .asm.
   - Antes, tu main abría directamente "a.asm".
   - Ahora movimos toda esa lógica aquí para poder invocarla desde la consola.
*/
int ejecutar_archivo(const char *ruta) {
    int Ax=0, Bx=0, Cx=0, Dx=0, Pc=1;
    char linea[128];
    char inst[4], var[3], val[16], copia[4];
    char IR[32];
    char *delimitador = (char *)" ,";
    char *token;

    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo: %s\n", ruta);
        return 1;
    }

    printf("Ax = %d     Bx = %d     Cx = %d     Dx = %d       Pc = %d\n\n",
           Ax, Bx, Cx, Dx, Pc);

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int i = 0;
        strcpy(IR, "");
        rtrim(linea);                                // === NUEVO === limpia \n
        if (linea[0] == ';' || linea[0] == '\0') {   // === NUEVO === ignora comentarios y vacías
            continue;
        }

        token = strtok(linea, delimitador);
        if (!token) continue;
        strncpy(copia, token, sizeof(copia)); copia[sizeof(copia)-1] = '\0';

        while (token != NULL) {
            switch (i) {
                case 0:
                    strncpy(inst, token, sizeof(inst)); inst[sizeof(inst)-1] = '\0';
                    strcat(IR, token);
                    break;
                case 1:
                    // === CAMBIO IMPORTANTE ===  var ahora es [3] para "Ax"/"Bx"/etc + '\0'
                    strncpy(var, token, sizeof(var)); var[sizeof(var)-1] = '\0';
                    strcat(IR, " ");
                    strcat(IR, token);
                    break;
                case 2:
                    strncpy(val, token, sizeof(val)); val[sizeof(val)-1] = '\0';
                    strcat(IR, ",");
                    strcat(IR, token);
                    break;
            }
            token = strtok(NULL, delimitador);
            i++;
        }

        int valor = (i >= 3) ? atoi(val) : 0;

        if (strcmp(inst, "MOV") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = mov(Ax, valor);
            else if (strcmp(var,"Bx")==0) Bx = mov(Bx, valor);
            else if (strcmp(var,"Cx")==0) Cx = mov(Cx, valor);
            else if (strcmp(var,"Dx")==0) Dx = mov(Dx, valor);
        }
        else if (strcmp(inst, "ADD") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = add(Ax, valor);
            else if (strcmp(var,"Bx")==0) Bx = add(Bx, valor);
            else if (strcmp(var,"Cx")==0) Cx = add(Cx, valor);
            else if (strcmp(var,"Dx")==0) Dx = add(Dx, valor);
        }
        else if (strcmp(inst, "SUB") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = sub(Ax, valor);
            else if (strcmp(var,"Bx")==0) Bx = sub(Bx, valor);
            else if (strcmp(var,"Cx")==0) Cx = sub(Cx, valor);
            else if (strcmp(var,"Dx")==0) Dx = sub(Dx, valor);
        }
        else if (strcmp(inst, "MUL") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = mul(Ax, valor);
            else if (strcmp(var,"Bx")==0) Bx = mul(Bx, valor);
            else if (strcmp(var,"Cx")==0) Cx = mul(Cx, valor);
            else if (strcmp(var,"Dx")==0) Dx = mul(Dx, valor);
        }
        else if (strcmp(inst, "DIV") == 0) {
            // === NUEVO === manejo seguro de división entre 0
            if (valor == 0) {
                printf("Error: DIV entre 0. Se omite instrucción.\n");
            } else {
                if      (strcmp(var,"Ax")==0) Ax = divi(Ax, valor);
                else if (strcmp(var,"Bx")==0) Bx = divi(Bx, valor);
                else if (strcmp(var,"Cx")==0) Cx = divi(Cx, valor);
                else if (strcmp(var,"Dx")==0) Dx = divi(Dx, valor);
            }
        }
        else if (strcmp(inst, "INC") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = inc(Ax);
            else if (strcmp(var,"Bx")==0) Bx = inc(Bx);
            else if (strcmp(var,"Cx")==0) Cx = inc(Cx);
            else if (strcmp(var,"Dx")==0) Dx = inc(Dx);
        }
        else if (strcmp(inst, "DEC") == 0) {
            if      (strcmp(var,"Ax")==0) Ax = dec(Ax);
            else if (strcmp(var,"Bx")==0) Bx = dec(Bx);
            else if (strcmp(var,"Cx")==0) Cx = dec(Cx);
            else if (strcmp(var,"Dx")==0) Dx = dec(Dx);
        }
        else {
            printf("Comando no reconocido: %s\n", inst);
        }

        Pc++;
        printf("Ax = %d     Bx = %d     Cx = %d     Dx = %d       Pc = %d       IR = %s\n",
               Ax, Bx, Cx, Dx, Pc, IR);
    }

    fclose(archivo);
    return 0;
}

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

