#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "s_arena.h"

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
    SArena* a = s_arena_create(1064 * 2 - 1, &err);
    PERROR(err);

    struct M *m1 = s_arena_alloc(a, sizeof(struct M), &err);
    PERROR(err);
    struct M *m2 = s_arena_alloc(a, sizeof(struct M), &err);
    PERROR(err);

    strcpy(m1->name, "NOVAK");
    strcpy(m1->desc, "111111111111111111");
    strcpy(m2->name, "EMILIJA");
    strcpy(m2->desc, "222222222222222222");

    s_arena_rewind(a);

    struct M *m3 = s_arena_alloc(a, sizeof(struct M), &err);
    PERROR(err);

    s_arena_destroy(a);

    printf("Done\n");
    return 0;
}
