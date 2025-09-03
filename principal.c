#include <stdio.h>
#include "interprete.h"
#include <string.h>
#include <stdlib.h> 

int main(){
    int Ax=0, Bx=0, Cx=0, Dx=0, Pc=1, id=0,i,valor;
    char linea[20];
    char inst[4],var[2],val[10],copia[3];
    char IR[20];
    char *delimitador = " ,";
    char *token;
    FILE *archivo;
    archivo = fopen("a.asm","r");
    if (archivo == NULL) {
        printf("No se pudo abrir el archivo.\n");
        return 1;
    }
    
    printf("Ax = %d     Bx = %d     Cx = %d     Dx = %d       Pc = %d\n\n",Ax,Bx,Cx,Dx,Pc);
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        i=0;
        //printf("%s", linea);
        token = strtok(linea, delimitador);
        strcpy(IR,"");
        strcpy(copia, token);
        while (token != NULL) {
            //printf("Token: %s\n", token);
            switch (i)
            {
            case 0:
                //printf("Token: %s\n", token);
                strcpy(inst, token);
                strcat(IR,token);
                //printf("%s\n", inst);
                break;
            case 1:
                strcpy(var, token);
                strcat(IR," ");
                strcat(IR,token);
                //printf("%s\n", var);
                break;
            
            case 2:
                strcpy(val, token);
                strcat(IR,",");
                strcat(IR,token);
                //printf("%s\n", val);
                break;
            default:
                break;
            }
            //printf("Token: %s\n", tokens[i]);
            
            token = strtok(NULL, delimitador);
            i++;
        }
        int valor=atoi(val);
        //printf("%d\n", valor);
        if (strcmp(inst, "MOV") == 0) { 
            if (strcmp(var,"Ax")==0){
                Ax=mov(Ax,valor);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=mov(Bx,valor);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=mov(Cx,valor);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=mov(Dx,valor);
            }     
        } 
        else if (strcmp(inst, "ADD") == 0) {
            if (strcmp(var,"Ax")==0){
                Ax=add(Ax,valor);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=add(Bx,valor);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=add(Cx,valor);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=add(Dx,valor);
            }  
        }
        else if (strcmp(inst, "SUB") == 0) {
            if (strcmp(var,"Ax")==0){
                Ax=sub(Ax,valor);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=sub(Bx,valor);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=sub(Cx,valor);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=sub(Dx,valor);
            }  
        }
        else if (strcmp(inst, "MUL") == 0) {
            if (strcmp(var,"Ax")==0){
                Ax=mul(Ax,valor);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=mul(Bx,valor);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=mul(Cx,valor);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=mul(Dx,valor);
            }  
        }
        else if (strcmp(inst, "DIV") == 0) {
            if (strcmp(var,"Ax")==0){
                Ax=divi(Ax,valor);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=divi(Bx,valor);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=divi(Cx,valor);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=divi(Dx,valor);
            }  
        }
        else if (strcmp(inst, "INC") == 0) {
            //printf("%s\n", var);
            if (strcmp(var,"Ax")==0){
                Ax=inc(Ax);
            }
            else if (strcmp(var,"Bx")==0){
                
                Bx=inc(Bx);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=inc(Cx);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=inc(Dx);
            }  
        }
        else if (strcmp(inst, "DEC") == 0) {
            if (strcmp(var,"Ax")==0){
                Ax=dec(Ax);
            }
            else if (strcmp(var,"Bx")==0){
                Bx=dec(Bx);
            }
            else if (strcmp(var,"Cx")==0){
                Cx=dec(Cx);
            }
            else if (strcmp(var,"Dx")==0){
                Dx=dec(Dx);
            }  
        }
        else {
            printf("Comando no reconocido.\n");
        }
        Pc++;
        printf("Ax = %d     Bx = %d     Cx = %d     Dx = %d       Pc = %d       IR = %s\n",Ax,Bx,Cx,Dx,Pc,IR);
    }
    fclose(archivo); 
    return 0;
}