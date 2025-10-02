#ifndef interprete_h //guarda de inclusiones (ifndef, define, endif)
#define interprete_h //guarda de inclusiones
#include<stdio.h> //libreria de entrada/salida
#include<stdlib.h> //libreria de utilidades generales(malloc, atoi, etc).
#include<string.h> //libreria de strings osea cadenas
#include "operaciones.h" //incluyo la libreria de ecabezado de operaciones
#include<ncurses.h>//incluí la librería ncurses
#include<unistd.h>//para usleep
#include "pcb.h"

// Asegúrate de que la definición de PCB esté disponible.
// Si pcb.h no define PCB, agrega aquí la definición:
// typedef struct PCB {
//     // campos de PCB
// } PCB;


/*** Prototipo de la funcion === */
static void rtrim(char *s); //elimina saltos de linea al final de cada string leido
static int RegistroValido(const char *var);//verifica si el registro es valido
static int Numero(const char *s);//verifica si el string es un numero
int ejecutar_archivo(const char *ruta);//funcion que ejecuta un archivo asm




/* 
   Utilidad para limpiar salto de línea al final de cada string leído.
   Recibe una cadena.

    Busca al final caracteres de salto de línea (\n o \r) y los reemplaza por \0.

    Se usa para limpiar líneas leídas de un archivo.
*/
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}

/*
funciones para poder hacer la validacion de los status
 Devuelve 1 si var es "Ax", "Bx", "Cx" o "Dx".

 Devuelve 0 en caso contrario.

 Sirve para validar que los operandos sean registros válidos.
*/ 
static int RegistroValido(const char *var){
    return (strcmp(var,"Ax")==0 || strcmp(var,"Bx")==0 || strcmp(var,"Cx")==0 || strcmp(var,"Dx")==0);
}

/*
la funcion numero hace lo siguiente:
 Comprueba si la cadena representa un número entero.

 Acepta opcionalmente un - al inicio para números negativos.

 Si encuentra un carácter que no es dígito → devuelve 0.

 Si todo es correcto → devuelve 1.

*/
static int Numero(const char *s){
    if(s == 0){
        return 0; //vacio no es numero
    }
    int i=0;
    if(s[0]=='-' && s[1]!='\0'){
        i=1; //permite signo negativo
    }
    for(;s[i];i++){
        if(s[i]<'0' || s[i]>'9') return 0;
    }
    return 1; // es un número válido
}

