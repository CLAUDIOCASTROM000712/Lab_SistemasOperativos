#ifndef interprete_h
#define interprete_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h> // usleep
#include "operaciones.h"
#include "pcb.h"

/* utilitaria para quitar \n\r */
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) { s[--n] = '\0'; }
}

/* Validaciones ya existentes */
static int RegistroValido(const char *var){
    return (strcmp(var,"Ax")==0 || strcmp(var,"Bx")==0 || strcmp(var,"Cx")==0 || strcmp(var,"Dx")==0);
}
static int Numero(const char *s){
    if(s == 0) return 0;
    int i=0;
    if(s[0]=='-' && s[1]!='\0') i=1;
    for(;s[i];i++) if(s[i]<'0' || s[i]>'9') return 0;
    return 1;
}

/* --- Listas ligadas --- */
/* listos: cola de PCB esperando
   ejecucion: puntero al PCB en ejecucion (NULL si ninguno)
   terminados: lista ligada de snapshots (cada nodo es un snapshot de instruccion ejecutada) */
static PCB *lista_listos = NULL;
static PCB *lista_terminados = NULL;
static PCB *pcb_ejecucion = NULL;

/* funciones auxiliares de lista */
static void push_listo(PCB *p) {
    p->siguiente = NULL;
    if (!lista_listos) {
        lista_listos = p;
    } else {
        PCB *q = lista_listos;
        while (q->siguiente) q = q->siguiente;
        q->siguiente = p;
    }
}
static PCB* pop_listo(void) {
    if (!lista_listos) return NULL;
    PCB *h = lista_listos;
    lista_listos = h->siguiente;
    h->siguiente = NULL;
    return h;
}

/* Para terminados guardamos snapshots: creamos un nodo nuevo con los campos actuales */
static void anexar_terminado_snapshot(int instr_id, PCB *src) {
    PCB *n = (PCB*)malloc(sizeof(PCB));
    if (!n) return;
    n->id = instr_id;                 // id único por instrucción (global)
    n->Pc = src->Pc;
    n->Ax = src->Ax;
    n->Bx = src->Bx;
    n->Cx = src->Cx;
    n->Dx = src->Dx;
    strncpy(n->IR, src->IR, sizeof(n->IR)-1);
    n->IR[sizeof(n->IR)-1] = '\0';
    strncpy(n->status, src->status, sizeof(n->status)-1);
    n->status[sizeof(n->status)-1] = '\0';
    strncpy(n->nombre, src->nombre, sizeof(n->nombre)-1);
    n->nombre[sizeof(n->nombre)-1] = '\0';
    n->file = NULL;
    n->siguiente = NULL;

    if (!lista_terminados) {
        lista_terminados = n;
    } else {
        PCB *q = lista_terminados;
        while (q->siguiente) q = q->siguiente;
        q->siguiente = n;
    }
}

