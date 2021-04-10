/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2020 Live Networks, Inc.  All rights reserved.
// A class for streaming data from a circular buffer.
// Implementation

#include "FramedMemorySource.hh"
#include "GroupsockHelper.hh"
#include "presentationTime.hh"

#include <pthread.h>

#define MILLIS_25 25000
#define MILLIS_10 10000

unsigned char NALU_HEADER[] = { 0x00, 0x00, 0x00, 0x01 };

extern int debug;

////////// FramedMemorySource //////////

FramedMemorySource*
FramedMemorySource::createNew(UsageEnvironment& env,
                                        cb_output_buffer *cbBuffer,
                                        unsigned preferredFrameSize,
                                        unsigned playTimePerFrame) {
    if (cbBuffer == NULL) return NULL;

    return new FramedMemorySource(env, cbBuffer, preferredFrameSize, playTimePerFrame);
}

FramedMemorySource::FramedMemorySource(UsageEnvironment& env,
                                                        cb_output_buffer *cbBuffer,
                                                        unsigned preferredFrameSize,
                                                        unsigned playTimePerFrame)
    : FramedSource(env), fBuffer(cbBuffer), fCurIndex(0),
      fPreferredFrameSize(preferredFrameSize), fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
      fLimitNumBytesToStream(False), fNumBytesToStream(0) {
}

FramedMemorySource::~FramedMemorySource() {}

void FramedMemorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
}

void FramedMemorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
}

// The second argument is the circular buffer
int FramedMemorySource::cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
{
    int ret;

    if (str2 + n > fBuffer->buffer + fBuffer->size) {
        ret = memcmp(str1, str2, fBuffer->buffer + fBuffer->size - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (fBuffer->buffer + fBuffer->size - str2), fBuffer->buffer, n - (fBuffer->buffer + fBuffer->size - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

void FramedMemorySource::doGetNextFrame() {
    if (fLimitNumBytesToStream && fNumBytesToStream == 0) {
        handleClosure();
        return;
    }

    // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
    fFrameSize = fMaxSize;
    if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fFrameSize) {
        fFrameSize = (unsigned)fNumBytesToStream;
    }
    if (fPreferredFrameSize > 0 && fPreferredFrameSize < fFrameSize) {
        fFrameSize = fPreferredFrameSize;
    }

    if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() start - fMaxSize %d - fLimitNumBytesToStream %d - fPreferredFrameSize %d\n", fMaxSize, fLimitNumBytesToStream, fPreferredFrameSize);

    pthread_mutex_lock(&(fBuffer->mutex));
    while (1) {
        if (fBuffer->frame_read_index == fBuffer->frame_write_index) {
            pthread_mutex_unlock(&(fBuffer->mutex));
            if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() read_index == write_index\n");
            usleep(MILLIS_25);
            pthread_mutex_lock(&(fBuffer->mutex));
            continue;
        } else if (fBuffer->output_frame[fBuffer->frame_read_index].ptr == NULL) {
            pthread_mutex_unlock(&(fBuffer->mutex));
            if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() error - NULL ptr\n");
            usleep(MILLIS_10);
            pthread_mutex_lock(&(fBuffer->mutex));
            continue;
        } else if (cb_memcmp(NALU_HEADER, fBuffer->output_frame[fBuffer->frame_read_index].ptr, sizeof(NALU_HEADER)) != 0) {
            fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;
            pthread_mutex_unlock(&(fBuffer->mutex));
            if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() error - wrong frame header\n");
            usleep(MILLIS_10);
            pthread_mutex_lock(&(fBuffer->mutex));
            continue;
        } else {
            break;
        }
    }

    unsigned char *ptr;
    unsigned int size;
    ptr = fBuffer->output_frame[fBuffer->frame_read_index].ptr;
    size = fBuffer->output_frame[fBuffer->frame_read_index].size;
    // Remove nalu header before sending the frame to FramedSource
    ptr += 4;
    if (ptr >= fBuffer->buffer + fBuffer->size) ptr -= fBuffer->size;
    size -= 4;

    if (size <= fFrameSize) {
        // The size of the frame is smaller than the available buffer
        fFrameSize = size;
        if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() whole frame - fFrameSize %d - counter %d - fMaxSize %d - fLimitNumBytesToStream %d\n", fFrameSize, fBuffer->output_frame[fBuffer->frame_read_index].counter, fMaxSize, fLimitNumBytesToStream);
        if (ptr + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, ptr, fBuffer->buffer + fBuffer->size - ptr);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - ptr), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - ptr));
//            if (debug & 4) fwrite("\0\0\0\1", 1, 4, stderr);
//            if (debug & 4) fwrite(ptr, 1, fBuffer->buffer + fBuffer->size - ptr, stderr);
//            if (debug & 4) fwrite(fBuffer->buffer, 1, fFrameSize - (fBuffer->buffer + fBuffer->size - ptr), stderr);
        } else {
            memmove(fTo, ptr, fFrameSize);
//            if (debug & 4) fwrite("\0\0\0\1", 1, 4, stderr);
//            if (debug & 4) fwrite(ptr, 1, fFrameSize, stderr);
        }
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;

        pthread_mutex_unlock(&(fBuffer->mutex));
        fNumTruncatedBytes = 0;
        if (debug & 2) fprintf(stderr, "RTSP doGetNextFrame() whole frame completed\n");
    } else {
        // The size of the frame is greater than the available buffer
        fprintf(stderr, "RTSP doGetNextFrame() error - the size of the frame is greater than the available buffer %d/%d\n", fFrameSize, fMaxSize);
    }

#ifndef PRES_TIME_CLOCK
    // Set the 'presentation time':
    if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) {
        if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
            // This is the first frame, so use the current time:
            gettimeofday(&fPresentationTime, NULL);
        } else {
            // Increment by the play time of the previous data:
            unsigned uSeconds = fPresentationTime.tv_usec + fLastPlayTime;
            fPresentationTime.tv_sec += uSeconds/1000000;
            fPresentationTime.tv_usec = uSeconds%1000000;
        }

        // Remember the play time of this data:
        fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
        fDurationInMicroseconds = fLastPlayTime;
    } else {
        // We don't know a specific play time duration for this data,
        // so just record the current time as being the 'presentation time':
        gettimeofday(&fPresentationTime, NULL);
    }
#else
    // Use system clock to set presentation time
    gettimeofday(&fPresentationTime, NULL);
#endif

    // Inform the downstream object that it has data:
    FramedSource::afterGetting(this);
}
