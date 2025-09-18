#ifndef OPERACIONES_H //guarda de inclusiones (ifndef, define, endif)
#define OPERACIONES_H //guarda de inclusiones
int mov(int a, int b); //prototipos de las funciones
int add(int a, int b); //prototipos de las funciones
int sub(int a, int b); //prototipos de las funciones
int mul(int a, int b); 
int divi(int a, int b);
int inc(int a);
int dec(int a);

//funciones de operaciones
//funcion de asignacion
int mov(int a, int b){
    return a=b;
}

//funcon de suma
int add(int a, int b){
    return a+b;
}

//funcion de resta
int sub(int a, int b){
    return a-b;
}

//funcion de multiplicacion
int mul(int a, int b){
    return a*b;
}

//funcion de division
int divi(int a, int b){
    return a/b;
}

//funcion de incremento y decremento
int inc(int a){
    a++;
    return a;
}

int dec(int a){
    a--;
    return a;
}

#endif




