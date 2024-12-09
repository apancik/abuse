#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "common.h"

void set_filename_prefix(char const *prefix);
char *get_filename_prefix();
void set_save_filename_prefix(char const *prefix);
char *get_save_filename_prefix();

// Drop-in replacement for fopen() that handles file prefixes
FILE *prefix_fopen(const char *filename, const char *mode);

// Drop-in replacement for open() that handles file prefixes
// Default mode is 0666 (standard file permissions) if not specified
int prefix_open(const char *filename, int flags, mode_t mode = 0666);

#endif // FILE_UTILS_H