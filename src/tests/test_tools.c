#include "tools.h"

int test_split_options() {
    const char *input_strs[] = {
        "a",
        "",
        "=", ",", ";",
        ",,,,,",
        ",;,;,",
        "=====",
        ",=,=",
        "=,=,",
        "==,==,",
        "=,,=,,",
        "==,,==,,",
        "a",
        "a=b",
        "a==b",
        "a=b=",
        "a=b==",
        "a==b=",
        "a=b=c",
        "a=b;a=b",
        "a=b;c=d",
        "a=b,c=d",
        "xx=",
        "xx=;",
        "xx=a",
        "xx=aa",
        "xy==more",
        "xx;yy=;zz==;uu===",
        "x=yes,a=no",
        "prefix-x=yes,prefix-a=no",
        "prefix-prefix-x",
        "\\",
        "\\\\",
        "k\\=e\\;y=value;",
        "key=v\\=a\\;l\\,u\\=le",
        "k=v;\\;b=c\\;\\\\",
        NULL
    };

    const char *prefix_strs[] = {
        "",
        "prefix-",
        NULL
    };

    const char **prefix = (const char **)prefix_strs;
    while (*prefix) {

        printf("\nPrefix: '%s'\n", *prefix);
        const char **input = (const char **)input_strs;
        while( *input){
            printf("\nInput: '%s'\n", *input);
            option_t *opts = split_options(*prefix, *input);
            print_options(opts);
            free_options(opts);

            ++input;
        }
        ++prefix;
    }

    return 0;
}

int main(){
    test_split_options();

    return 0;
}
