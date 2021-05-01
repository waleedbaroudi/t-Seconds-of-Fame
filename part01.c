#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

pthread_t tid[1024] = {0};
int thread_count = 0;

void createThread(void *(func)(void* vargp));

int main(int argc, char *argv[]){

  // REMINDER: This code should be run as follows:
  //                   gcc -pthread -o [file name].c [output name]
  //           Or, alternatively:
  //                   gcc -o [output name] [file name].c -pthread
  
  // Command signiture: part1 -n [number of threads] -p [question probability] -q [number of questions]
  
  char* numberOfThreads = argv[2];
  char* questionProbability = argv[4];
  char* numberOfQuestions = argv[6];  

  
  return 0;
}

void createThread(void *(func)(void* vargp)){
   pthread_create(&tid[thread_count++], NULL, func, NULL);
}
