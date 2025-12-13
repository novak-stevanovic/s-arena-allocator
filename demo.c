#include <stdio.h>
#include <assert.h>
#include <string.h>

#define _SARENA_IMPLEMENTATION_
#include "sarena.h"

struct M
{
    char name[50];
    char desc[1000];
    size_t id;
};

int main(int argc, char *argv[])
{
    sarena* a = sarena_create(1000000);
    assert(a != NULL);

    size_t i;
    void* it_alloc;
    for(i = 0; i < 10000; i++)
    {
        it_alloc = sarena_malloc(a, sizeof(struct M));
        assert(it_alloc != NULL);
    }

    // printf("REWIND\n");
    // sarena_rewind(a);

    printf("RESET\n");
    sarena_reset(a);

    for(i = 0; i < 10000000; i++)
    {
        it_alloc = sarena_malloc(a, sizeof(struct M));
        assert(it_alloc != NULL);
    }

    sarena_destroy(a);

    printf("Done\n");
    return 0;
}
