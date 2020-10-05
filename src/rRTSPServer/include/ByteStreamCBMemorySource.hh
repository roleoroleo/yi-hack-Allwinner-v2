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

#ifndef _BYTE_STREAM_CB_MEMORY_SOURCE_HH
#define _BYTE_STREAM_CB_MEMORY_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include <rRTSPServer.h>

class ByteStreamCBMemorySource: public FramedSource {
public:
    static ByteStreamCBMemorySource* createNew(UsageEnvironment& env,
                                                cb_output_buffer *cbBuffer,
                                                unsigned preferredFrameSize = 0,
                                                unsigned playTimePerFrame = 0);
      // "preferredFrameSize" == 0 means 'no preference'
      // "playTimePerFrame" is in microseconds

    void seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream = 0);
      // if "numBytesToStream" is >0, then we limit the stream to that number of bytes, before treating it as EOF
    void seekToByteRelative(int64_t offset, u_int64_t numBytesToStream = 0);

protected:
    ByteStreamCBMemorySource(UsageEnvironment& env,
                                cb_output_buffer *cbBuffer,
                                unsigned preferredFrameSize,
                                unsigned playTimePerFrame);
        // called only by createNew()

    virtual ~ByteStreamCBMemorySource();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();

private:
    cb_output_buffer *fBuffer;
    u_int64_t fCurIndex;
    unsigned fPreferredFrameSize;
    unsigned fPlayTimePerFrame;
    unsigned fLastPlayTime;
    Boolean fLimitNumBytesToStream;
    u_int64_t fNumBytesToStream; // used iff "fLimitNumBytesToStream" is True
};

#endif
