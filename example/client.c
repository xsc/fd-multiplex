#include <multiplex.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static int c = 0;
static void * receive_on_channel(void * mPtr) {
    Multiplex * m = (Multiplex *)mPtr;
    int selected = 0;
    char * buffer;
    const int cc = ++c;

    while (selected != CHANNEL_CLOSED) {
        selected = multiplex_select(m, 2000);
        if (selected >= 0) {
            buffer = multiplex_strdup(m, selected);
            printf("%d:[channel:%03d] %s\n", cc, selected, buffer);
            multiplex_clear(m, selected);
            free(buffer);
            usleep(rand() % 1000000);
        }
    }

}

int main(int argc, char * argv[]) {
    int threadCount = 1;
    if (argc > 1) threadCount = atoi(argv[1]);
    if (threadCount <= 0) threadCount = 1;

    {
        pthread_t thr[threadCount];
        Multiplex * m = multiplex_new(0);
        void * r = 0;

        multiplex_enable_range(m, 0, 255, 256);
        
        while (threadCount-- > 0)
            pthread_create(&thr[threadCount], 0, &receive_on_channel, m);
        pthread_join(thr[0], &r);
    }

    return 0;
}
