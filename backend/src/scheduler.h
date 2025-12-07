#ifndef SCHEDULER_H
#define SCHEDULER_H

void* scheduler_main(void* arg);
void write_log(const char* fmt, ...);
void write_json_files(void);

#endif