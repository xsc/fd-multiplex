# fd-multiplex

Multiplexing over C file descriptors. [just an exercise]

## Concept

Each packet transmitted over a multiplexed file descriptor consists of a 4-byte length
prefix, followed by one byte indicating the channel ID the data is destined for. So,
for example, the byte sequence `01 23 45 67` for channel 13 is put on the wire as:

```
00 00 00 05 0d 01 23 45 67
```

The problem addressed by this library is buffering of currently unused channels, as well
as concurrency. One might have different threads that want to process data from different
channels (but the same file descriptor), and they have to make sure that a read operation
from one of the threads does not result in data loss if the incoming data stems from the
wrong channel.

Data is stored in dynamically resizing buffers (only previously "activated" channels are
observed), and access is synchronized using a single mutex.

## Sender

You can wrap any file descriptor in a multiplexer, using `multiplex_new`, before sending
data to a given channel (0-255) using `multiplex_send`:

```c
#include <multiplex.h>
...
Multiplex * m = multiplex_new(sockfd);
multiplex_send(m, channelId, buffer, length);
...
```

## Receiver

TODO

## License

&copy; 2013 Yannick Scherer

This code is released under the MIT License.
