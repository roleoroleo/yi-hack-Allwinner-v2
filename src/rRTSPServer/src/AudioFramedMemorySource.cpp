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

#include "AudioFramedMemorySource.hh"
#include "GroupsockHelper.hh"
#include "misc.hh"

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
                                        cb_output_buffer *cbBuffer,
                                        unsigned samplingFrequency,
                                        unsigned numChannels) {
    if (cbBuffer == NULL) return NULL;

    return new AudioFramedMemorySource(env, cbBuffer, samplingFrequency, numChannels);
}

AudioFramedMemorySource::AudioFramedMemorySource(UsageEnvironment& env,
                                                        cb_output_buffer *cbBuffer,
                                                        unsigned samplingFrequency,
                                                        unsigned numChannels)
    : FramedSource(env), fBuffer(cbBuffer), fProfile(1), fSamplingFrequency(samplingFrequency),
      fNumChannels(numChannels), fHaveStartedReading(False) {

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

int AudioFramedMemorySource::cb_check_sync_word(unsigned char *str)
{
    int ret = 0, n;

    if (str + 2 > fBuffer->buffer + fBuffer->size) {
        n = ((*str & 0xFF) == 0xFF);
        n += ((*(fBuffer->buffer) & 0xF0) == 0xF0);
    } else {
        n = ((*str & 0xFF) == 0xFF);
        n += ((*(str + 1) & 0xF0) == 0xF0);
    }
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
        pthread_mutex_lock(&(fBuffer->mutex));
        fBuffer->frame_read_index = fBuffer->frame_write_index;
        pthread_mutex_unlock(&(fBuffer->mutex));
        fHaveStartedReading = True;
    }

    fFrameSize = fMaxSize;

    if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() start - fMaxSize %d\n", current_timestamp(), fMaxSize);

    pthread_mutex_lock(&(fBuffer->mutex));
    if (fBuffer->frame_read_index == fBuffer->frame_write_index) {
        pthread_mutex_unlock(&(fBuffer->mutex));
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
    } else if (fBuffer->output_frame[fBuffer->frame_read_index].ptr == NULL) {
        pthread_mutex_unlock(&(fBuffer->mutex));
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
    } else if (cb_check_sync_word(fBuffer->output_frame[fBuffer->frame_read_index].ptr) != 1) {
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;
        pthread_mutex_unlock(&(fBuffer->mutex));
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

    // Frame found, send it
    unsigned char *ptr;
    unsigned int size;
    ptr = fBuffer->output_frame[fBuffer->frame_read_index].ptr;
    size = fBuffer->output_frame[fBuffer->frame_read_index].size;
    ptr += HEADER_SIZE;
    if (ptr >= fBuffer->buffer + fBuffer->size) ptr -= fBuffer->size;
    size -= HEADER_SIZE;

    if (size <= fMaxSize) {
        // The size of the frame is smaller than the available buffer
        fNumTruncatedBytes = 0;
        fFrameSize = size;
        if (debug & 8) fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() whole frame - fFrameSize %d - counter %d - fMaxSize %d\n", current_timestamp(), fFrameSize, fBuffer->output_frame[fBuffer->frame_read_index].counter, fMaxSize);
        if (ptr + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, ptr, fBuffer->buffer + fBuffer->size - ptr);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - ptr), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - ptr));
        } else {
            memmove(fTo, ptr, fFrameSize);
        }
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;

        pthread_mutex_unlock(&(fBuffer->mutex));
    } else {
        // The size of the frame is greater than the available buffer
        fNumTruncatedBytes = size - fMaxSize;
        fFrameSize = fMaxSize;
        fprintf(stderr, "%lld: AudioFramedMemorySource - doGetNextFrame() error - the size of the frame is greater than the available buffer %d/%d\n", current_timestamp(), fFrameSize, fMaxSize);
        if (ptr + fFrameSize > fBuffer->buffer + fBuffer->size) {
            memmove(fTo, ptr, fBuffer->buffer + fBuffer->size - ptr);
            memmove(fTo + (fBuffer->buffer + fBuffer->size - ptr), fBuffer->buffer, fFrameSize - (fBuffer->buffer + fBuffer->size - ptr));
        } else {
            memmove(fTo, ptr, fFrameSize);
        }
        fBuffer->frame_read_index = (fBuffer->frame_read_index + 1) % fBuffer->output_frame_size;

        pthread_mutex_unlock(&(fBuffer->mutex));
    }

#ifndef PRES_TIME_CLOCK
    // Set the 'presentation time':
    struct timeval newPT;
    gettimeofday(&newPT, NULL);
    if ((fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) || (newPT.tv_sec % 60 == 0)) {
        // At the first frame and every minute use the current time:
        gettimeofday(&fPresentationTime, NULL);
    } else {
        // Increment by the play time of the previous data:
        unsigned uSeconds = fPresentationTime.tv_usec + fuSecsPerFrame;
        fPresentationTime.tv_sec += uSeconds/1000000;
        fPresentationTime.tv_usec = uSeconds%1000000;
    }

    fDurationInMicroseconds = fuSecsPerFrame;
#else
    // Use system clock to set presentation time
    gettimeofday(&fPresentationTime, NULL);

    fDurationInMicroseconds = fuSecsPerFrame;
#endif

  // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
            (TaskFunc*)FramedSource::afterGetting, this);
}
