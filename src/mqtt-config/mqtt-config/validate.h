#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define PARAM_SIZE    128
#define PARAM_OPTIONS 9
#define PARAM_NUM     58

int validate_param(char *file, char *key, char *value);
int extract_param(char *param, char *file, char *key, int index);
