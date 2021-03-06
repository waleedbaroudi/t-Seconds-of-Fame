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
#define USEC_CONVERSION_RATE 1000000

pthread_t moderator;
pthread_t *commentators;
pthread_t breaking_news;

int *requests; // requests queue(requests[i]: commentator i can_request to asnwer. -1: no comment)
int N = 0;
int Q = 0;
int T = 0;
double P = 0;
double B = 0;

struct timeval starting_time;

pthread_cond_t com_cond;
pthread_cond_t mod_cond;
pthread_cond_t news_cond;

pthread_mutex_t com_mutex;
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
int pthread_sleep(double seconds);
int timed_wait (pthread_cond_t *cond, pthread_mutex_t *mutex, double seconds);
char *formatted_current_time();
double real_random_time(int max);

  // // NOTE: This code should be run using the makefile as follows:
  // //                   make
  // //           Or, alternatively:
  // //                   gcc -o [output name] [file name].c -lpthread
  
  // // Command signiture: ./<bin name> -n [number of threads] -p [question probability] -q [number of questions] -t [max speaking time] -b [probability of brekaing news]
  // // Sample Command: ./<bin name> -n 4 -p 0.5 -q 4 -t 5 -b 0.2

int main(int argc, char *argv[]){
  srand(time(0));

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

  pthread_mutex_init(&com_mutex, NULL);
  pthread_mutex_init(&mod_mutex, NULL);
  pthread_mutex_init(&news_mutex, NULL);

  initialize_conds();
  clear_requests();
  gettimeofday(&starting_time, NULL);
  createThreads(com_function, mod_function, news_function); 

  while (1)
  {
    pthread_sleep(BREAKING_NEWS_CHECK);
    pthread_mutex_lock(&news_mutex);
    if (!debate_in_progress){
      pthread_mutex_unlock(&news_mutex);
      break;
    }
    
    if (willDoAction(B))
    {
      in_breaking_news = 1;

      pthread_mutex_lock(&com_mutex);
      pthread_cond_broadcast(&com_cond); // stop the current speaker
      pthread_mutex_unlock(&com_mutex);

      pthread_cond_signal(&news_cond);
      while (in_breaking_news)
      {
        pthread_cond_wait(&news_cond, &news_mutex);
      }

      pthread_mutex_lock(&mod_mutex);
      pthread_cond_signal(&mod_cond); // notify mod: end of breaking news
      pthread_mutex_unlock(&mod_mutex);
    }
    pthread_mutex_unlock(&news_mutex);
  }
  // notify news thread: end of debate
  pthread_mutex_lock(&news_mutex);
  pthread_cond_signal(&news_cond);
  pthread_mutex_unlock(&news_mutex);
  
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
    pthread_mutex_lock(&com_mutex);
    coms_entered++;
    if (coms_entered == N)
    { // allow the moderator in after the last commentator enters
      coms_entered = 0;
      sem_post(&precedence_sem);
    }
    do
    {
      pthread_cond_wait(&com_cond, &com_mutex);
    } while (!can_request[self - 1]);

    if(!requestComment(self)) {
      printf("%s Commentator %d did not request to speak, continue..\n", formatted_current_time(), self);
      
      pthread_mutex_unlock(&com_mutex);
      continue;
    }
    while (can_speak != self)
    {
     pthread_cond_wait(&com_cond, &com_mutex);
    }

    double time = real_random_time(T);
    printf("%s ", formatted_current_time());
    printf("Commentator #%d's turn to speak for %.3f seconds.\n", self, time);
    int speaking_state = timed_wait(&com_cond, &com_mutex, time);
    if(speaking_state == 110) { // SPEAK (SLEEP)
      printf("%s Commentator #%d finished speaking.\n", formatted_current_time(), self);
    } else if(speaking_state == 0) {
      printf("%s Commentator #%d was cut short due to breaking news.\n", formatted_current_time(), self);
    } else {
      printf("[FATAL] Thread sleep error, exitting..\n");
      exit(1);
    }    

    pthread_mutex_lock(&mod_mutex);
    pthread_cond_signal(&mod_cond);
    pthread_mutex_unlock(&mod_mutex);

    pthread_mutex_unlock(&com_mutex);
  }

  pthread_exit(NULL);
}

