#ifndef interprete_h
#define interprete_h
#include<stdio.h> 
#include<stdlib.h>
#include<string.h>
#include "operaciones.h"
  
/*** Prototipo de la funcion === */
static void rtrim(char *s);
static int RegistroValido(const char *var);
static int Numero(const char *s);
int ejecutar_archivo(const char *ruta);

/* === NUEVO ===
   Utilidad para limpiar salto de línea al final de cada string leído.
*/
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}

/*=== NUEVO ===
funciones para poder hacer la validacion de los status
*/ 
static int RegistroValido(const char *var){
    return (strcmp(var,"Ax")==0 || strcmp(var,"Bx")==0 || strcmp(var,"Cx")==0 || strcmp(var,"Dx")==0);
}
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
    char status[64];
    //char proceso[64];

    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo: %s\n", ruta);
        return 1;
    }

    printf("Ax          Bx           Cx          Dx         Pc          IR                  Status                              Proceso\n");

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int i = 0;
        strcpy(IR, "");
        strcpy(status,"Correcto");
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

        
        /*
            comparaciones de las variables instruccion en el incremento y decremento que ejecuta el status de operando incorrecto
            en mi caso seria lo de la validacion del status que me arroja error:operando incorrecto
        */
        // === VALIDACIONES ===
        if ((strcmp(inst,"INC")==0 || strcmp(inst,"DEC")==0)) {
            if (i != 2) strcpy(status,"operando incorrecto");
        } else {
            if (i != 3) strcpy(status,"operando incorrecto");
        }

        if (strcmp(status,"Correcto")==0 && !RegistroValido(var)) {
            strcpy(status,"Registro invalido");
        }

        /*comparacion con la variable valor para verificar el status cuando se le asigna un numero o letra*/ 
        int valor = 0;
        if (strcmp(status,"Correcto")==0 && (i==3)) {
            if (!Numero(val)) {
                strcpy(status,"Valor incorrecto");
            } else {
                valor = atoi(val);
            }
        }

        //int valor = (i >= 3) ? atoi(val) : 0;
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
                // === NUEVO === manejo seguro de división entre 0
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
                //printf("Comando no reconocido: %s\n", inst);
                strcpy(status,"Instruccion invalida");
            }
        }
        Pc++;
        printf("  %d           %d            %d            %d            %d        %s        %s                           %s\n",Ax, Bx, Cx, Dx, Pc, IR, status,ruta);
    }

    fclose(archivo);
    return 0;
}

   
#endif    