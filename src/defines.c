#define _GNU_SOURCE
#include <stdio.h>

#ifndef WITH_MPV
int cmd_dummy(const char *name,
        const char *param1, const char *param2,
        char **pOutput_message){
    char *msg;
    asprintf(&msg,
            "Without MPV support compiled. "
            "Called command: '%s', Param1: '%s', Param2: '%s'."
            , name, param1, param2);
    printf("%s\n", msg);
    *pOutput_message = msg;

    return 0;
}
#endif
