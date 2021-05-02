#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>

pthread_t moderator;
pthread_t *commentators;

int *requests; // comment requests (reqyests[i]: commentator i requests to asnwer. -1: no comment)
int N = 0;
int Q = 0;
int T = 0;
double P = 0;

pthread_cond_t *com_cond;
pthread_cond_t mod_cond;
pthread_mutex_t request_mutex;
pthread_mutex_t temp_mutex;

void createThreads(void *(func)(void* vargp), void *(mod_func)(void* vargp));
void *mod_function();
void *com_function(void *slf);
void doStuff(int N);
void clearRequests();
void initialize_conds();
void notifyModerator();
void signal_coms();
int willDoAction(double p);
int requestComment(int com_number);

int main(int argc, char *argv[]){
  srand(time(0));
  // NOTE: This code should be run as follows:
  //                   gcc -lpthread -o [file name].c [output name]
  //           Or, alternatively:
  //                   gcc -o [output name] [file name].c -lpthread
  
  // Command signiture: part1 -n [number of threads] -p [question probability] -q [number of questions] -t [max speaking time]

  pthread_mutex_init(&request_mutex, NULL);
  pthread_mutex_init(&temp_mutex, NULL);
  initialize_conds();
  
  char* inputN = argv[2];
  N = atoi(inputN);
  commentators = (pthread_t *) calloc(N, sizeof(pthread_t));
  com_cond = (pthread_cond_t *) calloc(N, sizeof(pthread_cond_t));
  requests = (int *) calloc(N, sizeof(int));
  clearRequests(); // initialize requests to -1
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
    createThreads(com_function, mod_function); 
}

  // pthread_mutex_lock(&mutex);
  // cnt++;
  // printf("Com Hello, cnt is: %d\n", cnt);
  // if (cnt == 4)
  // {
  //     pthread_cond_signal(&condition_variable);
  // }
  // pthread_mutex_unlock(&mutex);

void *com_function(void *slf){
  int self = *((int *) slf);
  int i;
  for(i=0; i < Q; i++) {
    pthread_cond_wait(&com_cond[self], &temp_mutex); /// todo: mutex?
    if(!requestComment(self))
      continue;
    pthread_cond_wait(&com_cond[self], &temp_mutex); /// todo: mutex???







  }

  pthread_exit(NULL);
}

// returns whether the commentator has requested to comment
int requestComment(int com_number) {
    int result = 0;
    printf("Commentator #%d calling request\n", com_number);
    pthread_mutex_lock(&request_mutex);
    printf("Commentator #%d acquired lock\n", com_number);
    int queue_position = 0;
    int i;
    for (i = 0; i < N; i++)
    {
      if (requests[i] == -1)
      {
        if (willDoAction(P)) {
          requests[i] = com_number;
          result = 1;
          printf("Commentator #%d generates an answer, position in queue: %d\n", com_number, queue_position);
        } else {
          requests[i] = 0;
        }
        break;
      }
      if (requests[i] != 0)
      {
        queue_position++;
      }
    }
    notifyModerator();
    printf("Commentator #%d releasing lock\n", com_number);
    pthread_mutex_unlock(&request_mutex);
    printf("Commentator #%d released lock\n\n", com_number);
    return result;
}

void *mod_function(){
  int i;
  for(i=0; i < Q; i++) {
    printf("Moderator asked Question %d\n", i+1);
    signal_coms();
    pthread_cond_wait(&mod_cond, &temp_mutex);
    printf("Moderator WOKE UP %d\n", i+1);
    // allow commentators to speak in order

    clearRequests();




  }

  pthread_exit(NULL);
}

void signal_coms() {
  int i;
  for(i=0; i < N; i++) {
    pthread_cond_signal(&com_cond[i]);
  }
}

void notifyModerator() {
    if (requests[N-1] == -1)
    {
      return;
    }

pthread_cond_signal(&mod_cond);
}

void createThreads(void *(com_func)(void* vargp), void *(mod_func)(void* vargp)){
  // create moderator threads
  pthread_create(&moderator, NULL, mod_func, NULL); 
  int i;
  for(i=0; i<N; i++) // create commentator threads
    pthread_create(&commentators[i], NULL, com_func, (void*) &i);
}

void initialize_conds() {
  int i;
  for(i=0; i<N; i++) {
    pthread_cond_init(&com_cond[i], NULL);
  }
}

int willDoAction(double p){
  printf("calling willDoAction with p=%f\n", p);
  int r = rand() % 100;
  return r <= p * 100;
}

void clearRequests() {
  int i;
  for (i = 0; i < N; i++) {
    requests[i] = -1;
  }
}