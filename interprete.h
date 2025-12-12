#ifndef INTERPRETE_H
#define INTERPRETE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // <-- necesario para srand/time
#include <ncurses.h>
#include <unistd.h>
#include <panel.h> // nueva libreria para paneles de ncurses
#include "operaciones.h"
#include "pcb.h"
#include "kbhit.h"

/* ------------------ Ventanas y paneles (variables globales) ------------------ */
static WINDOW *win_ejecucion = NULL;
static WINDOW *win_listos = NULL;
static WINDOW *win_terminados = NULL;
static WINDOW *win_nuevos = NULL;
static WINDOW *win_bloqueados = NULL;
static WINDOW *win_recursos = NULL; /* ventanita de recursos */

static PANEL *panel_ejecucion = NULL;
static PANEL *panel_listos = NULL;
static PANEL *panel_terminados = NULL;
static PANEL *panel_nuevos = NULL;
static PANEL *panel_bloqueados = NULL;
static PANEL *panel_recursos = NULL;

/* Prototipos para dibujado */
static void dibujar_nuevos(WINDOW *win_nuevos, int fila_base);
static void dibujar_listos(WINDOW *win_listos, int fila_base);
static void dibujar_terminados(WINDOW *win_terminados, int fila_base);
static void dibujar_bloqueados(WINDOW *win_bloqueados, int fila_base);
static void dibujar_recursos(WINDOW *win_recursos, int fila_base);
/* Forzar orden visual de paneles: TERMINADOS sobre BLOQUEADOS, RECURSOS siempre arriba */
static void asegurar_orden_paneles(void);
/* Prototipos para guardar/cargar contexto (declaraciones adelantadas) */
static void GuardarContexto(PCB *p);
static void CargarContexto(PCB *p);

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

/* ------------------ Recursos globales del SO ------------------ */
/* Valores totales del sistema (constantes) */
static const int RTotalX = 10;
static const int RTotalY = 10;
static const int RTotalZ = 10;
/* Recursos disponibles (se modifican en tiempo de ejecución) */
static int Rx = 10;
static int Ry = 10;
static int Rz = 10;

/* ------------------ Listas globales ------------------ */
static PCB *lista_listos = NULL;
static PCB *lista_terminados = NULL;
static PCB *lista_nuevos = NULL;     /* nueva lista donde se ponen los procesos al leerlos */
static PCB *lista_bloqueados = NULL; /* procesos que esperan recursos */
static PCB *pcb_ejecucion = NULL;

/* --- Registros globales (para simulacion de cargar/guardar contexto) --- */
static int gAx = 0, gBx = 0, gCx = 0, gDx = 0;
/* Quantum por defecto (puedes cambiarlo) */
static int Q = 4;

/* ------------------ Helpers para manejo de listas ------------------ */

/* libera todos los nodos de una lista simple y cierra archivos si están abiertos */
static void liberar_lista(PCB **head)
{
    PCB *t;
    while (*head)
    {
        t = *head;
        *head = t->siguiente;
        if (t->file)
            fclose(t->file);
        if (t->TMP)
            free(t->TMP);
        free(t);
    }
}

/* Inserta un PCB en lista_listos respetando prioridad.
   Prioridad 1 -> al frente; prioridad 4 -> al final.
   Si misma prioridad -> FIFO (se encola al final del grupo con la misma prioridad). */
static void push_listo(PCB *p)
{
    if (!p)
        return;
    p->siguiente = NULL;

    if (!lista_listos)
    {
        lista_listos = p;
        return;
    }

    /* Si p tiene mayor prioridad (número menor) que el primero -> se pone al frente */
    if (p->prioridad < lista_listos->prioridad)
    {
        p->siguiente = lista_listos;
        lista_listos = p;
        return;
    }

    /* Recorrer hasta encontrar la posición correcta: avanzamos mientras el siguiente
       exista y su prioridad sea menor o igual a la prioridad de p. Así p queda
       después de todos los de su misma prioridad (FIFO en la misma prioridad). */
    PCB *q = lista_listos;
    while (q->siguiente && q->siguiente->prioridad <= p->prioridad)
        q = q->siguiente;

    p->siguiente = q->siguiente;
    q->siguiente = p;
}

/* Extrae el primer PCB de lista_listos (pop FIFO en la lista ordenada por prioridad) */
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
    if (!src)
        return;
    /* Guardar los recursos asignados en temporales para devolverlos después
     * de copiar el PCB a la lista de terminados. De esta forma la entrada en
     * TERMINADOS conserva lo que el proceso tenía asignado y el sistema
     * recupera esos recursos. */
    int devolverX = src->RasigX;
    int devolverY = src->RasigY;
    int devolverZ = src->RasigZ;

    PCB *n = (PCB *)malloc(sizeof(PCB));
    if (!n)
        return;
    *n = *src;
    n->file = NULL; // no guardamos el FILE en la lista de terminados
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

/* --- Nueva lista `nuevos` --- */
static void push_nuevo(PCB *p)
{
    if (!p)
        return;
    p->siguiente = NULL;
    if (!lista_nuevos)
        lista_nuevos = p;
    else
    {
        PCB *q = lista_nuevos;
        while (q->siguiente)
            q = q->siguiente;
        q->siguiente = p;
    }
}

/* --- Nueva lista `bloqueados` --- */
static void push_bloqueado(PCB *p)
{
    if (!p)
        return;
    p->siguiente = NULL;
    if (!lista_bloqueados)
        lista_bloqueados = p;
    else
    {
        PCB *q = lista_bloqueados;
        while (q->siguiente)
            q = q->siguiente;
        q->siguiente = p;
    }
}

/* Cuenta procesos en la lista de listos (sin incluir el que está en ejecucion) */
static int contar_listos(void)
{
    int c = 0;
    PCB *q = lista_listos;
    while (q)
    {
        c++;
        q = q->siguiente;
    }
    return c;
}

