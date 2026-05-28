#ifndef SCHEDULE_H
#define SCHEDULE_H

void schedule_init(void);

void schedule_set_start_time(int hour);

void schedule_set_end_time(int hour);

int schedule_get_start_time();

int schedule_get_end_time();

#endif // SCHEDULE_H