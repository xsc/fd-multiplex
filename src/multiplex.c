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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "multiplex.h"

// -- Buffer Structure
typedef struct ChannelBuffer {
    char * data;    // receive buffer
    int offset;     // current read offset
    int length;     // current read length
    int capacity;   // capacity of receive buffer
    int initial;    // minimum capacity
    int newData;    // 0 = no new data since last 'select'
} ChannelBuffer;

// -- MUTEX
static int multiplex_lock(Multiplex * c) {
    if (c == 0) return 1;
#ifndef NO_MUTEX
    return pthread_mutex_lock(&(c->mutex));
#else
    return 0;
#endif
}

static int multiplex_unlock(Multiplex * c) {
    if (c == 0) return 1;
#ifndef NO_MUTEX
    return pthread_mutex_unlock(&(c->mutex));
#else
    return 0;
#endif
}

static int multiplex_lock_channel(Multiplex * c, unsigned char channelId) {
    // lock the Multiplex, but return 0 only if the given channel exists
    int r = multiplex_lock(c);
    if (r != 0) return r;
    if (c->channels[channelId] == 0) {
        multiplex_unlock(c);
        return 1;
    }
    return 0;
}

// -- CREATE
Multiplex * multiplex_new(int fd) {
    Multiplex * m = (Multiplex *)calloc(1, sizeof(Multiplex));
    if (m != 0) {
        if (pthread_mutex_init(&(m->mutex), 0) != 0) {
            free(m);
            return 0;
        }
        m->fd = fd;
    }
    return m;
}

// -- ACTIVATE CHANNEL
static void _enable_channel(Multiplex * c, unsigned char channelId, int initialBufferSize) {
    if (c != 0 && channelId >= 0 && channelId <= 255 && c->channels[channelId] == 0) {
        ChannelBuffer * buf = (ChannelBuffer *)calloc(1, sizeof(ChannelBuffer));
        if (buf != 0) {
            int size = initialBufferSize > 0 ? initialBufferSize : CHANNEL_INITIAL_BUFFER_SIZE;
            char * stream = (char *)calloc(size, sizeof(char));
            if (stream != 0) {
                buf->data = stream;
                buf->offset = 0;
                buf->length = 0;
                buf->initial = size;
                buf->capacity = size;
                c->channels[channelId] = buf;
            }
        }
    }
}

void multiplex_enable(Multiplex * c, unsigned char channelId, int initialBufferSize) {
    if (multiplex_lock(c) == 0) {
        _enable_channel(c, channelId, initialBufferSize);
        multiplex_unlock(c);
    }
}

void multiplex_enable_range(Multiplex * c, unsigned char minChannel, unsigned char maxChannel, int initialBufferSize) {
    if (multiplex_lock(c) == 0) {
        int i;
        for (i = minChannel; i <= maxChannel; ++i) 
            _enable_channel(c, i, initialBufferSize);
        multiplex_unlock(c);
    }
}

void multiplex_disable(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId) == 0) {
        ChannelBuffer * buf = c->channels[channelId];
        if (buf->data != 0) free(buf->data);
        free(buf);
        c->channels[channelId] = 0;
        multiplex_unlock(c);
    }
}

// -- REALLOCATE BUFFER
static int _reallocate_channel(Multiplex * c, unsigned char channelId, int additionalDataSize) {
    if (c == 0 || c->channels[channelId] == 0) return 0;
    else {
        ChannelBuffer * buf = c->channels[channelId];
        char * tmpBuf = buf->data;
        int newLen = buf->offset + buf->length + additionalDataSize;
        int allocateLen = buf->capacity;

        // Case 1: buffer is too empty (less than 25%)
        if (allocateLen > newLen * 4) {
            allocateLen = buf->initial;
        } else {
            // Case 2: buffer is big enough
            if (allocateLen >= newLen) return 1;

            // Case 3: move data within buffer (set offset to 0)
            if (buf->capacity >= buf->length + additionalDataSize) {
                if (buf->offset > 0) {
                    int i = 0; 
                    while (i < buf->length) {
                        buf->data[i] = buf->data[buf->offset + i];
                    }
                    buf->offset = 0;
                }
                return 1;
            }
        }

        // Case 4: extend buffer
        while (allocateLen < newLen) allocateLen *= 2;
        buf->data = (char *)calloc(allocateLen, sizeof(char));
        if (buf->data == 0) { buf->data = tmpBuf; return 0; }
        buf->capacity = allocateLen;
        memcpy(buf->data, tmpBuf + buf->offset, buf->length);
        free(tmpBuf);
        return 1;
    }
}