/* ------------------ Planificador largo plazo ------------------ */
/* Mueve procesos desde `nuevos` hacia `listos` hasta que `listos` alcance el límite de 3 procesos. */
static void planificador_largo_plazo(void)
{
    // quitamos el limite de 3 procesos
    while (lista_nuevos)
    {
        PCB *p = lista_nuevos;
        lista_nuevos = p->siguiente; // avanzar en la lista NUEVOS
        p->siguiente = NULL;

        /* Verificar que el archivo del proceso comience con una instrucción MAX */
        int tiene_max = 0;
        if (p->file)
        {
            char primera[256];
            long pos = ftell(p->file);
            rewind(p->file);
            while (fgets(primera, sizeof(primera), p->file))
            {
                rtrim(primera);
                if (primera[0] == '\0' || primera[0] == ';')
                    continue; /* saltar comentarios/lineas vacias */
                /* obtener el primer token */
                char token[32] = "";
                if (sscanf(primera, "%31s", token) >= 1)
                {
                    trim(token);
                    if (strcmp(token, "MAX") == 0)
                        tiene_max = 1;
                }
                break;
            }
            /* volver al inicio del archivo para la ejecución normal */
            rewind(p->file);
            (void)pos;
        }

        if (!tiene_max)
        {
            /* No comienza con MAX -> terminar con ErrorRecursos */
            strncpy(p->status, "ErrorRecursos", sizeof(p->status) - 1);
            p->status[sizeof(p->status) - 1] = '\0';
            GuardarContexto(p);
            anexar_terminado_final(p);
            if (p->file)
                fclose(p->file);
            if (p->TMP)
                free(p->TMP);

            free(p);
            continue; /* procesar siguiente nuevo */
        }

        /* Si pasó la verificación, actualizar estado y encolar en listos */
        strncpy(p->status, "Listo", sizeof(p->status) - 1);
        p->status[sizeof(p->status) - 1] = '\0';
        push_listo(p);
    }

    /* Redibujar paneles si están activos (modo ncurses) */
    if (win_nuevos && win_listos)
    {
        dibujar_nuevos(win_nuevos, 2);
        dibujar_listos(win_listos, 2);
        // dibujar_bloqueados(win_bloqueados, 2);
        // dibujar_terminados(win_terminados, 2);
        update_panels();
        doupdate();
    }
}

/* ------------------ Planificador medio plazo ------------------ */
/* Revisa la lista de bloqueados y, si hay recursos, los mueve a listos */
/* Reemplaza función planificador_medio_plazo por esta */
static void planificador_medio_plazo(void)
{
    PCB *prev = NULL;
    PCB *curr = lista_bloqueados;
    int hubo_cambios = 0;

    while (curr)
    {
        /* Verificamos si hay suficientes recursos globales (Rx, Ry, Rz) para cubrir lo que pide (Rpend) */
        if (curr->RpendX <= Rx && curr->RpendY <= Ry && curr->RpendZ <= Rz)
        {
            // 1. Descontamos de los globales (simulamos que el GET por fin tuvo éxito)
            Rx -= curr->RpendX;
            Ry -= curr->RpendY;
            Rz -= curr->RpendZ;

            // 2. Asignamos al proceso
            curr->RasigX += curr->RpendX;
            curr->RasigY += curr->RpendY;
            curr->RasigZ += curr->RpendZ;

            // 3. Limpiamos sus pendientes
            curr->RpendX = 0;
            curr->RpendY = 0;
            curr->RpendZ = 0;

            strncpy(curr->status, "Listo", sizeof(curr->status) - 1);

            // 4. Sacamos el nodo de la lista de BLOQUEADOS
            if (prev)
                prev->siguiente = curr->siguiente;
            else
                lista_bloqueados = curr->siguiente;

            PCB *move = curr;
            // Avanzamos 'curr' para no perder el hilo del while
            curr = curr->siguiente;

            // 5. Metemos el nodo en LISTOS
            move->siguiente = NULL;
            push_listo(move);

            hubo_cambios = 1;
            break;
        }

        prev = curr;
        curr = curr->siguiente;
    }

    /* FORZAR LA ACTUALIZACIÓN VISUAL SI ALGUIEN SE DESBLOQUEÓ */
    if (hubo_cambios)
    {
        if (win_listos)
            dibujar_listos(win_listos, 2);
        if (win_bloqueados)
            dibujar_bloqueados(win_bloqueados, 2);
        if (win_recursos)
            dibujar_recursos(win_recursos, 1);
        update_panels();
        doupdate();
    }
}

/* ------------------ Guardar/Cargar contexto ------------------ */
// Guarda los registros globales en el PCB (cuando pasa Ejecucion -> Listos/Terminados)
static void GuardarContexto(PCB *p)
{
    if (!p)
        return;
    p->Ax = gAx;
    p->Bx = gBx;
    p->Cx = gCx;
    p->Dx = gDx;
}

// Carga los registros del PCB a los registros globales (cuando pasa Listos -> Ejecucion)
static void CargarContexto(PCB *p)
{
    if (!p)
        return;
    gAx = p->Ax;
    gBx = p->Bx;
    gCx = p->Cx;
    gDx = p->Dx;
}

/* ------------------ Funcion Kill (añadida búsqueda en bloqueados) ------------------ */
/* Reemplaza tu función kill_proceso por esta */
static int kill_proceso(int id)
{
    PCB *prev = NULL;
    PCB *curr = lista_listos;

    // 1. Buscar en LISTOS
    while (curr)
    {
        if (curr->id == id)
        {
            if (prev)
                prev->siguiente = curr->siguiente;
            else
                lista_listos = curr->siguiente;

            strncpy(curr->status, "Killed", sizeof(curr->status) - 1);

            // OJO: Anexamos a terminados pero NO devolvemos recursos al sistema
            anexar_terminado_final(curr);

            // Actualizar interfaz
            planificador_largo_plazo();
            if (win_listos)
                dibujar_listos(win_listos, 2);
            if (win_terminados)
                dibujar_terminados(win_terminados, 2);
            update_panels();
            doupdate();

            if (curr->file)
                fclose(curr->file);
            if (curr->TMP)
                free(curr->TMP);
            free(curr);
            printf("Proceso %d matado (recursos NO devueltos).\n", id);
            return 1;
        }
        prev = curr;
        curr = curr->siguiente;
    }

    // 2. Buscar en EJECUCION
    if (pcb_ejecucion && pcb_ejecucion->id == id)
    {
        strncpy(pcb_ejecucion->status, "Killed", sizeof(pcb_ejecucion->status) - 1);
        anexar_terminado_final(pcb_ejecucion);

        planificador_largo_plazo(); // Ver si entra uno nuevo

        if (win_listos)
            dibujar_listos(win_listos, 2);
        if (win_terminados)
            dibujar_terminados(win_terminados, 2);
        update_panels();
        doupdate();

        if (pcb_ejecucion->file)
            fclose(pcb_ejecucion->file);
        if (pcb_ejecucion->TMP)
            free(pcb_ejecucion->TMP);
        free(pcb_ejecucion);
        pcb_ejecucion = NULL;
        printf("Proceso en ejecucion %d matado (recursos NO devueltos).\n", id);
        return 1;
    }

    // 3. Buscar en BLOQUEADOS
    prev = NULL;
    curr = lista_bloqueados;
    while (curr)
    {
        if (curr->id == id)
        {
            if (prev)
                prev->siguiente = curr->siguiente;
            else
                lista_bloqueados = curr->siguiente;

            strncpy(curr->status, "Killed", sizeof(curr->status) - 1);
            anexar_terminado_final(curr);

            if (win_bloqueados)
                dibujar_bloqueados(win_bloqueados, 2);
            if (win_terminados)
                dibujar_terminados(win_terminados, 2);
            update_panels();
            doupdate();

            if (curr->file)
                fclose(curr->file);
            if (curr->TMP)
                free(curr->TMP);
            free(curr);
            printf("Proceso bloqueado %d matado (recursos NO devueltos).\n", id);
            return 1;
        }
        prev = curr;
        curr = curr->siguiente;
    }

    printf("Proceso con id: %d no existe\n", id);
    return 0;
}

