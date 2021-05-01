#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_THREADS 1000

pthread_t threads[MAX_THREADS] = {0};
int thread_count = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_variable = PTHREAD_COND_INITIALIZER;

void createThread(void *(func)(void* vargp));
void *sayHello();
void doStuff(int N);

int main(int argc, char *argv[]){

  // NOTE: This code should be run as follows:
  //                   gcc -pthread -o [file name].c [output name]
  //           Or, alternatively:
  //                   gcc -o [output name] [file name].c -pthread
  
  // Command signiture: part1 -n [number of threads] -p [question probability] -q [number of questions]
  
  char* numberOfThreads = argv[2];
  char* questionProbability = argv[4];
  char* numberOfQuestions = argv[6];  

  int N = (*numberOfThreads) -'0';

  printf("%d\n", N);
  
  doStuff(N);

  // Wait for the threads to complete.
  for (int i = 0; i < N; i++)
    pthread_join(threads[i], NULL);
  
  printf("%d\n", thread_count);
  
  return 0;
}

void doStuff(int N){
  for(int i = 0; i < N; i++){
    createThread(sayHello);
  }
}

void *sayHello(void *id){
  pthread_mutex_lock(&mutex);
  pthread_cond_wait(&condition_variable, &mutex);
  printf("Hello\n");
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&condition_variable);
}

void createThread(void *(func)(void* vargp)){
  pthread_create(&threads[thread_count++], NULL, func, NULL);
}