// ----------------------------------------------------------------------
//
//   MODIFY BUFFER
//
// ----------------------------------------------------------------------
static void _write_channel(Multiplex * c, unsigned char channelId, char * data, int offset, int length) {
    if (_reallocate_channel(c, channelId, length)) {
        ChannelBuffer * buf = c->channels[channelId];
        if (buf != 0) {
            memcpy(buf->data + buf->offset + buf->length, data + offset, length);
            buf->length += length;
            buf->newData = length;
        }
    }
}

void multiplex_write(Multiplex * c, unsigned char channelId, char * data, int offset, int length) {
    if (multiplex_lock_channel(c, channelId) == 0) {
        _write_channel(c, channelId, data, offset, length);
        multiplex_unlock(c);
    }
}

static int _copy_channel(Multiplex * c, unsigned char channelId, char * dst, int offset, int length) {
    ChannelBuffer * buf = c->channels[channelId];
    int copyLen = length;
    if (buf == 0) return CHANNEL_IGNORED;

    //
    if (buf->length < length) copyLen = buf->length;
    memcpy(dst + offset, buf->data + buf->offset, copyLen);
    return copyLen;
}

int multiplex_copy(Multiplex * c, unsigned char channelId, char * dst, int offset, int length) {
    if (multiplex_lock_channel(c, channelId) != 0) return -1;
    else {
        int r = _copy_channel(c, channelId, dst, offset, length);
        multiplex_unlock(c);
        return r;
    }
}

static int _read_channel(Multiplex * c, unsigned char channelId, char * dst, int offset, int length) {
    ChannelBuffer * buf = c->channels[channelId];
    int copyLen = length;
    if (buf == 0) return CHANNEL_IGNORED;

    //
    if (buf->length < length) copyLen = buf->length;
    memcpy(dst + offset, buf->data + buf->offset, copyLen);
    buf->offset += copyLen;
    buf->length -= copyLen;
    buf->newData -= copyLen;
    if (buf->newData < 0) buf->newData = 0;
    return copyLen;
}

int multiplex_read(Multiplex * c, unsigned char channelId, char * dst, int offset, int length) {
    if (multiplex_lock_channel(c, channelId) != 0) return CHANNEL_CLOSED;
    else {
        int r = _read_channel(c, channelId, dst, offset, length);
        multiplex_unlock(c);
        return r;
    }
}

static void _clear_channel(Multiplex * c, unsigned char channelId) {
    ChannelBuffer * buf = c->channels[channelId];
    buf->offset = 0;
    buf->length = 0;
    buf->newData = 0;
}

void multiplex_clear(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId) == 0) {
        _clear_channel(c, channelId);
        multiplex_unlock(c);
    }
}

// ----------------------------------------------------------------------
// 
//   RECEIVE LOGIC
//
// ----------------------------------------------------------------------
static int _fd_read(int fd, int timeoutMs, char * buffer, int length) {
    int position = 0, bytesRead = 0, selectResult = -1;
    struct timeval timeout;
    fd_set fds;

    while (position < length) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        selectResult = select(fd + 1, &fds, (fd_set *)0, (fd_set *)0, &timeout);
        if (selectResult == 0) return CHANNEL_TIMEOUT;

        if (FD_ISSET(fd,&fds)) {
            bytesRead = read(fd, buffer + position, length - position);
            if (bytesRead <= 0) return CHANNEL_CLOSED;
            else position += bytesRead;
        }
    }
    return length;
}

static int _select_channel(Multiplex * c, int timeoutMs) {
    char prefixBuffer[5];
    int bytesRead = 0, dataLength = 0;
    unsigned char channelId = 0;

    //
    if (c == 0) return CHANNEL_CLOSED;

    // Check if data is available somewhere
    {
        int i = 0;
        ChannelBuffer * buf = 0;
        for (; i < 256; ++i) {
            buf = c->channels[i];
            if (buf != 0 && buf->length > 0 && buf->newData != 0) {
                buf->newData = 0;
                return i;
            }
        }
    }

    //
    bytesRead = _fd_read(c->fd, timeoutMs, prefixBuffer, 5); 
    if (bytesRead != 5) return bytesRead;

    //
    dataLength = (prefixBuffer[0] << 24) | (prefixBuffer[1] << 16) | (prefixBuffer[2] << 8) | prefixBuffer[3];
    channelId = (unsigned char)prefixBuffer[4];
    {
        char buffer[dataLength - 1];
        bytesRead = _fd_read(c->fd, timeoutMs, buffer, dataLength - 1);
        if (bytesRead != dataLength - 1) return bytesRead;
        if (c->channels[channelId] == 0) return CHANNEL_IGNORED;
        _write_channel(c, channelId, buffer, 0, dataLength - 1);
    }
    return channelId;
}