/* ------------------ Interfaz ncurses con paneles ------------------ */
static void inicializar_paneles()
{
    int ancho = COLS - 2;

    // 6 ventanas → dividimos pantalla en 6 secciones iguales
    int alto = (LINES - 8) / 6;

    int y = 1; // posición vertical inicial

    // === EJECUCIÓN ===
    win_ejecucion = newwin(alto, ancho, y, 1);
    panel_ejecucion = new_panel(win_ejecucion);
    box(win_ejecucion, 0, 0);
    mvwprintw(win_ejecucion, 0, 2, " Ejecucion ");
    y += alto;

    // === NUEVOS ===
    win_nuevos = newwin(alto, ancho, y, 1);
    panel_nuevos = new_panel(win_nuevos);
    box(win_nuevos, 0, 0);
    mvwprintw(win_nuevos, 0, 2, " Nuevos ");
    y += alto;

    // === LISTOS / PREPARADOS ===
    win_listos = newwin(alto, ancho, y, 1);
    panel_listos = new_panel(win_listos);
    box(win_listos, 0, 0);
    mvwprintw(win_listos, 0, 2, " Listos/Preparados ");
    y += alto;

    // === BLOQUEADOS ===
    // === BLOQUEADOS (VENTANA NUEVA COMPLETA) ===
    int ancho_bloq = COLS - 20;
    win_bloqueados = newwin(alto, ancho_bloq, y, 1);
    panel_bloqueados = new_panel(win_bloqueados);
    box(win_bloqueados, 0, 0);
    mvwprintw(win_bloqueados, 0, 2, " Bloqueados ");
    y += alto;

    // === TERMINADOS ===
    win_terminados = newwin(alto, ancho, y, 1);
    panel_terminados = new_panel(win_terminados);
    box(win_terminados, 0, 0);
    mvwprintw(win_terminados, 0, 2, " Terminados ");
    y += alto;

    update_panels();
    doupdate();
}

/* Limpieza de paneles */
static void destruir_paneles()
{
    if (panel_ejecucion)
        del_panel(panel_ejecucion);
    if (panel_listos)
        del_panel(panel_listos);
    if (panel_terminados)
        del_panel(panel_terminados);

    if (win_ejecucion)
        delwin(win_ejecucion);
    if (win_listos)
        delwin(win_listos);
    if (win_terminados)
        delwin(win_terminados);

    panel_ejecucion = panel_listos = panel_terminados = NULL;
    win_ejecucion = win_listos = win_terminados = NULL;
}

static void dibujar_bloqueados(WINDOW *win_bloqueados, int fila_base)
{
    if (!win_bloqueados)
        return;
    werase(win_bloqueados);
    box(win_bloqueados, 0, 0);
    wattron(win_bloqueados, COLOR_PAIR(1));
    mvwprintw(win_bloqueados, 0, 2, " BLOQUEADOS ");
    wattroff(win_bloqueados, COLOR_PAIR(1));

    mvwprintw(win_bloqueados, 1, 1,
              "ID  Pr   Ax      Bx      Cx      Dx      Pc      Rmax       Rasig      Rpend      IR              nombre         Status");

    int fila = 2;
    PCB *q = lista_bloqueados;

    while (q)
    {
        char bufRmax[32], bufRasig[32], bufRpend[32];

        snprintf(bufRmax, sizeof(bufRmax), "%d,%d,%d", q->RmaxX, q->RmaxY, q->RmaxZ);
        snprintf(bufRasig, sizeof(bufRasig), "%d,%d,%d", q->RasigX, q->RasigY, q->RasigZ);
        snprintf(bufRpend, sizeof(bufRpend), "%d,%d,%d", q->RpendX, q->RpendY, q->RpendZ);

        mvwprintw(win_bloqueados, fila, 1,
                  "%-3d %-3d %-7d %-7d %-7d %-7d %-7d %-10s %-10s %-10s %-18s %-14s %s",
                  q->id, q->prioridad,
                  q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                  bufRmax, bufRasig, bufRpend,
                  q->IR, q->nombre, q->status);

        fila++;
        q = q->siguiente;
    }

    wrefresh(win_bloqueados);
}

/* ------------------ Dibujado ------------------ */
static void dibujar_nuevos(WINDOW *win_nuevos, int fila_base)
{
    if (!win_nuevos)
        return;
    werase(win_nuevos);
    box(win_nuevos, 0, 0);

    wattron(win_nuevos, COLOR_PAIR(5));
    mvwprintw(win_nuevos, 0, 2, " NUEVOS ");
    wattroff(win_nuevos, COLOR_PAIR(5));

    mvwprintw(win_nuevos, 1, 1,
              "ID  Pr   Ax      Bx      Cx      Dx      Pc      Rmax       Rasig      Rpend      IR              nombre         Status");

    int fila = 2;
    PCB *q = lista_nuevos;

    while (q)
    {
        char bufRmax[32], bufRasig[32], bufRpend[32];

        snprintf(bufRmax, sizeof(bufRmax), "%d,%d,%d", q->RmaxX, q->RmaxY, q->RmaxZ);
        snprintf(bufRasig, sizeof(bufRasig), "%d,%d,%d", q->RasigX, q->RasigY, q->RasigZ);
        snprintf(bufRpend, sizeof(bufRpend), "%d,%d,%d", q->RpendX, q->RpendY, q->RpendZ);

        mvwprintw(win_nuevos, fila, 1,
                  "%-3d %-3d %-7d %-7d %-7d %-7d %-7d %-10s %-10s %-10s %-18s %-14s %s",
                  q->id, q->prioridad,
                  q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                  bufRmax, bufRasig, bufRpend,
                  "------", q->nombre, q->status);

        fila++;
        q = q->siguiente;
    }

    wrefresh(win_nuevos);
}

