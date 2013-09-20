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
// First, we create a "MultiplexContext" that will contain receiver buffers
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

typedef struct ChannelBuffer {
    char * data;    // receive buffer
    int offset;     // current read offset
    int length;     // current read length
    int capacity;   // capacity of receive buffer
    char newData;   // 0 = no new data since last 'select'
} ChannelBuffer;

typedef struct MultiplexContext {
    struct ChannelBuffer * channels[256];  // O(1) lookup for channels
} MultiplexContext;

// Multiplexing Operations (these are not thread-safe!)
MultiplexContext * multiplex_create_context();
void multiplex_activate_channel(MultiplexContext * c, unsigned char channelId, int initialBufferSize);

// -- send data; returns result of 'write'
int multiplex_send(int pipe, int channelId, char const * src, int length);

// -- select channel; returns the channel ID (>= 0) or a status code (< 0)
int multiplex_select(MultiplexContext * c, int pipe, int timeoutMs);

// -- receive data with timeout
int multiplex_receive(MultiplexContext * c, int pipe, int timeoutMs, int channelId, char * dst, int length);

// -- get length of channel buffer
int multiplex_length(MultiplexContext * c, int channelId);

// -- get length of data received in last select
int multiplex_last_received(MultiplexContext * c, int channelId);

// -- get channel buffer
char const * multiplex_get(MultiplexContext * c, int channelId);

// -- create copy of (part of) channel buffer
char * multiplex_copy(MultiplexContext * c, int channelId, int offset, int length);

// -- read from channel buffer
int multiplex_read(MultiplexContext * c, int channelId, char * dst, int length);

// -- clear buffer
void multiplex_clear(MultiplexContext * c, int channelId);

#endif
