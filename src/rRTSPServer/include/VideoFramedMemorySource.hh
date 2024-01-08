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
// C++ header

#ifndef _FRAMED_MEMORY_SOURCE_HH
#define _FRAMED_MEMORY_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include <rRTSPServer.h>

class VideoFramedMemorySource: public FramedSource {
public:
    static VideoFramedMemorySource* createNew(UsageEnvironment& env,
                                                int hNumber,
                                                cb_output_buffer *cbBuffer,
                                                unsigned preferredFrameSize = 0,
                                                unsigned playTimePerFrame = 0);

    void seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream = 0);
    void seekToByteRelative(int64_t offset, u_int64_t numBytesToStream = 0);
    static void doGetNextFrameTask(void *clientData);
    void doGetNextFrameEx();

protected:
    VideoFramedMemorySource(UsageEnvironment& env,
                                int hNumber,
                                cb_output_buffer *cbBuffer,
                                unsigned preferredFrameSize,
                                unsigned playTimePerFrame);
        // called only by createNew()

    virtual ~VideoFramedMemorySource();

private:
    int cb_memcmp(unsigned char *str1, unsigned char*str2, size_t n);
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();

private:
    int fHNumber;
    cb_output_buffer *fBuffer;
    u_int64_t fCurIndex;
    unsigned fPreferredFrameSize;
    unsigned fPlayTimePerFrame;
    unsigned fLastPlayTime;
    Boolean fLimitNumBytesToStream;
    u_int64_t fNumBytesToStream; // used iff "fLimitNumBytesToStream" is True
    Boolean fHaveStartedReading;
};

#endif
