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

// MULTIPLEXING OVER A SINGLE PIPE
// -------------------------------
// By prefixing each incoming packet with its length and requiring the
// first byte of the payload to be a channel ID, as well as reyling on
// only one sender (which might itself collect data from multiple 
// senders), i.e. on fragments arriving in order, we can implement
// a multiplexing scheme.
//
// First, we create a "Multiplex" that will contain receiver buffers
// for each channel we want to demultiplex. We create said buffers by
// 'activating' the channel.
//
// After that we can either run a 'select' (returns an active channel with new data)
// or a 'receive' (retrieves the data from a given channel), supplying a timeout
// (at least to select) which prevents infinite blocking.

#ifndef MULTIPLEX_H
#define MULTIPLEX_H

#define CHANNEL_INITIAL_BUFFER_SIZE 256
#define CHANNEL_IGNORED -255
#define CHANNEL_TIMEOUT -77
#define CHANNEL_CLOSED  -1

typedef struct Multiplex {
    int fd;
    struct ChannelBuffer * channels[256];  // O(1) lookup for channels
} Multiplex;

// Multiplexing Operations (these are not thread-safe!)
Multiplex * multiplex_new(int fd);
void multiplex_activate_channel(Multiplex * c, unsigned char channelId, int initialBufferSize);

// -- send data; returns result of 'write'
int multiplex_send(Multiplex * c, int channelId, char const * src, int length);

// -- select channel; returns the channel ID (>= 0) or a status code (< 0)
int multiplex_select(Multiplex * c, int timeoutMs);

// -- receive data with timeout
int multiplex_receive(Multiplex * c, int timeoutMs, int channelId, char * dst, int offset, int length);

// -- get length of channel buffer
int multiplex_length(Multiplex * c, int channelId);

// -- get length of data received in last select
int multiplex_last_received(Multiplex * c, int channelId);

// -- get channel buffer
char const * multiplex_get(Multiplex * c, int channelId);

// -- create copy of (part of) channel buffer
char * multiplex_copy(Multiplex * c, int channelId, int offset, int length);
char * multiplex_copy_all(Multiplex * c, int channelId);

// -- write to/read from channel buffer
void multiplex_write(Multiplex * c, unsigned char channelId, char * data, int offset, int length);
int multiplex_read(Multiplex * c, int channelId, char * dst, int offset, int length);

// -- clear buffer
void multiplex_clear(Multiplex * c, int channelId);

#endif
