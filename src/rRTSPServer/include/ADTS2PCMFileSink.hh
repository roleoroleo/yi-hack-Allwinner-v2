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
// Copyright (c) 1996-2023 Live Networks, Inc.  All rights reserved.
// File Sinks
// C++ header

#ifndef _ADTS2PCM_FILE_SINK_HH
#define _ADTS2PCM_FILE_SINK_HH

#include "FileSink.hh"
#include "Speaker.hh"
#include "fdk-aac/aacdecoder_lib.h"

class ADTS2PCMFileSink: public FileSink {
public:
  static ADTS2PCMFileSink* createNew(UsageEnvironment& env, char const* fileName,
			     int sampleRate, int numChannels, unsigned bufferSize = 1024);
  // "bufferSize" should be at least as large as the largest expected
  //   input frame.

  char* getConfigStr();

  virtual void addData(unsigned char* data, unsigned dataSize,
		       struct timeval presentationTime);
  // (Available in case a client wants to add extra data to the output file)

protected:
  ADTS2PCMFileSink(UsageEnvironment& env, FILE* fid, int sampleRate, int numChannels, unsigned bufferSize);
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
