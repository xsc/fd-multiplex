/* The MIT License (MIT)
 * 
 * Copyright (c) 2013 Yannick Scherer
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MULTIPLEX_H
#define MULTIPLEX_H

#define CHANNEL_INITIAL_BUFFER_SIZE 256
#define CHANNEL_IGNORED -255
#define CHANNEL_TIMEOUT -77
#define CHANNEL_CLOSED  -1

#ifndef NO_MUTEX
#include <pthread.h>
#endif

typedef struct ChannelBuffer {
    char * data;    // receive buffer
    int offset;     // current read offset
    int length;     // current read length
    int capacity;   // capacity of receive buffer
    int initial;    // minimum capacity
    int newData;    // 0 = no new data since last 'select'
} ChannelBuffer;

typedef struct Multiplex {
    int fd;                                // file descriptor
    struct ChannelBuffer * channels[256];  // O(1) lookup for channels
#ifndef NO_MUTEX
    pthread_mutex_t mutex;                 // for exclusive access
#endif
} Multiplex;

// Multiplexing Operations (these are not thread-safe!)
Multiplex * multiplex_new(int fd);
void multiplex_enable(Multiplex * c, unsigned char channelId, int initialBufferSize);
void multiplex_enable_range(Multiplex * c, unsigned char minChannelId, unsigned char maxChannelId, int initialBufferSize);
void multiplex_disable(Multiplex * c, unsigned char channelId);

// -- send data; returns result of 'write'
int multiplex_send(Multiplex * c, unsigned char channelId, char const * src, int length);
int multiplex_send_string(Multiplex * c, unsigned char channelId, char const * str);

// -- select channel; returns the channel ID (>= 0) or a status code (< 0)
int multiplex_select(Multiplex * c, int timeoutMs);

// -- receive data with timeout
int multiplex_receive(Multiplex * c, int timeoutMs, unsigned char channelId, char * dst, int offset, int length);

// -- get length of channel buffer
int multiplex_length(Multiplex * c, unsigned char channelId);

// -- get length of data received in last select
int multiplex_last_received(Multiplex * c, unsigned char channelId);

// -- get channel buffer
char const * multiplex_get(Multiplex * c, unsigned char channelId);

// -- create copy of (part of) channel buffer
char * multiplex_strdup(Multiplex * c, unsigned char channelId);

// -- write to/read from channel buffer
void multiplex_write(Multiplex * c, unsigned char channelId, char * data, int offset, int length);
int multiplex_read(Multiplex * c, unsigned char channelId, char * dst, int offset, int length);
int multiplex_copy(Multiplex * c, unsigned char channelId, char * dst, int offset, int length);

// -- clear buffer
void multiplex_clear(Multiplex * c, unsigned char channelId);

// -- remove select status, keep data
void multiplex_ignore(Multiplex * c, unsigned char channelId);

#endif
