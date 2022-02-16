#ifndef __TSPG_BADWORDS_H__
#define __TSPG_BADWORDS_H__

void BADWORDS_Init();
void BADWORDS_Shutdown();
void BADWORDS_AttemptReload();
bool BADWORDS_ShouldFilter(const char *text);

#endif