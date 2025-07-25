#ifndef _IO_H_
#define _IO_H_

#ifdef __cplusplus
extern "C" {
#endif

int gr_openstream(const char *path);
void gr_writestream(char *string, ...);
void gr_flushstream(int discard);
void gr_closestream(void);
int gr_startlistener(void);
int gr_inqgrplotport(void);
int gr_setgrplotport(int);

#ifdef __cplusplus
}
#endif

#endif
