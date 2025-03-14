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
 * A ServerMediaSubsession object that creates new, unicast, RTPSink
 * on demand, from an AAC audio stream in ADTS format
 */

#ifndef _ADTS_AUDIO_FRAMED_MEMORY_SERVER_MEDIA_SUBSESSION_HH
#define _ADTS_AUDIO_FRAMED_MEMORY_SERVER_MEDIA_SUBSESSION_HH

#include "OnDemandServerMediaSubsession.hh"
#include "StreamReplicator.hh"

#include "rRTSPServer.h"

class ADTSAudioFramedMemoryServerMediaSubsession: public OnDemandServerMediaSubsession {
public:
    static ADTSAudioFramedMemoryServerMediaSubsession*
    createNew(UsageEnvironment& env, StreamReplicator *replicator,
              Boolean reuseFirstSource, unsigned samplingFrequency,
              unsigned char numChannels);

    // Used to implement "getAuxSDPLine()":
    void checkForAuxSDPLine1();
    void afterPlayingDummy1();

protected:
    ADTSAudioFramedMemoryServerMediaSubsession(UsageEnvironment& env,
                                               StreamReplicator *replicator,
                                               Boolean reuseFirstSource,
                                               unsigned samplingFrequency,
                                               unsigned char numChannels);
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
    char* fAuxSDPLine;
    char fDoneFlag; // used when setting up "fAuxSDPLine"
    RTPSink* fDummyRTPSink; // ditto
    StreamReplicator *fReplicator;
    unsigned fSamplingFrequency;
    unsigned char fNumChannels;
    char fConfigStr[5];
};

#endif
