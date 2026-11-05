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

#include "AudioFramedMemorySource.hh"
#include "GroupsockHelper.hh"
#include "rRTSPServer.h"

#include <cstring>
#include <queue>
#include <vector>
#include <utility>

#include <pthread.h>

#define MILLIS_25 25000
#define MILLIS_10 10000

#define HEADER_SIZE 7

extern int debug;

////////// FramedMemorySource //////////

static unsigned const samplingFrequencyTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

AudioFramedMemorySource*
AudioFramedMemorySource::createNew(UsageEnvironment& env,
                                        output_queue *qBuffer,
                                        unsigned samplingFrequency,
                                        unsigned char numChannels,
                                        Boolean useTimeForPres) {
    if (qBuffer == NULL) return NULL;

    return new AudioFramedMemorySource(env, qBuffer, samplingFrequency, numChannels, useTimeForPres);
}

AudioFramedMemorySource::AudioFramedMemorySource(UsageEnvironment& env,
                                                        output_queue *qBuffer,
                                                        unsigned samplingFrequency,
                                                        unsigned char numChannels,
                                                        Boolean useTimeForPres)
    : FramedSource(env), fQBuffer(qBuffer), fProfile(1), fSamplingFrequency(samplingFrequency),
      fNumChannels(numChannels), fUseTimeForPres(useTimeForPres), fHaveStartedReading(False),
      fHaveAnchor(false), fAnchorFt(0) {

    u_int8_t samplingFrequencyIndex;
    int i;
    for (i = 0; i < 16; i++) {
        if (samplingFrequency == samplingFrequencyTable[i]) {
            samplingFrequencyIndex = i;
            break;
        }
    }
    if (i == 16) samplingFrequencyIndex = 8;

    u_int8_t channelConfiguration = numChannels;
    if (channelConfiguration == 8) channelConfiguration--;

    fuSecsPerFrame = (1024/*samples-per-frame*/*1000000) / samplingFrequency/*samples-per-second*/;
    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - fuSecsPerFrame %u\n", current_timestamp(), fuSecsPerFrame);

    // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
    unsigned char audioSpecificConfig[2];
    u_int8_t const audioObjectType = fProfile + 1;
    audioSpecificConfig[0] = (audioObjectType<<3) | (samplingFrequencyIndex>>1);
    audioSpecificConfig[1] = (samplingFrequencyIndex<<7) | (channelConfiguration<<3);
    sprintf(fConfigStr, "%02X%02X", audioSpecificConfig[0], audioSpecificConfig[1]);
    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - fConfigStr %s\n", current_timestamp(), fConfigStr);
}

AudioFramedMemorySource::~AudioFramedMemorySource() {}

int AudioFramedMemorySource::check_sync_word(unsigned char *str)
{
    int ret = 0, n;

    n = ((*str & 0xFF) == 0xFF);
    n += ((*(str + 1) & 0xF0) == 0xF0);
    if (n == 2) ret = 1;

    return ret;
}

void AudioFramedMemorySource::doStopGettingFrames() {
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    fHaveStartedReading = False;
}

void AudioFramedMemorySource::doGetNextFrameTask(void* clientData) {
    AudioFramedMemorySource *source = (AudioFramedMemorySource *) clientData;
    source->doGetNextFrameEx();
}

void AudioFramedMemorySource::doGetNextFrameEx() {
    doGetNextFrame();
}

void AudioFramedMemorySource::doGetNextFrame() {
    Boolean frameFound = false;

    if (!fHaveStartedReading) {
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() 1st start\n", current_timestamp());
        // Do NOT block the (single-threaded) event loop
        pthread_mutex_lock(&(fQBuffer->mutex));
        unsigned qSize = fQBuffer->frame_queue.size();
        if (qSize < 5) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            nextTask() = envir().taskScheduler().scheduleDelayedTask(2000,
                                 doGetNextFrameTask, this);
            return;
        }
        // Keep only the most recent frames to reduce initial latency.
        while (fQBuffer->frame_queue.size() > 5) fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fHaveStartedReading = True;
    }

    fFrameSize = fMaxSize;

    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() start - fMaxSize %d\n", current_timestamp(), fMaxSize);

    output_frame f;
    while (!frameFound) {
        pthread_mutex_lock(&(fQBuffer->mutex));
        if (fQBuffer->frame_queue.size() == 0) {
            pthread_mutex_unlock(&(fQBuffer->mutex));
            if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() read_index = write_index\n", current_timestamp());
            // Nothing to deliver yet: poll the queue again instead of handing an empty
            // frame to the sink (for AAC each empty frame becomes an RTP packet with no
            // payload, and the empty-frame round trips cost CPU)
            nextTask() = envir().taskScheduler().scheduleDelayedTask(2000,
                                 doGetNextFrameTask, this);
            return;
        } else if (fQBuffer->frame_queue.front().frame.size() < (unsigned)(HEADER_SIZE + 1)) {
            // Too small, drop it
            fQBuffer->frame_queue.pop();
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - bad frame (too small)\n", current_timestamp());
        } else if (check_sync_word(fQBuffer->frame_queue.front().frame.data()) != 1) {
            fQBuffer->frame_queue.pop();
            pthread_mutex_unlock(&(fQBuffer->mutex));
            fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - wrong frame header\n", current_timestamp());
        } else {
            // Deliver this frame: take ownership and release the lock before the copy below
            f = std::move(fQBuffer->frame_queue.front());
            fQBuffer->frame_queue.pop();
            pthread_mutex_unlock(&(fQBuffer->mutex));
            frameFound = true;
        }
    }

    // Frame found (owned by 'f', lock already released)
    int size = (int) f.frame.size();
    uint32_t frame_time = f.time;
    unsigned char *ptr = f.frame.data() + HEADER_SIZE;
    size -= HEADER_SIZE;

    if ((unsigned) size <= fMaxSize) {
        // The frame fits in the available buffer
        fFrameSize = size;
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - fMaxSize %d - counter %d - time %u\n",
                current_timestamp(), fFrameSize, fMaxSize, f.counter, frame_time);
        std::memcpy(fTo, ptr, size);
        fNumTruncatedBytes = 0;
    } else {
        // The frame is larger than the available buffer: drop it
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - frame larger than buffer %d/%d - frame lost\n", current_timestamp(), size, fMaxSize);
    }

    if (!fUseTimeForPres) {
        frametime_to_presentation(frame_time, &fPresentationTime, &fHaveAnchor, &fAnchorWall, &fAnchorFt);
    } else {
        // Use system clock to set presentation time
        gettimeofday(&fPresentationTime, NULL);
    }
    fDurationInMicroseconds = fuSecsPerFrame;

    // Inform the reader that he has data:
//    FramedSource::afterGetting(this);
    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                                         (TaskFunc*)FramedSource::afterGetting, this);
}
