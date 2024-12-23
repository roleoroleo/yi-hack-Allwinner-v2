/*
 * Copyright (c) 2024 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A class for streaming data from a queue
 */

#include "VideoFramedMemorySource.hh"
#include "GroupsockHelper.hh"
#include "misc.hh"

#include <pthread.h>

unsigned char NALU_HEADER[] = { 0x00, 0x00, 0x00, 0x01 };

extern int debug;

////////// VideoFramedMemorySource //////////

VideoFramedMemorySource*
VideoFramedMemorySource::createNew(UsageEnvironment& env,
                                        int hNumber,
                                        cb_output_buffer *cbBuffer,
                                        unsigned playTimePerFrame) {
    if (cbBuffer == NULL) return NULL;

    return new VideoFramedMemorySource(env, hNumber, cbBuffer, playTimePerFrame);
}

VideoFramedMemorySource::VideoFramedMemorySource(UsageEnvironment& env,
                                                        int hNumber,
                                                        cb_output_buffer *cbBuffer,
                                                        unsigned playTimePerFrame)
    : FramedSource(env), fHNumber(hNumber), fBuffer(cbBuffer), fCurIndex(0),
      fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
      fLimitNumBytesToStream(False), fNumBytesToStream(0), fHaveStartedReading(False) {
}

VideoFramedMemorySource::~VideoFramedMemorySource() {}

void VideoFramedMemorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
}

void VideoFramedMemorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
}

// The second argument is the circular buffer
int VideoFramedMemorySource::cb_memcmp(unsigned char *str1, unsigned char *str2, size_t n)
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

void VideoFramedMemorySource::doStopGettingFrames() {
    fHaveStartedReading = False;
}

void VideoFramedMemorySource::doGetNextFrameTask(void* clientData) {
    VideoFramedMemorySource *source = (VideoFramedMemorySource *) clientData;
    source->doGetNextFrameEx();
}

void VideoFramedMemorySource::doGetNextFrameEx() {
    doGetNextFrame();
}

void VideoFramedMemorySource::doGetNextFrame() {
    if (!fHaveStartedReading) {
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() 1st start\n", current_timestamp());
        pthread_mutex_lock(&(fBuffer->mutex));
        fBuffer->frame_read_index = fBuffer->frame_write_index;
        pthread_mutex_unlock(&(fBuffer->mutex));
        fHaveStartedReading = True;
    }

    if (fLimitNumBytesToStream && fNumBytesToStream == 0) {
        handleClosure();
        return;
    }

    // Try to read as many bytes as will fit in the buffer provided
    fFrameSize = fMaxSize;
    if (fLimitNumBytesToStream && fNumBytesToStream < (u_int64_t)fFrameSize) {
        fFrameSize = (unsigned)fNumBytesToStream;
    }

    if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() start - fMaxSize %d - fLimitNumBytesToStream %d\n", current_timestamp(), fMaxSize, fLimitNumBytesToStream);

    pthread_mutex_lock(&(fBuffer->mutex));
    if (fBuffer->frame_read_index == fBuffer->frame_write_index) {
        pthread_mutex_unlock(&(fBuffer->mutex));
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() read_index == write_index\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        nextTask() = envir().taskScheduler().scheduleDelayedTask(fPlayTimePerFrame/4,
                (TaskFunc*) VideoFramedMemorySource::doGetNextFrameTask, this);
        return;
    } else if (fBuffer->output_frame[fBuffer->frame_read_index].ptr == NULL) {
        pthread_mutex_unlock(&(fBuffer->mutex));
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - NULL ptr\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        nextTask() = envir().taskScheduler().scheduleDelayedTask(fPlayTimePerFrame/4,
                (TaskFunc*) VideoFramedMemorySource::doGetNextFrameTask, this);
        return;
    } else if (cb_memcmp(NALU_HEADER, fBuffer->output_frame[fBuffer->frame_read_index].ptr, sizeof(NALU_HEADER)) != 0) {
        // Maybe the buffer is too small, align read index with write index
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;
        pthread_mutex_unlock(&(fBuffer->mutex));
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - wrong frame header\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        fBuffer->frame_read_index = fBuffer->frame_write_index;
        nextTask() = envir().taskScheduler().scheduleDelayedTask(fPlayTimePerFrame/4,
                (TaskFunc*) VideoFramedMemorySource::doGetNextFrameTask, this);
        return;
    }

    // Frame found, send it
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
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - counter %d - fMaxSize %d - fLimitNumBytesToStream %d\n", current_timestamp(), fFrameSize, fBuffer->output_frame[fBuffer->frame_read_index].counter, fMaxSize, fLimitNumBytesToStream);
        if (ptr + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, ptr, fBuffer->buffer + fBuffer->size - ptr);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - ptr), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - ptr));
        } else {
            memmove(fTo, ptr, fFrameSize);
        }
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;
        pthread_mutex_unlock(&(fBuffer->mutex));
        fNumTruncatedBytes = 0;
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() whole frame completed\n", current_timestamp());
    } else {
        // The size of the frame is greater than the available buffer
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - the size of the frame is greater than the available buffer %d/%d\n", current_timestamp(), fFrameSize, fMaxSize);
        fFrameSize = 0;
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;
        pthread_mutex_unlock(&(fBuffer->mutex));
        fNumTruncatedBytes = 0;
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() frame lost\n", current_timestamp());
    }

    // Set the 'presentation time':
    // Use system clock to set presentation time
    gettimeofday(&fPresentationTime, NULL);
    fDurationInMicroseconds = fPlayTimePerFrame;

    // If it's a VPS/SPS/PPS set duration = 0
    u_int8_t nal_unit_type;
    if (fHNumber == 264) {
        nal_unit_type = ptr[0]&0x1F;
        if ((nal_unit_type == 7) || (nal_unit_type == 8)) fDurationInMicroseconds = 0;
    } else if (fHNumber == 265) {
        nal_unit_type = (ptr[0]&0x7E)>>1;
        if ((nal_unit_type == 32) || (nal_unit_type == 33) || (nal_unit_type == 34)) fDurationInMicroseconds = 0;
    }

     // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
            (TaskFunc*)FramedSource::afterGetting, this);
}