/* 
   Función que ejecuta un archivo .asm.
   - Antes, tu main abría directamente "a.asm".
   - Ahora movimos toda esa lógica aquí para poder invocarla desde la consola.
*/
//Función principal que procesa un archivo .asm y simula registros (Ax, Bx, Cx, Dx) y un contador de programa (Pc).
int ejecutar_archivo(const char *ruta) {
    int Ax=0, Bx=0, Cx=0, Dx=0, Pc=1;   // registros
    char linea[128]; 
    char inst[8], var[8], val[16], copia[8]; 
    char IR[32]; 
    char *delimitador = (char *)" ,"; 
    char *token; 
    char status[64]; 

    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo: %s\n", ruta);
        return 1;
    }

    // inicializa ncurses
    initscr();
    noecho();
    curs_set(0);

    // encabezado
    mvprintw(0, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  proceso        Status");
    mvprintw(1, 0, "==================================================================================================");

    int fila = 2; 
    int id = 1; // contador autoincremental

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int i = 0;
        strcpy(IR, "");
        strcpy(status,"Correcto");
        rtrim(linea);                                
        if (linea[0] == ';' || linea[0] == '\0') {   
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

        // === Validar END ===
        if (strcmp(inst, "END") == 0) {
            strcpy(status, "Correcto");
            mvprintw(fila, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-10s      %s",
                     id, Ax, Bx, Cx, Dx, Pc, IR, ruta,status);
            fila++;
            refresh();
            break; // termina ejecución aquí
        }

        // Validación de operandos
        if ((strcmp(inst,"INC")==0 || strcmp(inst,"DEC")==0)) {
            if (i != 2) strcpy(status,"operando incorrecto");
        } else {
            if (i != 3) strcpy(status,"operando incorrecto");
        }

        if (strcmp(status,"Correcto")==0 && !RegistroValido(var)) {
            strcpy(status,"Registro invalido");
        }

        int valor = 0;
        if (strcmp(status,"Correcto")==0 && (i==3)) {
            if (!Numero(val)) {
                strcpy(status,"Valor incorrecto");
            } else {
                valor = atoi(val);
            }
        }

        // === Ejecución de instrucciones ===
        if(strcmp(status,"Correcto")==0) {
            if (strcmp(inst, "MOV") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = mov(Ax, valor);
                else if (strcmp(var,"Bx")==0) Bx = mov(Bx, valor);
                else if (strcmp(var,"Cx")==0) Cx = mov(Cx, valor);
                else if (strcmp(var,"Dx")==0) Dx = mov(Dx, valor);
                else strcpy(status,"Registro inválido");
            }
            else if (strcmp(inst, "ADD") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = add(Ax, valor);
                else if (strcmp(var,"Bx")==0) Bx = add(Bx, valor);
                else if (strcmp(var,"Cx")==0) Cx = add(Cx, valor);
                else if (strcmp(var,"Dx")==0) Dx = add(Dx, valor);
                else strcpy(status,"Registro inválido");
            }
            else if (strcmp(inst, "SUB") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = sub(Ax, valor);
                else if (strcmp(var,"Bx")==0) Bx = sub(Bx, valor);
                else if (strcmp(var,"Cx")==0) Cx = sub(Cx, valor);
                else if (strcmp(var,"Dx")==0) Dx = sub(Dx, valor);
                else strcpy(status,"Registro inválido");
            }
            else if (strcmp(inst, "MUL") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = mul(Ax, valor);
                else if (strcmp(var,"Bx")==0) Bx = mul(Bx, valor);
                else if (strcmp(var,"Cx")==0) Cx = mul(Cx, valor);
                else if (strcmp(var,"Dx")==0) Dx = mul(Dx, valor);
                else strcpy(status,"Registro inválido");
            }
            else if (strcmp(inst, "DIV") == 0) {
                if (valor == 0) {
                    strcpy(status,"División por cero");
                } else {
                    if      (strcmp(var,"Ax")==0) Ax = divi(Ax, valor);
                    else if (strcmp(var,"Bx")==0) Bx = divi(Bx, valor);
                    else if (strcmp(var,"Cx")==0) Cx = divi(Cx, valor);
                    else if (strcmp(var,"Dx")==0) Dx = divi(Dx, valor);
                    else strcpy(status,"Registro inválido");
                }
            }
            else if (strcmp(inst, "INC") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = inc(Ax);
                else if (strcmp(var,"Bx")==0) Bx = inc(Bx);
                else if (strcmp(var,"Cx")==0) Cx = inc(Cx);
                else if (strcmp(var,"Dx")==0) Dx = inc(Dx);
                else strcpy(status,"Registro inválido");
            }
            else if (strcmp(inst, "DEC") == 0) {
                if      (strcmp(var,"Ax")==0) Ax = dec(Ax);
                else if (strcmp(var,"Bx")==0) Bx = dec(Bx);
                else if (strcmp(var,"Cx")==0) Cx = dec(Cx);
                else if (strcmp(var,"Dx")==0) Dx = dec(Dx);
                else strcpy(status,"Registro inválido");
            }
            else {
                strcpy(status,"Instruccion invalida");
            }
        }

        Pc++; 
        mvprintw(fila, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-10s     %s ",
                 id, Ax, Bx, Cx, Dx, Pc, IR, ruta,status);
        fila++; 
        id++; 
        refresh();
        usleep(500000);
    }

    mvprintw(fila + 1, 0, "Ejecucion finalizada. Presiona una tecla para salir.");
    getch();
    endwin();
    fclose(archivo);
    return 0;
}

   
#endif    

