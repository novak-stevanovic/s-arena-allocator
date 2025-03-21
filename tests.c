#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "sarena.h"

#define PERROR(err) if(err != SA_SUCCESS) printf("%d\n",err);

struct M
{
    char name[50];
    char desc[1000];
    size_t id;
};

int main(int argc, char *argv[])
{
    sa_err err;
    SArena* a = sarena_create(1000000, &err);
    PERROR(err);

    size_t i;
    for(i = 0; i < 10000; i++)
    {
        sarena_malloc(a, sizeof(struct M), &err);
        assert(err == SA_SUCCESS);
    }

    printf("REWIND\n");
    sarena_rewind(a);

    for(i = 0; i < 1000000; i++)
    {
        sarena_malloc(a, sizeof(struct M), &err);
        assert(err == SA_SUCCESS);
    }

    sarena_destroy(a);

    printf("Done\n");
    return 0;
}
