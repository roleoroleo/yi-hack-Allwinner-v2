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

#ifndef _AUDIO_FRAMED_MEMORY_SOURCE_HH
#define _AUDIO_FRAMED_MEMORY_SOURCE_HH

#ifndef _AUDIO_FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include "rRTSPServer.h"

class AudioFramedMemorySource: public FramedSource {
public:
    static AudioFramedMemorySource* createNew(UsageEnvironment& env,
                                                output_queue *qBuffer,
                                                unsigned samplingFrequency,
                                                unsigned char numChannels,
                                                Boolean useTimeForPres);

    unsigned samplingFrequency() const { return fSamplingFrequency; }
    unsigned char numChannels() const { return fNumChannels; }
    char const* configStr() const { return fConfigStr; }
    static void doGetNextFrameTask(void *clientData);
    void doGetNextFrameEx();

protected:
    AudioFramedMemorySource(UsageEnvironment& env,
                                output_queue *qBuffer,
                                unsigned samplingFrequency,
                                unsigned char numChannels,
                                Boolean useTimeForPres);
        // called only by createNew()

    virtual ~AudioFramedMemorySource();

private:
    int check_sync_word(unsigned char *str);
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();

private:
    output_queue *fQBuffer;
    u_int64_t fCurIndex;
    int fProfile;
    unsigned fSamplingFrequency;
    unsigned char  fNumChannels;
    Boolean fUseTimeForPres;
    unsigned fuSecsPerFrame;
    char fConfigStr[5];
    Boolean fHaveStartedReading;
};

#endif
