#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "log.h"

char *unit_name = NULL;
int unit_line = -1, unit_column = -1;

extern void log_close()
{
    free(unit_name);
}

extern void log_set_unit(const char *name)
{
    int len = strlen(name);
    if (unit_name != NULL) {
        free(unit_name);
    }
    unit_name = (char*)malloc(len + 1);
    memcpy(unit_name, name, len + 1);
}

extern void log_set_pos(int line, int column)
{
    unit_line = line;
    unit_column = column;
}

static void log_print_ex(FILE *stream, const char *prefix, const char *msg)
{
    if (unit_name != NULL) {
        fprintf(stream, "%s:", unit_name);

        if (unit_line != -1) {
            fprintf(stream, "%d:%d:", unit_line, unit_column);
        }
        fprintf(stream, " ");
    }
    fprintf(stream, "%s%s\n", prefix, msg);
}

extern void log_print(const char *msg)
{
    log_print_ex(stderr, "", msg);
}

extern void log_error(const char *msg)
{
    log_print_ex(stderr, "error: ", msg);
}

extern void log_warning(const char *msg)
{
    log_print_ex(stderr, "warning: ", msg);
}