static void dibujar_recursos(WINDOW *win_recursos, int fila_base)
{
    if (!win_recursos)
        return;
    werase(win_recursos);
    box(win_recursos, 0, 0);
    wattron(win_recursos, COLOR_PAIR(4));
    mvwprintw(win_recursos, 0, 2, " RECURSOS ");
    wattroff(win_recursos, COLOR_PAIR(4));
    mvwprintw(win_recursos, 1, 2, "Totals: X=%2d  Y=%2d  Z=%2d", RTotalX, RTotalY, RTotalZ);
    mvwprintw(win_recursos, 2, 2, "Avail : X=%2d  Y=%2d  Z=%2d", Rx, Ry, Rz);

    if (panel_recursos)
        top_panel(panel_recursos);
    update_panels();
    doupdate();
}

static void asegurar_orden_paneles(void)
{
    /* Queremos que TERMINADOS esté por encima de BLOQUEADOS (que no salte al frente),
       y que RECURSOS quede siempre encima de todo. */
    if (panel_terminados)
        top_panel(panel_terminados);
    if (panel_recursos)
        top_panel(panel_recursos);
    update_panels();
    doupdate();
}

static void dibujar_listos(WINDOW *win_listos, int fila_base)
{
    if (!win_listos)
        return;
    werase(win_listos);
    box(win_listos, 0, 0);

    wattron(win_listos, COLOR_PAIR(2));
    mvwprintw(win_listos, 0, 2, " LISTOS ");
    wattroff(win_listos, COLOR_PAIR(2));

    mvwprintw(win_listos, 1, 1,
              "ID  Pr   Ax      Bx      Cx      Dx      Pc      Rmax       Rasig      Rpend      IR              nombre         Status");

    int fila = 2;
    PCB *q = lista_listos;

    while (q)
    {
        char bufRmax[32], bufRasig[32], bufRpend[32];

        snprintf(bufRmax, sizeof(bufRmax), "%d,%d,%d", q->RmaxX, q->RmaxY, q->RmaxZ);
        snprintf(bufRasig, sizeof(bufRasig), "%d,%d,%d", q->RasigX, q->RasigY, q->RasigZ);
        snprintf(bufRpend, sizeof(bufRpend), "%d,%d,%d", q->RpendX, q->RpendY, q->RpendZ);

        mvwprintw(win_listos, fila, 1,
                  "%-3d %-3d %-7d %-7d %-7d %-7d %-7d %-10s %-10s %-10s %-18s %-14s %s",
                  q->id, q->prioridad,
                  q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                  bufRmax, bufRasig, bufRpend,
                  q->IR, q->nombre, q->status);

        fila++;
        q = q->siguiente;
    }

    wrefresh(win_listos);
}

static void dibujar_terminados(WINDOW *win_term, int fila_base)
{

    if (!win_term)
        return;

    // Calcular la porción superior (alto_sup) a partir de la altura de la ventana
    int alto = getmaxy(win_term);
    int alto_sup = (alto * 60) / 100;

    // NO usar werase(win_term) → borra subventana
    // Solo limpiar la parte superior
    for (int i = 1; i < alto_sup; i++)
    {
        wmove(win_term, i, 1);
        wclrtoeol(win_term);
    }

    // Dibujar título y borde exterior
    box(win_term, 0, 0);
    wattron(win_term, COLOR_PAIR(3));
    mvwprintw(win_term, 0, 2, " TERMINADOS ");
    wattroff(win_term, COLOR_PAIR(3));

    // Encabezado dentro de la parte superior
    mvwprintw(win_term, 1, 1,
              "ID  Pr   Ax      Bx      Cx      Dx      Pc      "
              "Rmax       Rasig      Rpend      IR              nombre         Status");

    int fila = 2;

    PCB *q = lista_terminados;

    while (q && fila < alto_sup - 1)
    {
        char bufRmax[32], bufRasig[32], bufRpend[32];
        snprintf(bufRmax, sizeof(bufRmax), "%d,%d,%d", q->RmaxX, q->RmaxY, q->RmaxZ);
        snprintf(bufRasig, sizeof(bufRasig), "%d,%d,%d", q->RasigX, q->RasigY, q->RasigZ);
        snprintf(bufRpend, sizeof(bufRpend), "%d,%d,%d", q->RpendX, q->RpendY, q->RpendZ);

        mvwprintw(win_term, fila, 1,
                  "%-3d %-3d %-7d %-7d %-7d %-7d %-7d %-10s %-10s %-10s %-18s %-14s %s",
                  q->id, q->prioridad, q->Ax, q->Bx, q->Cx, q->Dx, q->Pc,
                  bufRmax, bufRasig, bufRpend, q->IR, q->nombre, q->status);

        fila++;
        q = q->siguiente;
    }

    // wrefresh(win_term);
}

