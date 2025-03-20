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
    SArena* a = sarena_create(1064 * 2 - 1, &err);
    PERROR(err);

    struct M *m1 = sarena_alloc(a, sizeof(struct M), &err);
    PERROR(err);
    struct M *m2 = sarena_alloc(a, sizeof(struct M), &err);
    PERROR(err);

    strcpy(m1->name, "NOVAK");
    strcpy(m1->desc, "111111111111111111");
    strcpy(m2->name, "EMILIJA");
    strcpy(m2->desc, "222222222222222222");

    sarena_rewind(a);

    struct M *m3 = sarena_alloc(a, sizeof(struct M), &err);
    PERROR(err);

    sarena_destroy(a);

    printf("Done\n");
    return 0;
}
