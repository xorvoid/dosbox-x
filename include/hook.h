#ifndef DOSBOX_HOOK_H
#define DOSBOX_HOOK_H

void HOOK_Init(const char *libpath);
int HOOK_Attempt(void);
void HOOK_Notify_Ip(void);

#endif
