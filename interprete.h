#ifndef interprete_h //guarda de inclusiones (ifndef, define, endif)
#define interprete_h //guarda de inclusiones
#include<stdio.h> //libreria de entrada/salida
#include<stdlib.h> //libreria de utilidades generales(malloc, atoi, etc).
#include<string.h> //libreria de strings osea cadenas
#include "operaciones.h" //incluyo la libreria de ecabezado de operaciones
#include<ncurses.h>//incluí la librería ncurses
#include<unistd.h>//para usleep


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
    int Ax=0, Bx=0, Cx=0, Dx=0, Pc=1;   //variables de registros
    char linea[128]; //buffer de lectura
    char inst[4], var[3], val[16], copia[4]; //variables auxiliares
    char IR[32]; //registro de instruccion
    char *delimitador = (char *)" ,"; //delimitador de cadenas
    char *token; //puntero para tokenizar
    char status[64]; //estatus de la instruccion
    //char proceso[64];

    //funcion que abre un archivo y lo lee
    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo: %s\n", ruta);
        return 1;
    }
    //agregamos las funciones ncurses
    initscr();//inicialicé la pantalla de ncurses
    noecho();//no muestra lo que se escribe 
    curs_set(0);//oculté el cursor, cuestión estética

    //imprime el encabezado de la tabla.
    //printf("Ax          Bx           Cx          Dx         Pc          IR                  Status                              Proceso\n");

    //Dibuja la cabecera en la fila 0
    mvprintw(0, 0, "Ax        Bx        Cx        Dx        Pc        IR                  Status");
    //Dibuja una línea separadora en la fila 1
    mvprintw(1, 0, "======================================================================================");

    int fila = 2; //Empezaremos a imprimir en la fila 2

    /*
     Leer línea por línea:

     Se usa fgets(linea, sizeof(linea), archivo).

     Se limpia la línea con rtrim(linea).

     Si la línea es comentario (;...) o está vacía → se omite.
    
    */
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int i = 0;
        strcpy(IR, "");
        strcpy(status,"Correcto");
        rtrim(linea);                                // limpia \n
        if (linea[0] == ';' || linea[0] == '\0') {   // ignora comentarios y vacías
            continue;
        }
        
        //token es un puntero para tokenizar
        /*
            Tokenización (strtok):

            Se separa en inst (instrucción), var (registro), val (valor).

            Se reconstruye el IR concatenando la instrucción y sus operandos.
        */
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
        /*
            Ejecución de la instrucción:

            Se compara inst con cadenas (MOV, ADD, SUB, MUL, DIV, INC, DEC).

            Dependiendo del registro (Ax, Bx, Cx, Dx), se ejecuta la operación con las funciones de operaciones.h.

            Si la instrucción o registro es inválido → status = "Instruccion invalida" o "Registro inválido".
        */
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
                //manejo seguro de división entre 0
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
        Pc++; //actualiza el contador de programa
        //muestra el resultado de la tabla en pantalla.
        //printf("  %d           %d            %d            %d            %d        %s        %s                           %s\n",Ax, Bx, Cx, Dx, Pc, IR, status,ruta);

        mvprintw(fila, 0, "%-10d%-10d%-10d%-10d%-10d%-23s%s", Ax, Bx, Cx, Dx, Pc, IR, status);//BHimprime los registros en la fila correspondiente
        fila++; //Prepara la siguiente línea
        refresh();//Actualiza la pantalla para mostrar los cambios
        
        usleep(500000);//Pausa de 500,000 microsegundos (medio segundo)
        

    }
    mvprintw(fila + 1, 0, "Ejecucion finalizada. Presiona una tecla para salir.");//Informa al usuario que la ejecución ha finalizado
    getch(); //Espera a que el usuario presione una tecla antes de cerrar

    endwin();//Cierra la pantalla de ncurses
    
    //cierra el archivo.
    fclose(archivo);
    return 0;
}

   
#endif    