#pragma once


#define MSG_PREFIX "[webui] "
#define VERSION "0.2.1"

#define FREE(x) free(x); (x) = NULL;

#ifdef WITH_MPV
#define CMD(NAME) #NAME, (void *)cmd_##NAME
#define MEDIA_CMD(NAME) #NAME, (void *)cmd_media_##NAME
#else
int cmd_dummy(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message);
#define CMD(NAME) #NAME, (void *)cmd_dummy
#define MEDIA_CMD(NAME) #NAME, (void *)cmd_dummy
#endif
