#ifndef _SK8860_ENV_LIB_H_
#define _SK8860_ENV_LIB_H_

#if defined(__cplusplus)
extern "C"
{
#endif
int fw_printenv (unsigned int hand);

int fw_getenv (unsigned int hand, char *name, char *buf, int *length);

int fw_setenv (unsigned int hand, char *tag, char *buf);

int fw_saveenv(unsigned int hand);

unsigned int env_init(void);

extern void env_close(unsigned int hand);

void env_die(const char *s);
#endif

#if defined(__cplusplus)
}
#endif
