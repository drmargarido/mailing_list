#include <sys/time.h>
#define MAX_TIMERS 50

typedef struct Timer {
  size_t id;
  long double start_millis;
} Timer;

Timer timers[MAX_TIMERS];
int used_timers = 0;

long double current_time_millis() {
  struct timeval time;
  gettimeofday(&time, NULL);

  long double seconds_in_millis = (long double) time.tv_sec * 1000.0f;
  long double nano_in_millis = (long double) time.tv_usec / 1000.0f;

  return seconds_in_millis + nano_in_millis;
}

int get_timer_index(int id){
  for(int i=0; i < used_timers; i++){
    if(timers[i].id == id){
      return i;
    }
  }
  return -1;
}

void start_timer(int id){
  int index = get_timer_index(id);
  if(index == -1){
    // TODO: Make it work well under multithreaded workload
    index = used_timers;
    timers[index].id = id;
    used_timers++;
  }
  timers[index].start_millis = current_time_millis();
}

long double end_timer(int id){
  int index = get_timer_index(id);
  if(index == -1) {
    printf("Timer id is not valid!\n");
    return 0;
  }
  return current_time_millis() - timers[index].start_millis;
}