/* ------------------ Ejecución de instrucciones ------------------ */
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
        strncpy(p->status, "Error", sizeof(p->status) - 1);
        GuardarContexto(p);
        return -1;
    }
    trim(inst);
    trim(rest);

    // Si la instrucción es MAX/GET/FRE, tratamos el `rest` como números y no como registros
    if (strcmp(inst, "MAX") == 0 || strcmp(inst, "GET") == 0 || strcmp(inst, "FRE") == 0)
    {
        int a = 0, b = 0, c = 0;
        // Agregamos espacios en el formato "%d , %d , %d" para que ignore espacios en el archivo
        if (sscanf(rest, "%d , %d , %d", &a, &b, &c) != 3)
        {
            /* Log de debug: sscanf falló al parsear MAX/GET/FRE */
            FILE *dbg = fopen("debug.log", "a");
            if (dbg)
            {
                fprintf(dbg, "[DEBUG] sscanf failed for line='%s' inst='%s' rest='%s'\n", buf, inst, rest);
                fclose(dbg);
            }
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
        }

        if (strcmp(inst, "MAX") == 0)
        {
            /* CORRECCIÓN: Validar contra el TOTAL del sistema (10),
               NO contra lo que sobra ahorita (Rx). */

            // Usamos RTotalX, RTotalY, RTotalZ en vez de Rx, Ry, Rz
            if (a > RTotalX || b > RTotalY || c > RTotalZ)
            {
                strncpy(p->status, "ErrorRecursos", sizeof(p->status) - 1);
                GuardarContexto(p);
                return -1; // Error: Pide más de lo que el sistema tiene físicamente instalado
            }

            p->RmaxX = a;
            p->RmaxY = b;
            p->RmaxZ = c;
            strncpy(p->status, "Correcto", sizeof(p->status) - 1);
            snprintf(p->IR, sizeof(p->IR), "MAX %d,%d,%d", a, b, c);
            return 0;
        }
        else if (strcmp(inst, "GET") == 0)
        {
            /* Log de debug: antes de procesar GET */
            FILE *dbg = fopen("debug.log", "a");
            if (dbg)
            {
                fprintf(dbg, "[DEBUG] PID=%d INST=%s REST='%s' PARSED=(%d,%d,%d) Rx_before=%d Ry_before=%d Rz_before=%d\n",
                        p ? p->id : -1, inst, rest, a, b, c, Rx, Ry, Rz);
                fclose(dbg);
            }
            // solicitar recursos al SO
            if (a <= Rx && b <= Ry && c <= Rz)
            {
                Rx -= a;
                Ry -= b;
                Rz -= c;
                p->RasigX += a;
                p->RasigY += b;
                p->RasigZ += c;

                /* --- INICIO MODIFICACIÓN PARA VER LOS RECURSOS --- */

                // 1. Forzar el redibujado de la ventanita de recursos AHORA MISMO
                if (win_recursos)
                {
                    dibujar_recursos(win_recursos, 1);
                    // Aseguramos que se pinte encima de todo
                    if (panel_recursos)
                        top_panel(panel_recursos);
                    update_panels();
                    doupdate();
                }

                // 2. PAUSA OBLIGATORIA (0.8 segundos) para que tu ojo alcance a ver el cambio
                usleep(800000);

                strncpy(p->status, "Correcto", sizeof(p->status) - 1);
                /* Log de debug: GET aplicado */
                dbg = fopen("debug.log", "a");
                if (dbg)
                {
                    fprintf(dbg, "[DEBUG] GET applied PID=%d new_Rx=%d new_Ry=%d new_Rz=%d Rasig=(%d,%d,%d)\n",
                            p ? p->id : -1, Rx, Ry, Rz, p->RasigX, p->RasigY, p->RasigZ);
                    fclose(dbg);
                }
                snprintf(p->IR, sizeof(p->IR), "GET %d,%d,%d", a, b, c);
                return 0;
            }
            else
            {
                // No hay suficientes recursos: marcar pendientes y devolver código "bloqueado"
                p->RpendX = a;
                p->RpendY = b;
                p->RpendZ = c;
                strncpy(p->status, "Bloqueado", sizeof(p->status) - 1);
                GuardarContexto(p);
                /* Log de debug: GET bloqueado */
                dbg = fopen("debug.log", "a");
                if (dbg)
                {
                    fprintf(dbg, "[DEBUG] GET blocked PID=%d need=(%d,%d,%d) Rx=%d Ry=%d Rz=%d\n",
                            p ? p->id : -1, a, b, c, Rx, Ry, Rz);
                    fclose(dbg);
                }
                return 2; // proceso bloqueado
            }
        }
        else /* FRE */
        {
            // liberar recursos al SO
            Rx += a;
            Ry += b;
            Rz += c;

            p->RasigX -= a;
            if (p->RasigX < 0)
                p->RasigX = 0;
            p->RasigY -= b;
            if (p->RasigY < 0)
                p->RasigY = 0;
            p->RasigZ -= c;
            if (p->RasigZ < 0)
                p->RasigZ = 0;

            // intentar desbloquear procesos bloqueados
            planificador_medio_plazo();

            strncpy(p->status, "Correcto", sizeof(p->status) - 1);
            snprintf(p->IR, sizeof(p->IR), "FRE %d,%d,%d", a, b, c);
            return 0;
        }
    }

    // Si no es MAX/GET/FRE, seguimos con el parseo original (instrucciones sobre registros)
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
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
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
        strncpy(p->status, "Error", sizeof(p->status) - 1);
        GuardarContexto(p);
        return -1;
    }

    // Verificar formato según instrucción
    if (strcmp(inst, "INC") == 0 || strcmp(inst, "DEC") == 0)
    {
        if (valstr[0])
        {
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
        }
    }
    else if (strcmp(inst, "MOV") == 0 || strcmp(inst, "ADD") == 0 ||
             strcmp(inst, "SUB") == 0 || strcmp(inst, "MUL") == 0 ||
             strcmp(inst, "DIV") == 0)
    {
        if (!valstr[0])
        {
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
        }
        if (!Numero(valstr))
        {
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
        }
    }
    else
    {
        strncpy(p->status, "Error", sizeof(p->status) - 1);
        GuardarContexto(p);
        return -1;
    }

    int valor = 0;
    if (valstr[0])
        valor = atoi(valstr);

    strncpy(p->status, "Correcto", sizeof(p->status) - 1);

    // Ahora las operaciones se realizan sobre los registros globales (gAx, gBx...)
    if (strcmp(inst, "MOV") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = mov(gAx, valor);
        else if (strcmp(var, "Bx") == 0)
            gBx = mov(gBx, valor);
        else if (strcmp(var, "Cx") == 0)
            gCx = mov(gCx, valor);
        else if (strcmp(var, "Dx") == 0)
            gDx = mov(gDx, valor);
    }
    else if (strcmp(inst, "ADD") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = add(gAx, valor);
        else if (strcmp(var, "Bx") == 0)
            gBx = add(gBx, valor);
        else if (strcmp(var, "Cx") == 0)
            gCx = add(gCx, valor);
        else if (strcmp(var, "Dx") == 0)
            gDx = add(gDx, valor);
    }
    else if (strcmp(inst, "SUB") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = sub(gAx, valor);
        else if (strcmp(var, "Bx") == 0)
            gBx = sub(gBx, valor);
        else if (strcmp(var, "Cx") == 0)
            gCx = sub(gCx, valor);
        else if (strcmp(var, "Dx") == 0)
            gDx = sub(gDx, valor);
    }
    else if (strcmp(inst, "MUL") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = mul(gAx, valor);
        else if (strcmp(var, "Bx") == 0)
            gBx = mul(gBx, valor);
        else if (strcmp(var, "Cx") == 0)
            gCx = mul(gCx, valor);
        else if (strcmp(var, "Dx") == 0)
            gDx = mul(gDx, valor);
    }
    else if (strcmp(inst, "DIV") == 0)
    {
        if (valor == 0)
        {
            strncpy(p->status, "Error", sizeof(p->status) - 1);
            GuardarContexto(p);
            return -1;
        }
        if (strcmp(var, "Ax") == 0)
            gAx = divi(gAx, valor);
        else if (strcmp(var, "Bx") == 0)
            gBx = divi(gBx, valor);
        else if (strcmp(var, "Cx") == 0)
            gCx = divi(gCx, valor);
        else if (strcmp(var, "Dx") == 0)
            gDx = divi(gDx, valor);
    }
    else if (strcmp(inst, "INC") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = inc(gAx);
        else if (strcmp(var, "Bx") == 0)
            gBx = inc(gBx);
        else if (strcmp(var, "Cx") == 0)
            gCx = inc(gCx);
        else if (strcmp(var, "Dx") == 0)
            gDx = inc(gDx);
    }
    else if (strcmp(inst, "DEC") == 0)
    {
        if (strcmp(var, "Ax") == 0)
            gAx = dec(gAx);
        else if (strcmp(var, "Bx") == 0)
            gBx = dec(gBx);
        else if (strcmp(var, "Cx") == 0)
            gCx = dec(gCx);
        else if (strcmp(var, "Dx") == 0)
            gDx = dec(gDx);
    }

    return 0;
}

