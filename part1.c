#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <sys/time.h>

#define BREAKING_NEWS_CHECK 1
#define BREAKING_NEWS_PERIOD 5

pthread_t moderator;
pthread_t *commentators;
pthread_t breaking_news;

int *requests; // comment can_request (reqyests[i]: commentator i can_request to asnwer. -1: no comment)
int N = 0;
int Q = 0;
int T = 0;
double P = 0;
double B = 0;

pthread_cond_t com_cond;
pthread_cond_t mod_cond;
pthread_cond_t news_cond;

pthread_mutex_t request_mutex;
pthread_mutex_t mod_mutex;
pthread_mutex_t news_mutex;

sem_t precedence_sem;

int coms_entered = 0;
int *can_request;
int can_speak = -1;
int in_breaking_news = 0;
int debate_in_progress = 1;

void createThreads(void *(func)(void* vargp), void *(mod_func)(void* vargp), void *(news_func)(void* vargp));
void *mod_function();
void *com_function(void *slf);
void *news_function();
void doStuff();
void recreate_sem();
void clear_requests();
void initialize_conds();
void notifyModerator();
void signal_coms();
void print_requests_queue();
int willDoAction(double p);
int requestComment(int com_number);
int pthread_sleep (int seconds);

int main(int argc, char *argv[]){
  srand(time(0));
  // NOTE: This code should be run as follows:
  //                   gcc -lpthread -o [file name].c [output name]
  //           Or, alternatively:
  //                   gcc -o [output name] [file name].c -lpthread
  
  // Command signiture: part1 -n [number of threads] -p [question probability] -q [number of questions] -t [max speaking time] -b [probability of brekaing news]

  char* inputN = argv[2];
  N = atoi(inputN);
  char* inputP = argv[4];
  P = atof(inputP);
  char* inputQ = argv[6];
  Q = atoi(inputQ);
  char* inputT = argv[8];
  T = atoi(inputT);
  char* inputB = argv[10];
  B = atof(inputB);

  sem_init(&precedence_sem, 0, 0);

  requests = (int *) calloc(N, sizeof(int));
  can_request = (int *) calloc(N, sizeof(int));
  commentators = (pthread_t *) calloc(N, sizeof(pthread_t));

  pthread_mutex_init(&request_mutex, NULL);
  pthread_mutex_init(&mod_mutex, NULL);
  pthread_mutex_init(&news_mutex, NULL);

  initialize_conds();
  clear_requests(); // initialize can_request to -1

  createThreads(com_function, mod_function, news_function); 

  while (debate_in_progress)
  {
    if (willDoAction(B))
    {
      pthread_mutex_lock(&news_mutex);
      in_breaking_news = 1;
      pthread_cond_signal(&news_cond);
      while (in_breaking_news)
      {
        pthread_cond_wait(&news_cond, &news_mutex);
      }
      pthread_mutex_unlock(&news_mutex);
      
      /// TODO: implement breaking news case
    }
    pthread_sleep(BREAKING_NEWS_CHECK);
  }
  
  // Wait for the threads to complete.
  int i;
  for (i = 0; i < N; i++)
  {
    pthread_join(commentators[i], NULL);
  }
  pthread_join(moderator, NULL);
  pthread_join(breaking_news, NULL);

  // free allocated memory
  free(requests);
  free(commentators);
  free(can_request);
  
  return 0;
}

