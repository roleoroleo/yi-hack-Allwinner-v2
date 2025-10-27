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

#include "AudioFramedMemorySource.hh"
#include "GroupsockHelper.hh"
#include "rRTSPServer.h"

#include <cstring>
#include <queue>
#include <vector>

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
      fNumChannels(numChannels), fUseTimeForPres(useTimeForPres), fHaveStartedReading(False) {

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
    Boolean isFirstReading = !fHaveStartedReading;
    if (!fHaveStartedReading) {
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() 1st start\n", current_timestamp());
        pthread_mutex_lock(&(fQBuffer->mutex));
        while (fQBuffer->frame_queue.size() > 1) fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fHaveStartedReading = True;
    }

    fFrameSize = fMaxSize;

    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() start - fMaxSize %d\n", current_timestamp(), fMaxSize);

    pthread_mutex_lock(&(fQBuffer->mutex));
    if (fQBuffer->frame_queue.size() == 0) {
        pthread_mutex_unlock(&(fQBuffer->mutex));
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() read_index = write_index\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        // Trick to avoid segfault with StreamReplicator
        if (isFirstReading) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
        } else {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(fuSecsPerFrame/4,
                    (TaskFunc*) AudioFramedMemorySource::doGetNextFrameTask, this);
        }
        return;
    } else if (fQBuffer->frame_queue.front().frame.size() == 0) {
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - NULL ptr\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        // Trick to avoid segfault with StreamReplicator
        if (isFirstReading) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
        } else {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(fuSecsPerFrame/4,
                    (TaskFunc*) AudioFramedMemorySource::doGetNextFrameTask, this);
        }
        return;
    } else if (check_sync_word(fQBuffer->frame_queue.front().frame.data()) != 1) {
        pthread_mutex_unlock(&(fQBuffer->mutex));
        while (fQBuffer->frame_queue.size() > 0) {
            fQBuffer->frame_queue.pop();
        }
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - wrong frame header\n", current_timestamp());
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
        // Trick to avoid segfault with StreamReplicator
        if (isFirstReading) {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
        } else {
            nextTask() = envir().taskScheduler().scheduleDelayedTask(fuSecsPerFrame/4,
                    (TaskFunc*) AudioFramedMemorySource::doGetNextFrameTask, this);
        }
        return;
    }

    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() size of queue is %d\n", current_timestamp(), fQBuffer->frame_queue.size());

    // Frame found, send it
    unsigned char *ptr;
    int size = fQBuffer->frame_queue.front().frame.size();
    uint32_t frame_time = fQBuffer->frame_queue.front().time;
    ptr = fQBuffer->frame_queue.front().frame.data();
    ptr += HEADER_SIZE;
    size -= HEADER_SIZE;

    if ((unsigned) size <= fMaxSize) {
        // The size of the frame is smaller than the available buffer
        fFrameSize = size;
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - fMaxSize %d - counter %d - time %u\n",
                current_timestamp(), fFrameSize, fMaxSize, fQBuffer->frame_queue.front().counter, frame_time);
        std::memcpy(fTo, ptr, size);
        fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fNumTruncatedBytes = 0;
    } else {
        // The size of the frame is greater than the available buffer
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - the size of the frame is greater than the available buffer %d/%d\n", current_timestamp(), fFrameSize, fMaxSize);
        fFrameSize = 0;
        fQBuffer->frame_queue.pop();
        pthread_mutex_unlock(&(fQBuffer->mutex));
        fNumTruncatedBytes = 0;
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() frame lost\n", current_timestamp());
    }

    if (!fUseTimeForPres) {
        fPresentationTime.tv_usec = (frame_time % 1000) * 1000;
        fPresentationTime.tv_sec = frame_time / 1000;
    } else {
        // Use system clock to set presentation time
        gettimeofday(&fPresentationTime, NULL);
    }
    fDurationInMicroseconds = fuSecsPerFrame;

    // Inform the reader that he has data:
    FramedSource::afterGetting(this);
}