// returns whether the commentator has requested to comment
int requestComment(int com_number) {

    int result = 0;
    int queue_position = 0;
    int i;
    for (i = 0; i < N; i++)
    {
      if (requests[i] == -1)
      { // found empty slot
        if (willDoAction(P)) {
          requests[i] = com_number;
          result = 1;
          printf("%s Commentator #%d generates an answer, position in queue: %d\n", formatted_current_time(), com_number, queue_position);
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
    return result;
}

void *mod_function(){
  int i;
  for(i=0; i < Q; i++) {
    sem_wait(&precedence_sem);
    printf("%s Moderator asked Question %d\n", formatted_current_time(), i+1);
    pthread_mutex_lock(&com_mutex);
    signal_coms();
    pthread_mutex_unlock(&com_mutex);
    pthread_mutex_lock(&mod_mutex);
    do
    {
      pthread_cond_wait(&mod_cond, &mod_mutex);
    } while (in_breaking_news);

    pthread_mutex_unlock(&mod_mutex); // change unlock place (if needed)

    int i;
    for (i = 0; i < N; i++)
    {
      pthread_mutex_lock(&com_mutex);
      int current_com = requests[i];
      if (i == N - 1)
      { // last speaker
        clear_requests();
      }
      
      if (current_com == 0) { // this commentator does not wish to speak, skip to the next.
        pthread_mutex_unlock(&com_mutex);
        continue;
      }
      can_speak = current_com;
      if (i == N - 1)
      { // last speaker
        recreate_sem();
      }
      pthread_cond_broadcast(&com_cond);
      pthread_mutex_unlock(&com_mutex);
      pthread_mutex_lock(&mod_mutex);
      do {
        pthread_cond_wait(&mod_cond, &mod_mutex);
      } while (in_breaking_news);
      can_speak = -1;
      pthread_mutex_unlock(&mod_mutex); // change unlock place (if needed)
    }
    printf("\n");
  }
  pthread_mutex_lock(&news_mutex);
  debate_in_progress = 0;
  pthread_mutex_unlock(&news_mutex);

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
    pthread_mutex_lock(&mod_mutex);
    pthread_cond_signal(&mod_cond);
    pthread_mutex_unlock(&mod_mutex);
}

void *news_function() {
  while (debate_in_progress)
  {
    pthread_mutex_lock(&news_mutex);
    while (!in_breaking_news && debate_in_progress)
    {
      pthread_cond_wait(&news_cond, &news_mutex);
    }
    if (!debate_in_progress) // for when the program is over
    {
      pthread_mutex_unlock(&news_mutex);
      break;
    }
    printf("%s Breaking news!\n", formatted_current_time());
    pthread_sleep(BREAKING_NEWS_PERIOD);
    printf("%s Breaking news ends.\n", formatted_current_time());
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
  int r = rand() % 100;
  return (r <= (p * 100)) && (p != 0);
}

void recreate_sem() {
  // re-initialize precedence sem to 1
  sem_destroy(&precedence_sem);
  sem_init(&precedence_sem, 0, 0);
}

void clear_requests() {
  // clear requests array
  int i;
  for (int i = 0; i < N; i++)
  {
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
int pthread_sleep(double seconds){
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    if(pthread_mutex_init(&mutex,NULL)){
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL)){
        return -1;
    }

    struct timeval tp;
    struct timespec timetoexpire;
    // When to expire is an absolute time, so get the current time and add
    // it to our delay time
    gettimeofday(&tp, NULL);
    long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
    timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
    timetoexpire.tv_nsec = new_nsec % (long)1e9;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    //Upon successful completion, a value of zero shall be returned
    return res;
}


int timed_wait (pthread_cond_t *cond, pthread_mutex_t *mutex,double seconds)
{
  struct timeval tp;
  struct timespec timetoexpire;
  gettimeofday(&tp, NULL);
  long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
  timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
  timetoexpire.tv_nsec = new_nsec % (long)1e9;
  int res =  pthread_cond_timedwait(cond, mutex, &timetoexpire);
  return res;
}

char *formatted_current_time() {
  struct timeval current;
  gettimeofday(&current, NULL);
  int current_time = (current.tv_sec - starting_time.tv_sec) * USEC_CONVERSION_RATE + current.tv_usec - starting_time.tv_usec;
  int current_time_millis = (current_time / 1000) % 1000;
  int seconds = (current_time / USEC_CONVERSION_RATE) % 60;
  int minutes = (current_time / USEC_CONVERSION_RATE) / 60;

  char formattted_time[16];

  sprintf(formattted_time,
          "[%02d:%02d.%03d]",
          minutes,
          seconds,
          current_time_millis);
  return strcat(formattted_time, "");
}

double real_random_time(int max) {
  int rnd = rand();
  int i_random = rnd % 1001;

  double d_random = (double) (i_random / 1000.0) * (max - 1);

  return d_random + 1;
}