void *com_function(void *slf){
  int self = *((int *) slf);
  free(slf);
  int i;
  for(i=0; i < Q; i++) {
    // printf("COM WANTS REQ MUTEX TO can_request COMMENT/NO COMMENT\n");
    pthread_mutex_lock(&request_mutex);
    // printf("COM GOT REQ MUTEX TO can_request, WAITING com_cond SIGNAL FROM MOD\n");
    coms_entered++;
    // printf("COMS ENTERED: %d\n", coms_entered);
    if (coms_entered == N)
    { // allow the moderator in after the last commentator enters
      coms_entered = 0;
      sem_post(&precedence_sem);
    }
    // printf("COM %d waiting to can_request..\n", self);
    do
    {
      pthread_cond_wait(&com_cond, &request_mutex);
    } while (!can_request[self - 1]);

    // printf("COM GOT com_cond SIGNAL\n");
    // printf("COM %d requesting..\n", self);
    if(!requestComment(self)) {
      printf("Com %d did not request to answer, continue..\n", self);
      
      pthread_mutex_unlock(&request_mutex);
      continue;
    }
    // printf("COM WAITING SPEAK SIGNAL FROM MOD\n");
    // printf("COM %d waiting to can_speak..\n", self);
    while (can_speak != self)
    {
     pthread_cond_wait(&com_cond, &request_mutex); /// todo: mutex???
    }

    int time = (rand() % T) + 1;
    printf("Commentator #%d's turn to speak for %d seconds.\n", self, time);
    if(pthread_sleep(time) != 0) { // SPEAK (SLEEP)
      /// TODO: Handle sleep error here
    }

    printf("Commentator #%d finished speaking.\n", self);

    // printf("COM WANTS MOD MUTEX TO SIGNAL MOD: FINISHED SPEAKING\n");
    pthread_mutex_lock(&mod_mutex);
    // printf("COM GOT MOD MUTEX, SIGNALLING MOD..\n");
    pthread_cond_signal(&mod_cond);
    pthread_mutex_unlock(&mod_mutex);

    pthread_mutex_unlock(&request_mutex);
  }

  pthread_exit(NULL);
}

// returns whether the commentator has requested to comment
int requestComment(int com_number) {
    // printf("COM CALLING can_request COMMENT\n");

    int result = 0;
    // printf("Commentator #%d calling can_request\n", com_number);
    // printf("Commentator #%d acquired lock\n", com_number);
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
        can_request[com_number - 1] = 0;
        break;
      }
      if (requests[i] != 0)
      {
        queue_position++;
      }
    }
    


    notifyModerator();
    // printf("Commentator #%d releasing lock\n", com_number);
    // printf("Commentator #%d released lock\n\n", com_number);
    return result;
}

void *mod_function(){
  int i;
  for(i=0; i < Q; i++) {
    sem_wait(&precedence_sem);
    printf("Moderator asked Question %d\n", i+1);
    pthread_mutex_lock(&request_mutex);
    // printf("MODERATOR GOT REQ MUTEX TO LET COM can_request COMMENT/NO COMMENT\n");
    signal_coms();
    pthread_mutex_unlock(&request_mutex);
    // printf("MODERATOR WANTS MOD MUTEX TO WAIT FOR REQUESTS\n");
    pthread_mutex_lock(&mod_mutex);
    // printf("MODERATOR GOT MOD MUTEX TO WAIT, WAITING..\n");
    do
    {
      pthread_cond_wait(&mod_cond, &mod_mutex);
    } while (in_breaking_news);

    print_requests_queue();
    
    pthread_mutex_unlock(&mod_mutex); // change unlock place (if needed)

    int i;
    for (i = 0; i < N; i++)
    {
      pthread_mutex_lock(&request_mutex);
      int current_com = requests[i];
      if (i == N - 1)
      { // last speaker
        clear_requests(); // todo: maybe place in critical section
      }
      
      if (current_com == 0) { // this commentator does not wish to speak, skip to the next.
        pthread_mutex_unlock(&request_mutex);
        continue;
      }
      can_speak = current_com;
      if (i == N - 1)
      { // last speaker
        recreate_sem(); // todo: maybe place in critical section
      }
      pthread_cond_broadcast(&com_cond);
      pthread_mutex_unlock(&request_mutex);
      // printf("MODERATOR WANTS MOD MUTEX TO WAIT FOR SPEAKER\n");
      pthread_mutex_lock(&mod_mutex);
      // printf("MODERATOR GOT MOD MUTEX TO WAIT, WAITING..\n");
      do {
        pthread_cond_wait(&mod_cond, &mod_mutex);
      } while (in_breaking_news);

      can_speak = -1;
      // printf("MODERATOR WOKE UP, COMS FINISHED SPEAKING\n\n");
      pthread_mutex_unlock(&mod_mutex); // change unlock place (if needed)
    }
    printf("\n"); /// todo: remove?
  }

  pthread_exit(NULL);
}

