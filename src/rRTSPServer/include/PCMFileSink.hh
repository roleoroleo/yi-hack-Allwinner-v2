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

#ifndef _PCM_FILE_SINK_HH
#define _PCM_FILE_SINK_HH

#include "FileSink.hh"
#include "Speaker.hh"

#define ULAW 0
#define ALAW 1

#define FILTER_M 0.125

class PCMFileSink: public FileSink {
public:
  static PCMFileSink* createNew(UsageEnvironment& env, char const* fileName,
				int destSampleRate, int srcLaw,
				unsigned bufferSize = 8192);
  // "bufferSize" should be at least as large as the largest expected
  //   input frame.

  virtual void addData(unsigned char* data, unsigned dataSize,
		       struct timeval presentationTime);
  // (Available in case a client wants to add extra data to the output file)

protected:
  PCMFileSink(UsageEnvironment& env, FILE* fid, int destSampleRate, int srcLaw, unsigned bufferSize);
      // called only by createNew()
  virtual ~PCMFileSink();

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

    int fDestSampleRate;
    int fSrcLaw;
    int fPacketCounter;
    int16_t *fPCMBuffer;
    int16_t fLastSample;
    Speaker *fSpeaker;
};

#endif
