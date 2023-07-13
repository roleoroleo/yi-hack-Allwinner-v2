#include "config.h"

void (*fconf_handler)(const char* key, const char* value);

void ucase(char *string) {
    // Convert to upper case
    char *s = string;
    while (*s) {
        *s = toupper((unsigned char) *s);
        s++;
    }
}

void lcase(char *string) {
    // Convert to lower case
    char *s = string;
    while (*s) {
        *s = tolower((unsigned char) *s);
        s++;
    }
}

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

void config_replace(char *filename, char *key, char *value)
{
    char buf[MAX_LINE_LENGTH];
    char oldkey[128];
    char oldvalue[128];
    char *line;
    int parsed;
    FILE *fps, *fpt;
    char tmpFile[L_tmpnam + 1]; // Add +1 for the null character. 

    lcase(filename);
    ucase(key);

    fps = fopen(filename, "r");
    if (fps == NULL) {
        printf("Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    tmpnam(tmpFile);
    fpt = fopen(tmpFile, "w");
    if (fpt == NULL) {
        printf("Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    while (!feof(fps)) {
        line = fgets(buf, MAX_LINE_LENGTH, fps);
        if (line == NULL) break;
        if (buf[0] != '#') {
            parsed = sscanf(buf, "%[^=] = %s", oldkey, oldvalue);
            if((parsed == 2) && (strcasecmp(key, oldkey) == 0)) {
                sprintf(buf, "%s=%s\n", key, value);
            }
        }
        fputs(buf, fpt);
    }

    fclose(fpt);
    fclose(fps);

    // Rename file (rename function is a cross-device link)
    fps = fopen(filename, "w");
    if (fps == NULL) {
        printf("Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    fpt = fopen(tmpFile, "r");
    if (fpt == NULL) {
        printf("Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    while (!feof(fpt)) {
        line = fgets(buf, MAX_LINE_LENGTH, fpt);
        if (line == NULL) break;
        fputs(buf, fps);
    }

    fclose(fpt);
    fclose(fps);

    remove(tmpFile);
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
