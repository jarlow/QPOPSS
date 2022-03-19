#ifndef UTILS_H
#define UTILS_H

#include <sys/time.h>
#include <stdlib.h>

struct timeval global_timer_start,global_timer_stop;

uint64_t rdtsc(){
    unsigned int lo,hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

void startTime(){
    gettimeofday(&global_timer_start,NULL);
}
void stopTime(){
    gettimeofday(&global_timer_stop,NULL);
}

unsigned long getTimeMs(){
    return (global_timer_stop.tv_sec - global_timer_start.tv_sec)*1000 + (global_timer_stop.tv_usec - global_timer_start.tv_usec)/1000;
}

#endif