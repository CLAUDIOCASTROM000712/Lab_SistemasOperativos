#ifndef interprete_h
#define interprete_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include "operaciones.h"
#include "pcb.h"
#include "kbhit.h"

/* --- Funciones utilitarias --- */
static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static void trim(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

static int RegistroValido(const char *var)
{
    return (strcmp(var, "Ax") == 0 || strcmp(var, "Bx") == 0 ||
            strcmp(var, "Cx") == 0 || strcmp(var, "Dx") == 0);
}

static int Numero(const char *s)
{
    if (s == NULL)
        return 0;
    int i = 0;
    if (s[0] == '-' && s[1] != '\0')
        i = 1;
    for (; s[i]; i++)
        if (s[i] < '0' || s[i] > '9')
            return 0;
    return 1;
}

/* --- Listas globales --- */
static PCB *lista_listos = NULL;
static PCB *lista_terminados = NULL;
static PCB *pcb_ejecucion = NULL;

/* --- Funciones auxiliares para listas --- */
static void push_listo(PCB *p)
{
    p->siguiente = NULL;
    if (!lista_listos)
        lista_listos = p;
    else
    {
        PCB *q = lista_listos;
        while (q->siguiente)
            q = q->siguiente;
        q->siguiente = p;
    }
}

static PCB *pop_listo(void)
{
    if (!lista_listos)
        return NULL;
    PCB *h = lista_listos;
    lista_listos = h->siguiente;
    h->siguiente = NULL;
    return h;
}

static void anexar_terminado_final(PCB *src)
{
    PCB *n = (PCB *)malloc(sizeof(PCB));
    if (!n)
        return;
    *n = *src;
    n->file = NULL;
    n->siguiente = NULL;

    if (!lista_terminados)
        lista_terminados = n;
    else
    {
        PCB *q = lista_terminados;
        while (q->siguiente)
            q = q->siguiente;
        q->siguiente = n;
    }
}

/* --- Interfaz ncurses --- */
static void dibujar_encabezados()
{
    clear();
    mvprintw(0, 0, "Ejecucion:");
    mvprintw(2, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  nombre         Status");
    mvprintw(3, 0, "==================================================================================================");

    mvprintw(6, 0, "Listos/Preparados:");
    mvprintw(8, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  nombre         Status");
    mvprintw(9, 0, "--------------------------------------------------------------------------------------------------");

    mvprintw(12, 0, "Terminados:");
    mvprintw(14, 0, "ID   Ax        Bx        Cx        Dx        Pc        IR                  nombre         Status");
    mvprintw(15, 0, "==================================================================================================");

    mvprintw(LINES-1, 0, "Presiona 'q' o ESC para detener la simulacion.");
}

static void dibujar_listos(int fila_base)
{
    int fila = fila_base;
    PCB *q = lista_listos;
    while (q)
    {
        mvprintw(fila, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-12s%s",
                 q->id, q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                 "------", q->nombre, "------");
        fila++;
        q = q->siguiente;
    }
}

static void dibujar_terminados(int fila_base)
{
    int fila = fila_base;
    PCB *q = lista_terminados;
    while (q)
    {
        mvprintw(fila, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-12s%s",
                 q->id, q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                 q->IR, q->nombre, q->status);
        fila++;
        q = q->siguiente;
    }
}

/* --- Ejecución de instrucciones --- */
static int ejecutar_instruccion_linea(PCB *p, const char *linea)
{
    char buf[128];
    strncpy(buf, linea, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    rtrim(buf);
    if (buf[0] == ';' || buf[0] == '\0')
        return 0;

    char inst[16] = "", rest[64] = "";
    char var[32] = "", valstr[32] = "";

    if (sscanf(buf, "%15s %63[^\n]", inst, rest) < 1)
    {
        strncpy(p->status, "Formato invalido", sizeof(p->status) - 1);
        return 0;
    }
    trim(inst);
    trim(rest);

    if (rest[0])
    {
        char copy[64];
        strncpy(copy, rest, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';
        char *tok = strtok(copy, ",");
        if (tok)
        {
            strncpy(var, tok, sizeof(var) - 1);
            var[sizeof(var) - 1] = '\0';
            trim(var);
        }
        tok = strtok(NULL, ",");
        if (tok)
        {
            strncpy(valstr, tok, sizeof(valstr) - 1);
            valstr[sizeof(valstr) - 1] = '\0';
            trim(valstr);
        }
        tok = strtok(NULL, ",");
        if (tok)
        {
            strncpy(p->status, "Formato invalido", sizeof(p->status) - 1);
            return 0;
        }
    }

    char tempIR[128];
    if (var[0] && valstr[0])
        snprintf(tempIR, sizeof(tempIR), "%s %s,%s", inst, var, valstr);
    else if (var[0])
        snprintf(tempIR, sizeof(tempIR), "%s %s", inst, var);
    else
        snprintf(tempIR, sizeof(tempIR), "%s", inst);
    strncpy(p->IR, tempIR, sizeof(p->IR) - 1);
    p->IR[sizeof(p->IR) - 1] = '\0';

    if (strcmp(inst, "END") == 0)
    {
        strncpy(p->status, "Correcto", sizeof(p->status) - 1);
        return 1;
    }

    if (var[0] && !RegistroValido(var))
    {
        strncpy(p->status, "Registro invalido", sizeof(p->status) - 1);
        return 0;
    }

    if (strcmp(inst, "INC") == 0 || strcmp(inst, "DEC") == 0)
    {
        if (valstr[0])
        {
            strncpy(p->status, "Formato invalido", sizeof(p->status) - 1);
            return 0;
        }
    }
    else if (strcmp(inst, "MOV") == 0 || strcmp(inst, "ADD") == 0 ||
             strcmp(inst, "SUB") == 0 || strcmp(inst, "MUL") == 0 ||
             strcmp(inst, "DIV") == 0)
    {
        if (!valstr[0])
        {
            strncpy(p->status, "Falta operando", sizeof(p->status) - 1);
            return 0;
        }
        if (!Numero(valstr))
        {
            strncpy(p->status, "Valor invalido", sizeof(p->status) - 1);
            return 0;
        }
    }
    else
    {
        strncpy(p->status, "Instruccion invalida", sizeof(p->status) - 1);
        return 0;
    }

    int valor = 0;
    if (valstr[0])
        valor = atoi(valstr);

    strncpy(p->status, "Correcto", sizeof(p->status) - 1);

    if (strcmp(inst, "MOV") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = mov(p->Ax, valor);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = mov(p->Bx, valor);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = mov(p->Cx, valor);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = mov(p->Dx, valor);
    }
    else if (strcmp(inst, "ADD") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = add(p->Ax, valor);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = add(p->Bx, valor);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = add(p->Cx, valor);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = add(p->Dx, valor);
    }
    else if (strcmp(inst, "SUB") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = sub(p->Ax, valor);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = sub(p->Bx, valor);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = sub(p->Cx, valor);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = sub(p->Dx, valor);
    }
    else if (strcmp(inst, "MUL") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = mul(p->Ax, valor);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = mul(p->Bx, valor);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = mul(p->Cx, valor);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = mul(p->Dx, valor);
    }
    else if (strcmp(inst, "DIV") == 0)
    {
        if (valor == 0)
        {
            strncpy(p->status, "Div por cero", sizeof(p->status) - 1);
            return 0;
        }
        if (strcmp(var, "Ax") == 0)
            p->Ax = divi(p->Ax, valor);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = divi(p->Bx, valor);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = divi(p->Cx, valor);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = divi(p->Dx, valor);
    }
    else if (strcmp(inst, "INC") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = inc(p->Ax);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = inc(p->Bx);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = inc(p->Cx);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = inc(p->Dx);
    }
    else if (strcmp(inst, "DEC") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            p->Ax = dec(p->Ax);
        else if (strcmp(var, "Bx") == 0)
            p->Bx = dec(p->Bx);
        else if (strcmp(var, "Cx") == 0)
            p->Cx = dec(p->Cx);
        else if (strcmp(var, "Dx") == 0)
            p->Dx = dec(p->Dx);
    }

    return 0;
}

/* --- Ejecución completa --- */
int ejecutar_archivo(const char *ruta_mult)
{
    while (lista_listos)
    {
        PCB *t = lista_listos;
        lista_listos = t->siguiente;
        if (t->file)
            fclose(t->file);
        free(t);
    }
    while (lista_terminados)
    {
        PCB *t = lista_terminados;
        lista_terminados = t->siguiente;
        free(t);
    }

    char tmp[512];
    strncpy(tmp, ruta_mult, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *tok = strtok(tmp, " ");
    int proc_id = 1;

    while (tok)
    {
        FILE *f = fopen(tok, "r");
        if (!f)
        {
            printf("No se pudo abrir: %s\n", tok);
            return 1;
        }
        PCB *p = (PCB *)malloc(sizeof(PCB));
        crear_PCB(p, proc_id++, tok, f);
        push_listo(p);
        tok = strtok(NULL, " ");
    }

    initscr();
    noecho();
    curs_set(0);

    dibujar_encabezados();
    dibujar_listos(10);
    refresh();

    while ((pcb_ejecucion = pop_listo()) != NULL)
    {
        rewind(pcb_ejecucion->file);
        strncpy(pcb_ejecucion->status, "ejecucion", sizeof(pcb_ejecucion->status) - 1);

        char linea[128];
        while (fgets(linea, sizeof(linea), pcb_ejecucion->file))
        {
            rtrim(linea);
            if (linea[0] == ';' || linea[0] == '\0')
                continue;
            int fin = ejecutar_instruccion_linea(pcb_ejecucion, linea);
            pcb_ejecucion->Pc++;

            // agrego lo de kbhit
            //  --- 🔹 Verificar interrupción por teclado ---
            if (kbhit())
            {
                int ch = getchar(); // leer tecla presionada
                if (ch == 'q' || ch == 'Q' || ch == 27)
                { // 'q' o 'ESC'
                    mvprintw(LINES - 3, 0, "Interrupcion detectada por teclado. Cancelando ejecucion...");
                    refresh();
                    usleep(800000);

                    // Cerrar ncurses
                    endwin();

                    // Liberar memoria pendiente
                    while (lista_listos)
                    {
                        PCB *t = lista_listos;
                        lista_listos = t->siguiente;
                        if (t->file)
                            fclose(t->file);
                        free(t);
                    }
                    while (lista_terminados)
                    {
                        PCB *t = lista_terminados;
                        lista_terminados = t->siguiente;
                        if (t->file)
                            fclose(t->file);
                        free(t);
                    }
                    if (pcb_ejecucion)
                    {
                        if (pcb_ejecucion->file)
                            fclose(pcb_ejecucion->file);
                        free(pcb_ejecucion);
                        pcb_ejecucion = NULL;
                    }

                    printf("\n\n[Ejecucion interrumpida por el usuario]\n");
                    return 0;
                }else if (ch == 'p' || ch == 'P') { // Pausar
                    mvprintw(LINES-3, 0, "⏸  Ejecucion pausada. Presiona 'r' para reanudar o 'q' para salir.");
                    refresh();
                    int ch2;
                    do {
                        usleep(100000);
                        if (kbhit()) {
                            ch2 = getchar();
                            if (ch2 == 'r' || ch2 == 'R') {
                                mvprintw(LINES-3, 0, "▶  Ejecucion reanudada.                                          ");
                                refresh();
                                break;
                            }
                            if (ch2 == 'q' || ch2 == 'Q' || ch2 == 27) {
                                endwin();
                                printf("\n\n[Ejecucion interrumpida durante pausa]\n");
                            return 0;
                            }
                        }
                    } while (1);
                }
            }

            dibujar_encabezados();
            mvprintw(4, 0, "%-5d%-10d%-10d%-10d%-10d%-10d%-20s%-12s%s",
                     pcb_ejecucion->id, pcb_ejecucion->Ax, pcb_ejecucion->Bx,
                     pcb_ejecucion->Cx, pcb_ejecucion->Dx, pcb_ejecucion->Pc,
                     pcb_ejecucion->IR, pcb_ejecucion->nombre, pcb_ejecucion->status);

            dibujar_listos(10);
            dibujar_terminados(16);
            refresh();
            usleep(700000);

            if (fin)
                break;
        }

        anexar_terminado_final(pcb_ejecucion);
        dibujar_encabezados();
        dibujar_listos(10);
        dibujar_terminados(16);
        refresh();
        usleep(500000);

        if (pcb_ejecucion->file)
            fclose(pcb_ejecucion->file);
        free(pcb_ejecucion);
        pcb_ejecucion = NULL;
    }

    mvprintw(LINES - 2, 0, "Ejecucion finalizada. Presiona una tecla para salir...");
    refresh();
    getch();
    endwin();
    return 0;
}

#endif
