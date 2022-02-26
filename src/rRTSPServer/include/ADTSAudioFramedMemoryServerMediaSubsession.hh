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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a H264 Elementary Stream video memory.
// C++ header

#ifndef _ADTS_AUDIO_FRAMED_MEMORY_SERVER_MEDIA_SUBSESSION_HH
#define _ADTS_AUDIO_FRAMED_MEMORY_SERVER_MEDIA_SUBSESSION_HH

#include "OnDemandServerMediaSubsession.hh"
#include "StreamReplicator.hh"

#include "rRTSPServer.h"

class ADTSAudioFramedMemoryServerMediaSubsession: public OnDemandServerMediaSubsession {
public:
    static ADTSAudioFramedMemoryServerMediaSubsession*
    createNew(UsageEnvironment& env, StreamReplicator *replicator,
                                Boolean reuseFirstSource);

    // Used to implement "getAuxSDPLine()":
    void checkForAuxSDPLine1();
    void afterPlayingDummy1();

protected:
    ADTSAudioFramedMemoryServerMediaSubsession(UsageEnvironment& env,
                                        StreamReplicator *replicator,
                                        Boolean reuseFirstSource);
        // called only by createNew();
    virtual ~ADTSAudioFramedMemoryServerMediaSubsession();

    void setDoneFlag() { fDoneFlag = ~0; }

protected: // redefined virtual functions
    virtual char const* getAuxSDPLine(RTPSink* rtpSink,
                                    FramedSource* inputSource);
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
                                                unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                    unsigned char rtpPayloadTypeIfDynamic,
                                    FramedSource* inputSource);

private:
    cb_output_buffer *fBuffer;
    char* fAuxSDPLine;
    char fDoneFlag; // used when setting up "fAuxSDPLine"
    char fConfigStr[5];
    RTPSink* fDummyRTPSink; // ditto
    StreamReplicator *fReplicator;
};

#endif