/* imprime cabeceras y listas (posiciones fijas en pantalla) */
static void dibujar_encabezados() {
    clear();
    mvprintw(0, 0, "Ejecutando:");
    mvprintw(2, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  nombre         Status");
    mvprintw(3, 0, "==========================================================================================================");
    mvprintw(6, 0, "Listos/Preparados:");
    mvprintw(8, 0, "ID   Ax   Bx   Cx   Dx   Pc   IR               nombre    Status");
    mvprintw(10, 0, "--------------------------------------------------------------------------------");
    mvprintw(12, 0, "Terminados (historial de instrucciones ejecutadas):");
    mvprintw(14, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  proceso        Status");
    mvprintw(15, 0, "==========================================================================================================");
}

/* dibuja la lista de listos (a partir de fila_base) */
static void dibujar_listos(int fila_base) {
    int fila = fila_base;
    PCB *q = lista_listos;
    while (q) {
        mvprintw(fila, 0, "%-5d%-6d%-6d%-6d%-6d%-6d%-18s%-12s",
                 q->id, q->Ax, q->Bx, q->Cx, q->Dx, q->Pc, q->IR, q->nombre);
        fila++;
        q = q->siguiente;
    }
}

/* dibuja la lista de terminados desde fila_base (se añaden abajo) */
static void dibujar_terminados(int fila_base) {
    int fila = fila_base;
    PCB *q = lista_terminados;
    while (q) {
        mvprintw(fila, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-14s%s",
                 q->id, q->Ax, q->Bx, q->Cx, q->Dx, q->Pc, q->IR, q->nombre, q->status);
        fila++;
        q = q->siguiente;
    }
}

/* parsea la linea y ejecuta UNA instrucción en el PCB dado; devuelve 1 si fue END (termina proceso) */
static int ejecutar_instruccion_linea(PCB *p, const char *linea) {
    char buf[128];
    strncpy(buf, linea, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    rtrim(buf);
    if (buf[0] == ';' || buf[0] == '\0') return 0; // comentario o vacía -> no cambia PC salvo no contar como instruccion

    // tokenizar
    char copia[128];
    strncpy(copia, buf, sizeof(copia)-1); copia[sizeof(copia)-1] = '\0';
    char *delim = " ,";
    char *token = strtok(copia, delim);
    char inst[16] = "", var[16] = "", valstr[32] = "";
    int i = 0;
    while (token) {
        if (i==0) strncpy(inst, token, sizeof(inst)-1);
        else if (i==1) strncpy(var, token, sizeof(var)-1);
        else if (i==2) strncpy(valstr, token, sizeof(valstr)-1);
        token = strtok(NULL, delim);
        i++;
    }
    p->IR[0] = '\0';
    strcat(p->IR, inst);
    if (var[0]) { strcat(p->IR, " "); strcat(p->IR, var); }
    if (valstr[0]) { strcat(p->IR, ","); strcat(p->IR, valstr); }

    if (strcmp(inst, "END") == 0) {
        strncpy(p->status, "Correcto", sizeof(p->status)-1);
        return 1;
    }

    int expected_tokens = (strcmp(inst,"INC")==0 || strcmp(inst,"DEC")==0) ? 2 : 3;
    if (i != expected_tokens) { strncpy(p->status, "operando incorrecto", sizeof(p->status)-1); return 0; }
    if (!RegistroValido(var)) { strncpy(p->status, "Registro invalido", sizeof(p->status)-1); return 0; }

    int valor = 0;
    if (expected_tokens == 3) {
        if (!Numero(valstr)) { strncpy(p->status, "Valor incorrecto", sizeof(p->status)-1); return 0; }
        valor = atoi(valstr);
    }

    strncpy(p->status, "Correcto", sizeof(p->status)-1);

    if (strcmp(inst,"MOV")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = mov(p->Ax, valor);
        else if (strcmp(var,"Bx")==0) p->Bx = mov(p->Bx, valor);
        else if (strcmp(var,"Cx")==0) p->Cx = mov(p->Cx, valor);
        else if (strcmp(var,"Dx")==0) p->Dx = mov(p->Dx, valor);
    } else if (strcmp(inst,"ADD")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = add(p->Ax, valor);
        else if (strcmp(var,"Bx")==0) p->Bx = add(p->Bx, valor);
        else if (strcmp(var,"Cx")==0) p->Cx = add(p->Cx, valor);
        else if (strcmp(var,"Dx")==0) p->Dx = add(p->Dx, valor);
    } else if (strcmp(inst,"SUB")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = sub(p->Ax, valor);
        else if (strcmp(var,"Bx")==0) p->Bx = sub(p->Bx, valor);
        else if (strcmp(var,"Cx")==0) p->Cx = sub(p->Cx, valor);
        else if (strcmp(var,"Dx")==0) p->Dx = sub(p->Dx, valor);
    } else if (strcmp(inst,"MUL")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = mul(p->Ax, valor);
        else if (strcmp(var,"Bx")==0) p->Bx = mul(p->Bx, valor);
        else if (strcmp(var,"Cx")==0) p->Cx = mul(p->Cx, valor);
        else if (strcmp(var,"Dx")==0) p->Dx = mul(p->Dx, valor);
    } else if (strcmp(inst,"DIV")==0) {
        if (valor == 0) { strncpy(p->status, "Division por cero", sizeof(p->status)-1); return 0; }
        if (strcmp(var,"Ax")==0) p->Ax = divi(p->Ax, valor);
        else if (strcmp(var,"Bx")==0) p->Bx = divi(p->Bx, valor);
        else if (strcmp(var,"Cx")==0) p->Cx = divi(p->Cx, valor);
        else if (strcmp(var,"Dx")==0) p->Dx = divi(p->Dx, valor);
    } else if (strcmp(inst,"INC")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = inc(p->Ax);
        else if (strcmp(var,"Bx")==0) p->Bx = inc(p->Bx);
        else if (strcmp(var,"Cx")==0) p->Cx = inc(p->Cx);
        else if (strcmp(var,"Dx")==0) p->Dx = inc(p->Dx);
    } else if (strcmp(inst,"DEC")==0) {
        if (strcmp(var,"Ax")==0) p->Ax = dec(p->Ax);
        else if (strcmp(var,"Bx")==0) p->Bx = dec(p->Bx);
        else if (strcmp(var,"Cx")==0) p->Cx = dec(p->Cx);
        else if (strcmp(var,"Dx")==0) p->Dx = dec(p->Dx);
    } else {
        strncpy(p->status,"Instruccion invalida", sizeof(p->status)-1);
    }

    return 0;
}

/* Función principal: acepta 'ruta' que puede contener múltiples nombres separados por espacios */
int ejecutar_archivo(const char *ruta_mult) {
    // Limpiar listas previas si hubieran
    while (lista_listos) {
        PCB *t = lista_listos;
        lista_listos = lista_listos->siguiente;
        if (t->file) { fclose(t->file); }
        free(t);
    }
    while (lista_terminados) {
        PCB *t = lista_terminados;
        lista_terminados = lista_terminados->siguiente;
        free(t);
    }
    pcb_ejecucion = NULL;

    // Parsear nombres
    char tmp[512];
    strncpy(tmp, ruta_mult, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';
    char *tok = strtok(tmp, " ");
    int proc_id = 1;
    while (tok) {
        // intentar abrir archivo
        FILE *f = fopen(tok, "r");
        if (!f) {
            printf("No se pudo abrir: %s\n", tok);
            // liberar recursos abiertos antes de salir
            PCB *q = lista_listos;
            while (q) { PCB *n=q->siguiente; if (q->file) fclose(q->file); free(q); q=n; }
            lista_listos = NULL;
            return 1;
        }
        // crear PCB y anexar a listos
        PCB *p = (PCB*)malloc(sizeof(PCB));
        if (!p) { fclose(f); return 2; }
        crear_PCB(p, proc_id++, tok, f);
        // guardar IR inicial (primera instrucción mostrada al inicio)
        p->IR[0] = '\0';
        // leer la primera línea para mostrarla como primera IR al imprimir listos (no mover el file pointer)
        // NO avanzamos el file pointer; nos limitamos a mostrar el estado inicial.
        push_listo(p);
        tok = strtok(NULL, " ");
    }

    // inicializar ncurses
    initscr();
    noecho();
    curs_set(0);

    dibujar_encabezados();
    dibujar_listos(11);
    refresh();

    int instr_global_id = 1;

    // procesar la cola de listos: sacar uno, ejecutarlo totalmente línea por línea, anexar snapshots a terminados
    while ((pcb_ejecucion = pop_listo()) != NULL) {
        // poner status ejecucion
        strncpy(pcb_ejecucion->status, "ejecucion", sizeof(pcb_ejecucion->status)-1);

        // posicionar al inicio del archivo para empezar a leer desde la primera linea
        rewind(pcb_ejecucion->file);
        char linea[128];
        // fila de impresión para ejecucion (fila 4)
        int fila_ejec = 4;

        while (fgets(linea, sizeof(linea), pcb_ejecucion->file) != NULL) {
            rtrim(linea);
            if (linea[0] == ';' || linea[0] == '\0') continue;

            int fue_END = ejecutar_instruccion_linea(pcb_ejecucion, linea);
            // aumentar PC (después de ejecutar la instrucción)
            pcb_ejecucion->Pc++;

            // anexar snapshot a terminados (cada instrucción produce una fila en terminados)
            anexar_terminado_snapshot(instr_global_id++, pcb_ejecucion);

            // dibujar pantalla actualizada
            dibujar_encabezados();
            // mostrar ejecucion - una sola fila
            mvprintw(fila_ejec, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-12s%s",
                     pcb_ejecucion->id, pcb_ejecucion->Ax, pcb_ejecucion->Bx,
                     pcb_ejecucion->Cx, pcb_ejecucion->Dx, pcb_ejecucion->Pc,
                     pcb_ejecucion->IR, pcb_ejecucion->nombre, pcb_ejecucion->status);

            // actualizar listos y terminados
            dibujar_listos(11);
            dibujar_terminados(16);

            refresh();
            usleep(500000); // 0.5s por instrucción

            if (fue_END) {
                // marcar correcto y salir del ciclo para terminar proceso
                strncpy(pcb_ejecucion->status, "Correcto", sizeof(pcb_ejecucion->status)-1);
                break;
            }
        }

        // cerramos archivo del proceso ejecutado y liberamos su memoria (no lo ponemos en lista_listos)
        if (pcb_ejecucion->file) { fclose(pcb_ejecucion->file); pcb_ejecucion->file = NULL; }
        // Opcional: mantener en terminados el último snapshot (ya agregado)
        free(pcb_ejecucion);
        pcb_ejecucion = NULL;
    }

    // Una vez terminados todos los procesos:
    mvprintw( (LINES > 3 ? LINES-2 : 20), 0, "Ejecucion finalizada. Presiona una tecla para salir.");
    refresh();
    getch();
    endwin();

    return 0;
}

#endif

