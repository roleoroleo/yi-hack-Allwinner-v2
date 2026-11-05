/*
 * Copyright (c) 2026 roleo.
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
#include <utility>

unsigned char NALU_HEADER[] = { 0x00, 0x00, 0x00, 0x01 };

extern int debug;

// Return True if the NAL unit is a decoder sync point
static Boolean isSyncFrame(int hNumber, unsigned char const* frame, unsigned size) {
    if (size < 5) return False;
    unsigned char nalHdr = frame[4];
    if (hNumber == 264) {
        unsigned nut = nalHdr & 0x1F;
        // 5 = IDR slice, 7 = SPS, 8 = PPS
        return (nut == 5 || nut == 7 || nut == 8) ? True : False;
    } else if (hNumber == 265) {
        unsigned nut = (nalHdr >> 1) & 0x3F;
        // 16..23 = IRAP (BLA/IDR/CRA), 32 = VPS, 33 = SPS, 34 = PPS
        return ((nut >= 16 && nut <= 23) || nut == 32 || nut == 33 || nut == 34) ? True : False;
    }
    return True;
}

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
      fLimitNumBytesToStream(False), fNumBytesToStream(0), fHaveStartedReading(False),
      fHaveLastCounter(False), fLastCounter(0), fHaveAnchor(false), fAnchorFt(0) {

    if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - fPlayTimePerFrame %u\n", current_timestamp(), fPlayTimePerFrame);
}

VideoFramedMemorySource::~VideoFramedMemorySource() {}

void VideoFramedMemorySource::seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream) {
}

void VideoFramedMemorySource::seekToByteRelative(int64_t offset, u_int64_t numBytesToStream) {
}

void VideoFramedMemorySource::doStopGettingFrames() {
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
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
        // Do NOT block the (single-threaded) event loop
        pthread_mutex_lock(&(fQBuffer->mutex));
        unsigned qSize = fQBuffer->frame_queue.size();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        if (qSize < 5) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(2000,
                                 doGetNextFrameTask, this);
            return;
        }
        fHaveStartedReading = True;
        // Force a resync to a keyframe/parameter-set before delivering the first frame
        fHaveLastCounter = False;
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

    output_frame f;
    while (!frameFound) {
        pthread_mutex_lock(&(fQBuffer->mutex));
        if (fQBuffer->frame_queue.size() == 0) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() queue is empty\n", current_timestamp());
            // Nothing to deliver yet: poll the queue again instead of handing an empty
            // frame to the sink (for AAC each empty frame becomes an RTP packet with no
            // payload, and the empty-frame round trips cost CPU)
            nextTask() = envir().taskScheduler().scheduleDelayedTask(2000,
                                 doGetNextFrameTask, this);
            return;
        } else if (fQBuffer->frame_queue.front().frame.size() < 5) {
            // Too small, drop it
            fQBuffer->frame_queue.pop();
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - bad frame (too small)\n", current_timestamp());
        } else if (memcmp(NALU_HEADER, fQBuffer->frame_queue.front().frame.data(), sizeof(NALU_HEADER)) != 0) {
            // Maybe the buffer is too small, align read index with write index
            if (fQBuffer->frame_queue.size() > 0) {
                fQBuffer->frame_queue.pop();
            }
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - wrong frame header\n", current_timestamp());
        } else {
            // Keyframe-aware resync: after a discontinuity (frames dropped on
            // queue overflow, or a fresh (re)start) resume only at a decoder
            // sync point
            u_int32_t counter = (u_int32_t) fQBuffer->frame_queue.front().counter;
            Boolean discontinuity = (!fHaveLastCounter) ||
                                    (counter != ((fLastCounter + 1) & 0xFFFF));
            if (discontinuity &&
                !isSyncFrame(fHNumber,
                             fQBuffer->frame_queue.front().frame.data(),
                             fQBuffer->frame_queue.front().frame.size())) {
                fQBuffer->frame_queue.pop();
                pthread_mutex_unlock(&(fQBuffer->mutex));
                if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() resync - skipping orphan frame\n", current_timestamp());
            } else {
                // Deliver this frame: take ownership and release the lock before the copy below
                f = std::move(fQBuffer->frame_queue.front());
                fQBuffer->frame_queue.pop();
                pthread_mutex_unlock(&(fQBuffer->mutex));
                frameFound = true;
            }
        }
    }

    // Frame found (owned by 'f', lock already released)
    int size = (int) f.frame.size();
    uint32_t frame_time = f.time;
    u_int32_t frame_counter = (u_int32_t) f.counter;
    unsigned char *ptr = f.frame.data() + 4;
    size -= 4;
    unsigned char nal = ptr[0];

    if ((unsigned) size <= fFrameSize) {
        // The frame fits in the available buffer
        fFrameSize = size;
        if (debug & 4) fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - fMaxSize %d - counter %d - time %u\n",
                current_timestamp(), fFrameSize, fMaxSize, frame_counter, frame_time);
        std::memcpy(fTo, ptr, size);
        fNumTruncatedBytes = 0;
        // Frame delivered: remember its sequence counter
        fLastCounter = frame_counter;
        fHaveLastCounter = True;
    } else {
        // The frame is larger than the available buffer: drop it
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        fprintf(stderr, "%lld: VideoFramedMemorySource - doGetNextFrame() error - frame larger than buffer %d/%d - frame lost\n", current_timestamp(), size, fMaxSize);
    }

    if (!fUseTimeForPres) {
        frametime_to_presentation(frame_time, &fPresentationTime, &fHaveAnchor, &fAnchorWall, &fAnchorFt);
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
//    FramedSource::afterGetting(this);
    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                                         (TaskFunc*)FramedSource::afterGetting, this);
}
