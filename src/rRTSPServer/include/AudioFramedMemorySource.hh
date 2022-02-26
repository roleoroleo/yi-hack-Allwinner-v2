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

#ifndef _AUDIO_FRAMED_MEMORY_SOURCE_HH
#define _AUDIO_FRAMED_MEMORY_SOURCE_HH

#ifndef _AUDIO_FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include <rRTSPServer.h>

class AudioFramedMemorySource: public FramedSource {
public:
    static AudioFramedMemorySource* createNew(UsageEnvironment& env,
                                                cb_output_buffer *cbBuffer,
                                                unsigned samplingFrequency,
                                                unsigned numChannels);

    unsigned samplingFrequency() const { return fSamplingFrequency; }
    unsigned numChannels() const { return fNumChannels; }
    char const* configStr() const { return fConfigStr; }

protected:
    AudioFramedMemorySource(UsageEnvironment& env,
                                cb_output_buffer *cbBuffer,
                                unsigned samplingFrequency,
                                unsigned numChannels);
        // called only by createNew()

    virtual ~AudioFramedMemorySource();

private:
    int cb_check_sync_word(unsigned char *str);
    // redefined virtual functions:
    virtual void doGetNextFrame();

private:
    cb_output_buffer *fBuffer;
    u_int64_t fCurIndex;
    int fProfile;
    int fSamplingFrequency;
    int fNumChannels;
    unsigned fuSecsPerFrame;
    char fConfigStr[5];
};

#endif
