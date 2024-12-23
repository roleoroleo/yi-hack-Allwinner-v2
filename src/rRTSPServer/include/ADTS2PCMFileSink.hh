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
 * File sink that converts ADTS to PCM
 */

#ifndef _ADTS2PCM_FILE_SINK_HH
#define _ADTS2PCM_FILE_SINK_HH

#include "FileSink.hh"
#include "Speaker.hh"
#include "fdk-aac/aacdecoder_lib.h"

class ADTS2PCMFileSink: public FileSink {
public:
    static ADTS2PCMFileSink* createNew(UsageEnvironment& env, char const* fileName,
                                       int sampleRate, int numChannels, Boolean enableSpeaker,
                                       unsigned bufferSize = 1024);
    // "bufferSize" should be at least as large as the largest expected
    //   input frame.

    char* getConfigStr();

    virtual void addData(unsigned char* data, unsigned dataSize,
                         struct timeval presentationTime);
    // (Available in case a client wants to add extra data to the output file)

protected:
    ADTS2PCMFileSink(UsageEnvironment& env, FILE* fid, int sampleRate, int numChannels,
                     Boolean enableSpeaker, unsigned bufferSize);
    // called only by createNew()
    virtual ~ADTS2PCMFileSink();

protected: // redefined virtual functions:
    virtual Boolean continuePlaying();

protected:
    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds);
    virtual void afterGettingFrame(unsigned frameSize,
                                   unsigned numTruncatedBytes,
                                   struct timeval presentationTime);

    int fSampleRate;
    int fNumChannels;
    char fConfigStr[8];
    int fPacketCounter;
    HANDLE_AACDECODER fAACHandle;
    INT_PCM fPCMBuffer[1024];
    unsigned fSampleRateIndex;
    unsigned fChannelConfiguration;
    Speaker *fSpeaker;
};

#endif
