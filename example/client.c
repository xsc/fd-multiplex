#include <multiplex.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int selected = 0;
    char * buffer;
    Multiplex * m = multiplex_new(0);
    for (selected = 0; selected < 256; ++selected)
        multiplex_activate_channel(m, selected, 256);

    while (selected != CHANNEL_CLOSED) {
        selected = multiplex_select(m, 2000);
        if (selected >= 0) {
            buffer = multiplex_copy(m, selected, 0, multiplex_length(m, selected) + 1);
            printf("[channel:%03d] %s\n", selected, buffer);
            multiplex_clear(m, selected);
            free(buffer);
        }
    }

    return 0;
}