/* ------------------ Planificador corto plazo ------------------ */
static void planificador_corto_plazo(WINDOW *win_ejec, WINDOW *win_listos, WINDOW *win_term)
{
    // === Bucle principal (planificador a corto plazo) ===
    while ((pcb_ejecucion = pop_listo()) != NULL)
    {
        strncpy(pcb_ejecucion->status, "Ejecucion", sizeof(pcb_ejecucion->status) - 1);
        pcb_ejecucion->status[sizeof(pcb_ejecucion->status) - 1] = '\0';
        CargarContexto(pcb_ejecucion);

        char linea[128];
        int instrucciones_ejecutadas = 0;

        while (fgets(linea, sizeof(linea), pcb_ejecucion->file))
        {
            rtrim(linea);
            if (linea[0] == ';' || linea[0] == '\0')
                continue;

            int fin = ejecutar_instruccion_linea(pcb_ejecucion, linea);
            pcb_ejecucion->Pc++;
            instrucciones_ejecutadas++;

            /* Control por teclado (q/ESC para terminar todo, p para pausar) */
            if (kbhit())
            {
                int ch = getchar();

                // Interrupción total
                if (ch == 'q' || ch == 'Q' || ch == 27)
                {
                    wattron(win_ejec, COLOR_PAIR(1));
                    mvwprintw(win_ejec, 8, 2, "[Q] Interrupción detectada. Cancelando ejecución global...");
                    wattroff(win_ejec, COLOR_PAIR(1));
                    wrefresh(win_ejec);
                    usleep(400000);

                    // Liberar todas las listas
                    liberar_lista(&lista_nuevos);
                    liberar_lista(&lista_listos);
                    liberar_lista(&lista_terminados);
                    liberar_lista(&lista_bloqueados);

                    // Cerrar ncurses limpiamente
                    endwin();

                    printf("\n[Ejecución interrumpida por el usuario]\n");
                    printf("[Todas las listas han sido liberadas correctamente]\n\n");
                    return; // <- Regresa al main() sin cerrar el programa
                }

                // Pausar ejecución
                else if (ch == 'p' || ch == 'P')
                {
                    wattron(win_ejec, COLOR_PAIR(4));
                    mvwprintw(win_ejec, 8, 2, "[P] Ejecución pausada. Presiona 'R' para reanudar...");
                    wattroff(win_ejec, COLOR_PAIR(4));
                    wrefresh(win_ejec);

                    int ch2;
                    while (1)
                    {
                        usleep(500000);
                        if (kbhit())
                        {
                            ch2 = getchar();

                            // Reanudar
                            if (ch2 == 'r' || ch2 == 'R')
                            {
                                mvwprintw(win_ejec, 8, 2, "Reanudando ejecución...                        ");
                                wrefresh(win_ejec);
                                usleep(500000);
                                break;
                            }

                            // Interrumpir también desde pausa
                            else if (ch2 == 'q' || ch2 == 'Q' || ch2 == 27)
                            {
                                mvwprintw(win_ejec, 8, 2, "[Q] Interrupción detectada durante pausa...");
                                wrefresh(win_ejec);
                                usleep(400000);

                                liberar_lista(&lista_nuevos);
                                liberar_lista(&lista_listos);
                                liberar_lista(&lista_terminados);
                                liberar_lista(&lista_bloqueados);

                                endwin();
                                printf("\n[Ejecución interrumpida durante pausa]\n");
                                printf("[Todas las listas han sido liberadas correctamente]\n\n");
                                return;
                            }
                        }
                    }
                }
            }

            // === Dibujar ejecución y paneles ===
            werase(win_ejec);
            box(win_ejec, 0, 0);
            wattron(win_ejec, COLOR_PAIR(4));
            mvwprintw(win_ejec, 0, 2, " EJECUCION (Quantum = %d) ", Q);
            wattroff(win_ejec, COLOR_PAIR(4));

            mvwprintw(win_ejec, 2, 2, "ID  Ax      Bx      Cx      Dx      Pc      IR                  Nombre        Estado");
            mvwhline(win_ejec, 3, 2, ACS_HLINE, COLS - 6);
            mvwprintw(win_ejec, 4, 2, "%-4d %-7d %-7d %-7d %-7d %-7d %-20s %-12s %s",
                      pcb_ejecucion->id, gAx, gBx, gCx, gDx, pcb_ejecucion->Pc,
                      pcb_ejecucion->IR, pcb_ejecucion->nombre, pcb_ejecucion->status);

            dibujar_listos(win_listos, 2);
            dibujar_terminados(win_term, 2);

            update_panels();
            doupdate();

            usleep(500000); // ritmo de animación

            // === Manejo de fin, error o bloqueo ===
            if (fin == -1 || fin == 1 || fin == 2)
            {
                if (fin == -1)
                    strncpy(pcb_ejecucion->status, "Error", sizeof(pcb_ejecucion->status) - 1);
                else if (fin == 1)
                    strncpy(pcb_ejecucion->status, "Correcto", sizeof(pcb_ejecucion->status) - 1);
                else if (fin == 2)
                    strncpy(pcb_ejecucion->status, "Bloqueado", sizeof(pcb_ejecucion->status) - 1);

                GuardarContexto(pcb_ejecucion);

                if (fin == -1 || fin == 1)
                {

                    if (win_recursos)
                    {
                        dibujar_recursos(win_recursos, 1);
                        if (panel_recursos)
                            top_panel(panel_recursos);
                        update_panels();
                        doupdate();
                    }

                    anexar_terminado_final(pcb_ejecucion);

                    // Cerrar archivo del proceso que terminó y liberar memoria
                    if (pcb_ejecucion->file)
                        fclose(pcb_ejecucion->file);
                    if (pcb_ejecucion->TMP)
                        free(pcb_ejecucion->TMP);
                    free(pcb_ejecucion);
                    pcb_ejecucion = NULL;

                    // Tras finalizar un proceso, mover nuevos a listos
                    planificador_largo_plazo();
                    dibujar_nuevos(win_nuevos, 2);
                    dibujar_listos(win_listos, 2);
                    dibujar_terminados(win_term, 2);
                    update_panels();
                    doupdate();

                    goto siguiente_proceso;
                }
                else if (fin == 2) // Bloqueado
                {
                    // 1. Mover a la lista de bloqueados
                    push_bloqueado(pcb_ejecucion);

                    dibujar_listos(win_listos, 2);
                    dibujar_bloqueados(win_bloqueados, 2);
                    if (win_recursos)
                        dibujar_recursos(win_recursos, 1);

                    wattron(win_ejec, COLOR_PAIR(3));
                    mvwprintw(win_ejec, 6, 2, "Proceso %d BLOQUEADO por falta de recursos!", pcb_ejecucion->id);
                    wattroff(win_ejec, COLOR_PAIR(3));

                    update_panels();
                    doupdate();

                    usleep(2000000);

                    // Limpiar el mensaje y la variable de ejecución
                    mvwprintw(win_ejec, 6, 2, "                                           ");
                    pcb_ejecucion = NULL;

                    goto siguiente_proceso;
                }
            }

            // === Quantum alcanzado ===
            if (instrucciones_ejecutadas >= Q)
            {
                GuardarContexto(pcb_ejecucion);
                strncpy(pcb_ejecucion->status, "Listo", sizeof(pcb_ejecucion->status) - 1);
                pcb_ejecucion->status[sizeof(pcb_ejecucion->status) - 1] = '\0';

                /* Re-encolar según prioridad */
                push_listo(pcb_ejecucion);
                pcb_ejecucion = NULL;
                break;
            }
        } // fin while fgets

        /* Si se salió del while de lectura sin haber terminado (por EOF),
           pero pcb_ejecucion aún existe, entonces cerrarlo y anexarlo a terminados. */
        if (pcb_ejecucion)
        {
            // Si el archivo llegó a su fin sin encontrar END, lo tratamos como final correcto
            GuardarContexto(pcb_ejecucion);
            strncpy(pcb_ejecucion->status, "Correcto", sizeof(pcb_ejecucion->status) - 1);
            anexar_terminado_final(pcb_ejecucion);
            if (pcb_ejecucion->file)
                fclose(pcb_ejecucion->file);
            free(pcb_ejecucion);
            pcb_ejecucion = NULL;
        }

    siguiente_proceso:
        // Al liberar un proceso, revisar si hay nuevos que puedan pasar
        planificador_largo_plazo();
        dibujar_nuevos(win_nuevos, 2);
        dibujar_listos(win_listos, 2);
        dibujar_terminados(win_term, 2);
        update_panels();
        doupdate();
    } // fin while pop_listo != NULL
}

