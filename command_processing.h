#ifndef COMMAND_PROCESSING_H
#define COMMAND_PROCESSING_H

#include "defines.h"

char (*get_raw_log(void))[LOG_LINE_LENGTH];
int parse_json_command(char *command_string);
void *command_processing_thread(void *arg);

#endif // COMMAND_PROCESSING_H
