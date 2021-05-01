#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

pthread_t *commentators;
pthread_t moderator;
int N = 0;
int Q = 0;
int T = 0;
double P = 0;

pthread_mutex_t mutex;
pthread_cond_t condition_variable; // todo: how many do we need?

void createThreads(void *(func)(void* vargp), void *(mod_func)(void* vargp));
void *sayHello();
void *sayHello2();
void doStuff(int N);

int main(int argc, char *argv[]){

  // NOTE: This code should be run as follows:
  //                   gcc -lpthread -o [file name].c [output name]
  //           Or, alternatively:
  //                   gcc -o [output name] [file name].c -lpthread
  
  // Command signiture: part1 -n [number of threads] -p [question probability] -q [number of questions] -t [max speaking time]

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&condition_variable, NULL);
  
  char* inputN = argv[2];
  N = atoi(inputN);
  commentators = (pthread_t *) calloc(N, sizeof(pthread_t));
  char* inputP = argv[4];
  P = atof(inputP);
  char* inputQ = argv[6];
  Q = atoi(inputQ);
  char* inputT = argv[8];
  T = atoi(inputT);

  doStuff(N);

  // Wait for the threads to complete.
  int i;
  for (i = 0; i < N; i++)
    pthread_join(commentators[i], NULL);
  pthread_join(moderator, NULL);
  
  return 0;
}

void doStuff(int N){
    createThreads(sayHello, sayHello2); /// TODO: change funcs
}

int cnt = 0;
void *sayHello(void *id){
  pthread_mutex_lock(&mutex);
  cnt++;
  printf("Com Hello, cnt is: %d\n", cnt);
  if (cnt == 4)
  {
      pthread_cond_signal(&condition_variable);
  }
  pthread_mutex_unlock(&mutex);
  pthread_exit(NULL);
}

void *sayHello2(void *id){
  pthread_cond_wait(&condition_variable, &mutex);
  printf("Mod Hello, cnt is: %d\n", cnt);
  pthread_mutex_unlock(&mutex);
  pthread_exit(NULL);
}

void createThreads(void *(com_func)(void* vargp), void *(mod_func)(void* vargp)){
  pthread_create(&moderator, NULL, mod_func, NULL); 
  int i;
  for(i=0; i<N; i++) // create commentator threads
    pthread_create(&commentators[i], NULL, com_func, NULL);
  // create moderator threads
}