void signal_coms() {
  int i;
  for (i = 0; i < N; i++)
  {
    can_request[i] = 1;
  }
  pthread_cond_broadcast(&com_cond);
}

void notifyModerator() {
    if (requests[N - 1] == -1)
    {
      return;
    }
    // printf("COM WANTS MOD MUTEX TO SIGNAL MOD: FINISHED REQUESTING\n");
    pthread_mutex_lock(&mod_mutex);
    // printf("COM GOT MOD MUTEX, SIGNALLING MOD..\n");
    pthread_cond_signal(&mod_cond);
    pthread_mutex_unlock(&mod_mutex);
}

void *news_function() {
  while (debate_in_progress)
  {
    pthread_mutex_lock(&news_mutex);
    while (!in_breaking_news)
    {
      pthread_cond_wait(&news_cond, &news_mutex);
    }
    if (!debate_in_progress) // for when the program is over
    {
      pthread_mutex_unlock(&news_mutex);
      break;
    }
    printf("BREAKING NEWS STARTED..\n");
    pthread_sleep(5);
    printf("BREAKING NEWS FINISHED.\n");
    in_breaking_news = 0;
    pthread_cond_signal(&news_cond);
    pthread_mutex_unlock(&news_mutex);
  }
  pthread_exit(NULL);
}

void createThreads(void *(com_func)(void* vargp), void *(mod_func)(void* vargp), void *(news_func)(void* vargp)){
  int i;
  for (i = 0; i < N; i++)
  {
    // printf("CREATING COM THREAD %d\n", i);
    int *current = (int *) malloc(sizeof(int));
    *current = i + 1;
    pthread_create(&commentators[i], NULL, com_func, (void *) current);
  }
  // create moderator threads
  pthread_create(&moderator, NULL, mod_func, NULL); 
  pthread_create(&breaking_news, NULL, news_func, NULL); 
}

void initialize_conds() {
    pthread_cond_init(&mod_cond, NULL);
    pthread_cond_init(&com_cond, NULL);
    pthread_cond_init(&news_cond, NULL);
}

int willDoAction(double p){
  // printf("calling willDoAction with p=%f\n", p);
  int r = rand() % 101;
  return r < (p * 100 + 1);
}

void recreate_sem() {
  // re-initialize precedence sem to 1
  sem_destroy(&precedence_sem);
  sem_init(&precedence_sem, 0, 0); // todo: fix value?
}

void clear_requests() {
  // clear requests array
  int i;
  for (int i = 0; i < N; i++)
  {
    // printf("CLEARING can_request %d\n", i);
    requests[i] = -1;
    can_request[i] = 0;
  }
}

void print_requests_queue() {
      printf("[");
    for (int i = 0; i < N; i++)
    {
      printf("%d ", requests[i]);
    }
    printf("]\n");
}

 /****************************************************************************** 
  pthread_sleep takes an integer number of seconds to pause the current thread 
  original by Yingwu Zhu
  updated by Muhammed Nufail Farooqi
  *****************************************************************************/
int pthread_sleep (int seconds)
{
   pthread_mutex_t mutex;
   pthread_cond_t conditionvar;
   struct timespec timetoexpire;
   if(pthread_mutex_init(&mutex,NULL))
    {
      printf("FAILED TO INIT SLEEP MUTEX\n");
      return -1;
    }
   if(pthread_cond_init(&conditionvar,NULL))
    {
      printf("FAILED TO INIT SLEEP COND\n");
      return -1;
    }
   struct timeval tp;
   //When to expire is an absolute time, so get the current time and add //it to our delay time
   gettimeofday(&tp, NULL);
   timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;

   pthread_mutex_lock (&mutex);
   int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
   pthread_mutex_unlock (&mutex);
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&conditionvar);

   //Upon successful completion, a value of zero shall be returned
  //  if (res != 0)
  //  {
  //     printf("SLEEP FAILED WITH CODE: %d\n", res);
  //  }
   
   return res;

}