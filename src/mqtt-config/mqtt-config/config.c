#include "config.h"

void (*fconf_handler)(const char* key, const char* value);

void config_set_handler(void (*f)(const char* key, const char* value))
{
    if (f != NULL)
        fconf_handler = f;
}

void config_parse(FILE *fp)
{
    char buf[MAX_LINE_LENGTH];
    char key[128];
    char value[128];

    int parsed;

    if (fp == NULL)
        return;

    while (!feof(fp)) {
        fgets(buf, MAX_LINE_LENGTH, fp);
        if (buf[0]!='#') // ignore the comments
        {
            parsed=sscanf(buf, "%[^=] = %s", key, value);
            if (parsed==2 && fconf_handler!=NULL)
                (*fconf_handler)(key, value);
        }
    }
}

FILE *open_conf_file(const char* filename)
{
    FILE *fp;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Can't open file \"%s\": %s\n", filename, strerror(errno));
        return NULL;
    }

    return fp;
}

void close_conf_file(FILE *fp)
{
    if (fp != NULL)
        fclose(fp);
    fp = NULL;
}
