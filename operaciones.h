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
    return a+b; //devuelve la suma de a y b
}

//funcion de resta
int sub(int a, int b){
    return a-b; //devuelve la resta de a y b
}

//funcion de multiplicacion
int mul(int a, int b){
    return a*b; //devuelve el producto de a y b
}

//funcion de division
int divi(int a, int b){
    return a/b; //devuelve el resultado de la division de a entre b
}

//funcion de incremento y decremento
int inc(int a){
    a++;    //incrementa en una unidad el valor de a
    return a;
}

int dec(int a){
    a--;    //decrementa en una unidad el valor de a
    return a;
}

#endif
