#include <multiplex.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char buffer[256];
    int ch = 0;
    Multiplex * m = multiplex_new(1);

    srand(time(0));
    while (1) {
        ch = rand()%256;
        sprintf(buffer, "Hello on Channel %d.", ch);
        multiplex_send(m, ch, buffer, strlen(buffer));
        sleep(rand()%3 + 1);
    }

    return 0;
}