/* ------------------ Ejecución completa con quantum y paneles ncurses ------------------ */
int ejecutar_archivo(const char *ruta_mult)
{
    // 1. Reiniciar los recursos del sistema a sus valores totales (10)
    Rx = RTotalX;
    Ry = RTotalY;
    Rz = RTotalZ;

    // 2. Reiniciar los registros de la CPU simulada a 0
    gAx = 0;
    gBx = 0;
    gCx = 0;
    gDx = 0;
    // === Limpieza previa de listas (por si se reusa la función) ===
    liberar_lista(&lista_listos);
    liberar_lista(&lista_terminados);
    liberar_lista(&lista_nuevos);
    liberar_lista(&lista_bloqueados);
    lista_nuevos = lista_listos = lista_terminados = lista_bloqueados = NULL;

    // Inicializar generador aleatorio para prioridades (una vez)
    srand((unsigned int)time(NULL));

    // === Cargar múltiples archivos ASM ===
    char tmp[1024];
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
            tok = strtok(NULL, " ");
            continue;
        }
        PCB *p = (PCB *)malloc(sizeof(PCB));
        if (!p)
        {
            fclose(f);
            printf("Error: malloc PCB\n");
            tok = strtok(NULL, " ");
            continue;
        }

        crear_PCB(p, proc_id++, tok, f);

        if (p->prioridad < 1 || p->prioridad > 4)
            p->prioridad = (rand() % 4) + 1;

        push_nuevo(p);
        tok = strtok(NULL, " ");
    }

    // === Inicialización de ncurses ===
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();

    init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Encabezado
    init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Listos
    init_pair(3, COLOR_RED, COLOR_BLACK);    // Terminados
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Ejecución
    init_pair(5, COLOR_WHITE, COLOR_BLACK);  // General

    // === Crear ventanas (usar las variables globales) ===
    int mid_x = COLS / 2;
    int mid_y = LINES / 2;

    win_ejecucion = newwin(8, COLS - 2, 1, 1);
    int small_h = (mid_y - 12 > 4 ? mid_y - 12 : 4);

    win_nuevos = newwin(small_h, mid_x - 2, 10, 1);
    win_listos = newwin(small_h, mid_x - 2, 10, mid_x + 1);

    win_bloqueados = newwin(small_h, COLS - 2, 12 + small_h + 8, 1);
    win_terminados = newwin(LINES - mid_y - 2 > 4 ? LINES - mid_y - 2 : 4, COLS - 2, mid_y + 1, 1);

    // Crear panels
    PANEL *pnl_ejec = new_panel(win_ejecucion);
    PANEL *pnl_nuevos = new_panel(win_nuevos);
    PANEL *pnl_listos = new_panel(win_listos);
    PANEL *pnl_term = new_panel(win_terminados);
    PANEL *pnl_bloq = new_panel(win_bloqueados);

    // Dibujar bordes y títulos
    box(win_ejecucion, 0, 0);
    wattron(win_ejecucion, COLOR_PAIR(4));
    mvwprintw(win_ejecucion, 0, 2, " EJECUCION (Quantum = %d) ", Q);
    wattroff(win_ejecucion, COLOR_PAIR(4));

    box(win_nuevos, 0, 0);
    wattron(win_nuevos, COLOR_PAIR(5));
    mvwprintw(win_nuevos, 0, 2, " NUEVOS ");
    wattroff(win_nuevos, COLOR_PAIR(5));

    box(win_listos, 0, 0);
    wattron(win_listos, COLOR_PAIR(2));
    mvwprintw(win_listos, 0, 2, " LISTOS ");
    wattroff(win_listos, COLOR_PAIR(2));

    box(win_terminados, 0, 0);
    wattron(win_terminados, COLOR_PAIR(3));
    mvwprintw(win_terminados, 0, 2, " TERMINADOS ");
    wattroff(win_terminados, COLOR_PAIR(3));

    box(win_bloqueados, 0, 0);
    wattron(win_bloqueados, COLOR_PAIR(1));
    mvwprintw(win_bloqueados, 0, 2, " BLOQUEADOS ");
    wattroff(win_bloqueados, COLOR_PAIR(1));

    update_panels();
    doupdate();

    /* Crear la ventanita de recursos (pequeña, al lado derecho) */
    int alto_rec = 3;
    int ancho_rec = 36;
    int rec_y = 1;
    int rec_x = (COLS > ancho_rec + 4) ? (COLS - ancho_rec - 2) : 1;
    win_recursos = newwin(alto_rec, ancho_rec, rec_y, rec_x);
    panel_recursos = new_panel(win_recursos);
    box(win_recursos, 0, 0);
    wattron(win_recursos, COLOR_PAIR(4));
    mvwprintw(win_recursos, 0, 2, " RECURSOS ");
    wattroff(win_recursos, COLOR_PAIR(4));
    dibujar_recursos(win_recursos, 1);

    // Dibujar los procesos nuevos inicialmente y planificar
    dibujar_nuevos(win_nuevos, 2);

    // Ejecutar el planificador a largo plazo primero para llenar "listos" desde "nuevos"
    planificador_largo_plazo();

    // Redibujar nuevos y listos después de la planificación inicial
    dibujar_nuevos(win_nuevos, 2);
    dibujar_listos(win_listos, 2);
    update_panels();
    doupdate();

    // === Bucle principal de la simulación ===
    while (lista_listos != NULL || lista_nuevos != NULL || lista_bloqueados != NULL)
    {
        // Si no hay procesos listos pero hay nuevos, intentar moverlos
        if (!lista_listos && lista_nuevos)
        {
            planificador_largo_plazo();
            dibujar_nuevos(win_nuevos, 2);
            dibujar_listos(win_listos, 2);
            update_panels();
            doupdate();
        }

        // Ejecuta el planificador corto (procesa todos los listos disponibles)
        planificador_corto_plazo(win_ejecucion, win_listos, win_terminados);

        // Después de una ejecución corta, intentar mover nuevos a listos (si hay espacio)
        planificador_largo_plazo();

        // Redibujar todo
        dibujar_nuevos(win_nuevos, 2);
        dibujar_listos(win_listos, 2);
        dibujar_terminados(win_terminados, 2);
        dibujar_bloqueados(win_bloqueados, 2);
        update_panels();
        doupdate();

        // pequeña pausa para evitar loop demasiado rápido (opcional)
        usleep(700000);
        if (lista_listos == NULL && lista_nuevos == NULL && pcb_ejecucion == NULL && lista_bloqueados != NULL)
        {
            // Opcional: Mostrar mensaje de que quedaron procesos olvidados
            wattron(win_ejecucion, COLOR_PAIR(3)); // Rojo
            mvwprintw(win_ejecucion, 6, 2, "DEADLOCK: Procesos bloqueados sin esperanza. Terminando...");
            wattroff(win_ejecucion, COLOR_PAIR(3));
            wrefresh(win_ejecucion);
            usleep(2000000); // Pausa de 2 seg para leer el mensaje

            break;
        }
    }

    Rx = RTotalX;
    Ry = RTotalY;
    Rz = RTotalZ;

    // === Mensaje final ===
    werase(win_ejecucion);
    box(win_ejecucion, 0, 0);
    wattron(win_ejecucion, COLOR_PAIR(2));
    mvwprintw(win_ejecucion, 1, 2, "Ejecución finalizada. Presiona una tecla para salir...");
    wattroff(win_ejecucion, COLOR_PAIR(2));
    wrefresh(win_ejecucion);

    /* Redibujar y poner la ventanita de recursos encima justo antes de la pausa final */
    if (win_recursos)
    {
        dibujar_recursos(win_recursos, 1);
        if (panel_recursos)
            top_panel(panel_recursos);
    }
    update_panels();
    doupdate();

    getch();

    // === Limpieza ===
    del_panel(pnl_ejec);
    del_panel(pnl_nuevos);
    del_panel(pnl_listos);
    del_panel(pnl_term);
    del_panel(pnl_bloq);

    delwin(win_ejecucion);
    delwin(win_nuevos);
    delwin(win_listos);
    delwin(win_terminados);
    delwin(win_bloqueados);

    win_ejecucion = win_nuevos = win_listos = win_terminados = NULL;
    win_bloqueados = NULL;

    endwin();

    // Liberar cualquier resto (seguridad)
    liberar_lista(&lista_nuevos);
    liberar_lista(&lista_listos);
    liberar_lista(&lista_terminados);
    liberar_lista(&lista_bloqueados);

    return 0;
}

#endif
