#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "shared.h"

void init_scheduler();
void start_scheduler();
void fifo();
void priorities();
void *aging(void *args);

#endif