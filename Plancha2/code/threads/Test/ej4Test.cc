#include "../scheduler.hh"
#include "../synch.hh"
#include <unistd.h>
extern Thread* currentThread;
Lock *l;

#define MAX 3 /* segundos */
#define NUM_THREADS 4


// Testeando 4-b
void imprimir2(void *arg){
  usleep(random()%MAX);
  for(int i = 0; i < 10;i++){
     l-> Acquire();
     printf("soy %s y tengo prioridad %d, iteracion %d \n",currentThread->GetName(),currentThread -> GetPriority(),i);
     currentThread -> Yield();
     l-> Release();
  }
  currentThread -> Finish();

}


void ej4BTest(){
  printf("Testeando 4-b\n");
  l = new Lock(NULL);
  char * names[NUM_THREADS];
  Thread *threads[NUM_THREADS];
  char v[10];
  int *mjes = new int[NUM_THREADS];

  for(int i = 0;i < NUM_THREADS;i++){
     mjes[i] = i;
  }

  for (int i = 0 ; i < NUM_THREADS; i++){
    names[i] = new char [64];
    sprintf(names[i],"Hilo %d",i);
    threads[i] = new Thread(names[i],false,random()%L_PRIORITY);
    threads[i] -> Fork(imprimir2, names+i);
    }
}



////////////////////////////////////////////////////////////////////////////////////


//Testeando 4-a

void imprimir(void *arg){
  usleep(random()%MAX);
  for(int i = 0; i < 10;i++){

     printf("soy %s y tengo prioridad %d, iteracion %d \n",currentThread->GetName(),currentThread -> GetPriority(),i);
     currentThread -> Yield();
  }
 currentThread -> Finish();
}

void ej4ATest(){
  printf("Testeando 4-a\n");
  char * names[NUM_THREADS];
  Thread *threads[NUM_THREADS];
  int *mjes = new int[NUM_THREADS];

  for(int i = 0;i < NUM_THREADS;i++){
     mjes[i] = i;
  }

  for (int i = 0 ; i < NUM_THREADS; i++){
    names[i] = new char [64];
    sprintf(names[i],"Hilo %d",i);
    threads[i] = new Thread(names[i],false,random()%L_PRIORITY);
    threads[i] -> Fork(imprimir,(void*) names+i);
    }

}
