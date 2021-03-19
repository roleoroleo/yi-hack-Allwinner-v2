#include "config.h"

int open_conf_file(const char* filename);

void (*fconf_handler)(const char* key, const char* value);

FILE *fp;

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

int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

int init_config(const char* config_filename)
{
    if (open_conf_file(config_filename) != 0)
        return -1;

    return 0;
}

void stop_config()
{
    if (fp != NULL)
        fclose(fp);
}

void config_set_handler(void (*f)(const char* key, const char* value))
{
    if (f != NULL)
        fconf_handler = f;
}

void config_parse()
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
    FILE *fpt;
    char tmpFile[L_tmpnam + 1]; // Add +1 for the null character. 

    lcase(filename);
    ucase(key);

    fp = fopen(filename, "r");
    if(fp == NULL) {
        fprintf(stderr, "Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    tmpnam(tmpFile);
    fpt = fopen(tmpFile, "w");
    if (fpt == NULL) {
        fprintf(stderr, "Can't open file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    while (!feof(fp)) {
        line = fgets(buf, MAX_LINE_LENGTH, fp);
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
    fclose(fp);
    remove(filename);
    cp(filename, tmpFile);
    remove(tmpFile);
}

int open_conf_file(const char* filename)
{
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Can't open file \"%s\": %s\n", filename, strerror(errno));
        return -1;
    }

    return 0;
}
