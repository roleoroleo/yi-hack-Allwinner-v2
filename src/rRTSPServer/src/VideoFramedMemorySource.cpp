/*
 * Copyright (c) 2025 roleo.
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
#include "rRTSPServer.h"

#include <pthread.h>

#include <cstring>
#include <queue>
#include <vector>

unsigned char NALU_HEADER[] = { 0x00, 0x00, 0x00, 0x01 };

extern int debug;

////////// VideoFramedMemorySource //////////

VideoFramedMemorySource*
VideoFramedMemorySource::createNew(UsageEnvironment& env,
                                        int hNumber,
                                        output_queue *qBuffer,
                                        Boolean useTimeForPres,
                                        unsigned playTimePerFrame) {
    if (qBuffer == NULL) return NULL;

    return new VideoFramedMemorySource(env, hNumber, qBuffer, useTimeForPres, playTimePerFrame);
}

VideoFramedMemorySource::VideoFramedMemorySource(UsageEnvironment& env,
                                                        int hNumber,
                                                        output_queue *qBuffer,
                                                        Boolean useTimeForPres,
                                                        unsigned playTimePerFrame)
    : FramedSource(env), fHNumber(hNumber), fQBuffer(qBuffer),
      fCurIndex(0), fUseTimeForPres(useTimeForPres), fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0),
      fLimitNumBytesToStream(False), fNumBytesToStream(0), fHaveStartedReading(False) {

    if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - fPlayTimePerFrame %u\n", current_timestamp(), fPlayTimePerFrame);
}

VideoFramedMemorySource::~VideoFramedMemorySource() {}

void VideoFramedMemorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
}

void VideoFramedMemorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
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
    Boolean frameFound = false;

    if (!fHaveStartedReading) {
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() 1st start\n", current_timestamp());
        pthread_mutex_lock(&(fQBuffer->mutex));
        // Yes, I know that I should not block the event loop
        while (fQBuffer->frame_queue.size() < 5) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            usleep(1000);
            pthread_mutex_lock(&(fQBuffer->mutex));
        }
        while (fQBuffer->frame_queue.size() > 5) fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
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

    while (!frameFound) {
        pthread_mutex_lock(&(fQBuffer->mutex));
        if (fQBuffer->frame_queue.size() == 0) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() queue is empty\n", current_timestamp());
            usleep(1000);
        } else if (fQBuffer->frame_queue.front().frame.size() == 0) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - NULL ptr\n", current_timestamp());
            usleep(1000);
        } else if (memcmp(NALU_HEADER, fQBuffer->frame_queue.front().frame.data(), sizeof(NALU_HEADER)) != 0) {
            // Maybe the buffer is too small, align read index with write index
            if (fQBuffer->frame_queue.size() > 0) {
                fQBuffer->frame_queue.pop();
            }
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - wrong frame header\n", current_timestamp());
        } else {
            frameFound = true;
        }
    }

    if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() size of queue is %d\n", current_timestamp(), fQBuffer->frame_queue.size());

    // Frame found, send it
    unsigned char *ptr;
    unsigned char nal;
    int size = fQBuffer->frame_queue.front().frame.size();
    uint32_t frame_time = fQBuffer->frame_queue.front().time;
    ptr = fQBuffer->frame_queue.front().frame.data();
    // Remove nalu header before sending the frame to FramedSource
    ptr += 4 * sizeof(unsigned char);
    size -= 4 * sizeof(unsigned char);
    nal = ptr[0];

    if ((unsigned) size <= fFrameSize) {
        // The size of the frame is smaller than the available buffer
        fFrameSize = size;
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - fMaxSize %d - counter %d - time %u\n",
                current_timestamp(), fFrameSize, fMaxSize, fQBuffer->frame_queue.front().counter, frame_time);
        std::memcpy(fTo, ptr, size);
        fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fNumTruncatedBytes = 0;
    } else {
        // The size of the frame is greater than the available buffer
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - the size of the frame is greater than the available buffer %d/%d\n", current_timestamp(), fFrameSize, fMaxSize);
        fFrameSize = 0;
        fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fNumTruncatedBytes = 0;
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() frame lost\n", current_timestamp());
    }

    if (!fUseTimeForPres) {
        fPresentationTime.tv_usec = (frame_time % 1000) * 1000;
        fPresentationTime.tv_sec = frame_time / 1000;
    } else {
        // Set the 'presentation time':
        // Use system clock to set presentation time
        gettimeofday(&fPresentationTime, NULL);
    }
    fDurationInMicroseconds = fPlayTimePerFrame;

    // If it's a VPS/SPS/PPS set duration = 0
    u_int8_t nal_unit_type;
    if (fHNumber == 264) {
        nal_unit_type = nal&0x1F;
        if ((nal_unit_type == 7) || (nal_unit_type == 8)) fDurationInMicroseconds = 0;
    } else if (fHNumber == 265) {
        nal_unit_type = (nal&0x7E)>>1;
        if ((nal_unit_type == 32) || (nal_unit_type == 33) || (nal_unit_type == 34)) fDurationInMicroseconds = 0;
    }

    // Inform the reader that he has data:
    FramedSource::afterGetting(this);
}