int multiplex_select(Multiplex * c, int timeoutMs) {
    if (multiplex_lock(c) == 0) {
        int r = _select_channel(c, timeoutMs);
        multiplex_unlock(c);
        return r;
    }
    return CHANNEL_CLOSED;
}

void multiplex_ignore(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId) == 0) {
        c->channels[channelId]->newData = 0;
        multiplex_unlock(c);
    }
}

static int _receive_channel(Multiplex * c,
                            int timeoutMs,
                            unsigned char channelId,
                            char * dst,
                            int offset,
                            int length) {
    int receiveId = CHANNEL_IGNORED;
    ChannelBuffer * buf = 0;
    if (c == 0) return CHANNEL_CLOSED;
    buf = c->channels[channelId];
    if (buf == 0) return CHANNEL_IGNORED;

    // Check if data is already buffered.
    if (buf->length > 0) {
        if (length <= buf->length) {
            memcpy(dst + offset, buf->data + buf->offset, length);
            buf->offset += length;
            buf->length -= length;
            return length;
        } else {
            int tmpLen = buf->length;
            memcpy(dst + offset, buf->data + buf->offset, tmpLen);
            buf->offset = 0;
            buf->length = 0;
            return tmpLen;
        }
    }

    // Receive on the given Channel
    receiveId = _select_channel(c, timeoutMs);
    if (receiveId < 0) return receiveId;
    if (receiveId != channelId) return CHANNEL_IGNORED;

    // Copy from ChannelBuffer
    return _read_channel(c, channelId, dst, offset, length);
}

int multiplex_receive(Multiplex * c,
                      int timeoutMs,
                      unsigned char channelId,
                      char * dst,
                      int offset,
                      int length) {
    if (multiplex_lock(c) == 0) {
        int r = _receive_channel(c, timeoutMs, channelId, dst, offset, length);
        multiplex_unlock(c);
        return r;
    }
    return CHANNEL_CLOSED;
}

// ----------------------------------------------------------------------
//
//   SEND LOGIC
//
// ----------------------------------------------------------------------
int multiplex_send(Multiplex * c, unsigned char channelId, char const * src, int length) {
    if (src == 0 || multiplex_lock(c) != 0) return -1;
    else {
        int len = length + 1;
        char buffer[5 + length];
        buffer[0] = (char)((len >> 24) & 0xFF);
        buffer[1] = (char)((len >> 16) & 0xFF);
        buffer[2] = (char)((len >> 8) & 0xFF);
        buffer[3] = (char)(len & 0xFF);
        buffer[4] = (char)(channelId & 0xFF);
        memcpy(buffer + 5, src, length);
        len = write(c->fd, buffer, 5 + length);
        multiplex_unlock(c);
        return len;
    }
} 

int multiplex_send_string(Multiplex * c, unsigned char channelId, char const * str) {
    if (str == 0) return -1;
    return multiplex_send(c, channelId, str, strlen(str));
}

// ----------------------------------------------------------------------
//
//   BUFFER INSPECTION
//
// ----------------------------------------------------------------------
int multiplex_length(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId)) return -1;
    else {
        int r = c->channels[channelId]->length;
        multiplex_unlock(c);
        return r;
    }
}

int multiplex_last_received(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId)) return 0;
    else {
        int r = c->channels[channelId]->newData;
        multiplex_unlock(c);
        return r;
    }
}

char const * multiplex_get(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId) != 0) return 0;
    else {
        ChannelBuffer * buf = c->channels[channelId];
        char const * ptr = buf->data + buf->offset;
        multiplex_unlock(c);
        return ptr;
    }
}

char * multiplex_strdup(Multiplex * c, unsigned char channelId) {
    if (multiplex_lock_channel(c, channelId) != 0) return 0;
    else {
        ChannelBuffer * buf = c->channels[channelId];
        char * tmp = (char *)calloc(buf->length + 1, sizeof(char));
        if (tmp != 0) memcpy(tmp, buf->data + buf->offset, buf->length);
        multiplex_unlock(c);
        return tmp;
    }
}
