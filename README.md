# fd-multiplex

Multiplexing over C file descriptors. [just an exercise]

## Sender

You can wrap any file descriptor in a multiplexer, using `multiplex_new`, before sending
data to a given channel (0-255) using `multiplex_send`:

```c
#include <multiplex.h>
...
Multiplex * m = multiplex_new(sockfd);
multiplex_send(m, 0x23, buffer, length);
...
```

## Receiver

TODO

## License

&copy; 2013 Yannick Scherer

This code is released under the MIT License.
