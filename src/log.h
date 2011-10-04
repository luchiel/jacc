#ifndef JACC_LOG_H
#define JACC_LOG_H

extern void log_set_unit(const char *name);
extern void log_set_pos(int line, int column);

extern void log_print(const char *msg);
extern void log_error(const char *msg);
extern void log_warning(const char *msg);

#endif