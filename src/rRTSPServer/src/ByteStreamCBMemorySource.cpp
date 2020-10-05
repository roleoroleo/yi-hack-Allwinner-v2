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

#include "ByteStreamCBMemorySource.hh"
#include "GroupsockHelper.hh"

#include <pthread.h>

extern int debug;

////////// ByteStreamCBMemoryBufferSource //////////

ByteStreamCBMemorySource*
ByteStreamCBMemorySource::createNew(UsageEnvironment& env,
                                        cb_output_buffer *cbBuffer,
                                        unsigned preferredFrameSize,
                                        unsigned playTimePerFrame) {
    if (cbBuffer == NULL) return NULL;

    return new ByteStreamCBMemorySource(env, cbBuffer, preferredFrameSize, playTimePerFrame);
}

ByteStreamCBMemorySource::ByteStreamCBMemorySource(UsageEnvironment& env,
                                                        cb_output_buffer *cbBuffer,
                                                        unsigned preferredFrameSize,
                                                        unsigned playTimePerFrame)
    : FramedSource(env), fBuffer(cbBuffer), fCurIndex(0),
      fPreferredFrameSize(preferredFrameSize), fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
      fLimitNumBytesToStream(False), fNumBytesToStream(0) {
}

ByteStreamCBMemorySource::~ByteStreamCBMemorySource() {}

void ByteStreamCBMemorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
}

void ByteStreamCBMemorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
}

void ByteStreamCBMemorySource::doGetNextFrame() {
    if (fLimitNumBytesToStream && fNumBytesToStream == 0) {
        handleClosure();
        return;
    }

    if (debug) fprintf(stderr, "0 - fFrameSize %d - fMaxSize %d - fLimitNumBytesToStream %d - fPreferredFrameSize %d\n", fFrameSize, fMaxSize, fLimitNumBytesToStream, fPreferredFrameSize);

    // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
    fFrameSize = fMaxSize;
    if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fFrameSize) {
        fFrameSize = (unsigned)fNumBytesToStream;
    }
    if (fPreferredFrameSize > 0 && fPreferredFrameSize < fFrameSize) {
        fFrameSize = fPreferredFrameSize;
    }

    pthread_mutex_lock(&(fBuffer->mutex));
    while (fBuffer->read_index == fBuffer->write_index) {
        pthread_mutex_unlock(&(fBuffer->mutex));
        usleep(25000);
        pthread_mutex_lock(&(fBuffer->mutex));
    }

    if (((fBuffer->write_index + fBuffer->size - fBuffer->read_index) % fBuffer->size) < fFrameSize) {
        fFrameSize = (fBuffer->write_index + fBuffer->size - fBuffer->read_index) % fBuffer->size;
        if (debug) fprintf(stderr, "1 - fFrameSize %d - fMaxSize %d - fLimitNumBytesToStream %d\n", fFrameSize, fMaxSize, fLimitNumBytesToStream);
//        if (debug) fprintf(stderr, "10 - fFrameSize %d - fBuffer->write_index 0x%08x -  fBuffer->size %d - fBuffer->read_index 0x%08x\n", fFrameSize, fBuffer->write_index, fBuffer->size, fBuffer->read_index);
        if (fBuffer->read_index + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, fBuffer->read_index, fBuffer->buffer + fBuffer->size - fBuffer->read_index);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - fBuffer->read_index), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - fBuffer->read_index));
            fBuffer->read_index += fFrameSize - fBuffer->size;
        } else {
            memmove(fTo, fBuffer->read_index, fFrameSize);
            fBuffer->read_index += fFrameSize;
        }
        pthread_mutex_unlock(&(fBuffer->mutex));
        if (debug) fprintf(stderr, "1 - Completed\n");
    } else {
        if (debug) fprintf(stderr, "2 - fFrameSize %d - fMaxSize %d - fLimitNumBytesToStream %d\n", fFrameSize, fMaxSize, fLimitNumBytesToStream);
        if (fBuffer->read_index + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, fBuffer->read_index, fBuffer->buffer + fBuffer->size - fBuffer->read_index);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - fBuffer->read_index), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - fBuffer->read_index));
            fBuffer->read_index += fFrameSize - fBuffer->size;
        } else {
            memmove(fTo, fBuffer->read_index, fFrameSize);
            fBuffer->read_index += fFrameSize;
        }
        pthread_mutex_unlock(&fBuffer->mutex);
        if (debug) fprintf(stderr, "22 - Completed\n");
    }
    if (fBuffer->read_index == fBuffer->buffer + fBuffer->size) {
        fBuffer->read_index = fBuffer->buffer;
    }

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

    // Inform the downstream object that it has data:
    FramedSource::afterGetting(this);
}
