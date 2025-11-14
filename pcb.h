#ifndef PCB_H
#define PCB_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PCB {
    int id; // identificador de proceso (no de instruccion)
    int Pc; // program counter (1-based)
    int Ax, Bx, Cx, Dx; // registros
    char IR[64]; // instrucción actual (texto)
    char status[32]; // estado: "listo", "ejecucion", "terminado", "error"
    char nombre[64]; // nombre del archivo .asm
    FILE *file; // apuntador al archivo abierto
    int prioridad; //1..4 (1 de mayor prioridad)
    struct PCB *siguiente; // puntero al siguiente (lista ligada)
} PCB;

/* helper para inicializar PCB (opcional) */
/*Nota: srand() NO se llama aquí (llamar una vez en ejecutar_archivo antes de crear PCBs)*/
static void crear_PCB(PCB *p, int id, const char *nombre, FILE *file) {
    p->id = id;
    p->Pc = 1;
    p->Ax = p->Bx = p->Cx = p->Dx = 0;
    p->IR[0] = '\0';
    strncpy(p->status, "listo", sizeof(p->status)-1);
    p->status[sizeof(p->status)-1] = '\0';
    strncpy(p->nombre, nombre, sizeof(p->nombre)-1);
    p->nombre[sizeof(p->nombre)-1] = '\0';
    p->file = file;
    /*prioridad aleatoria */
    p->prioridad = (rand() % 4) + 1;
    p->siguiente = NULL;
}
#endif
