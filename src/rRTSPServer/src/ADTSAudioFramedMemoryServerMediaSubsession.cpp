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
// Copyright (c) 1996-2022 Live Networks, Inc.  All rights reserved.
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from an AAC audio file in ADTS format.
// Implementation

#include "ADTSAudioFramedMemoryServerMediaSubsession.hh"
#include "ADTSAudioFileSource.hh"
#include "AudioFramedMemorySource.hh"
#include "MPEG4GenericRTPSink.hh"
#include "FramedFilter.hh"

// #define DEBUG 1

ADTSAudioFramedMemoryServerMediaSubsession*
ADTSAudioFramedMemoryServerMediaSubsession::createNew(UsageEnvironment& env,
                                                StreamReplicator *replicator,
                                                Boolean reuseFirstSource) {
    return new ADTSAudioFramedMemoryServerMediaSubsession(env, replicator, reuseFirstSource);
}

ADTSAudioFramedMemoryServerMediaSubsession::ADTSAudioFramedMemoryServerMediaSubsession(UsageEnvironment& env,
                                                                        StreamReplicator *replicator,
                                                                        Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL), fReplicator(replicator) {
}

ADTSAudioFramedMemoryServerMediaSubsession::~ADTSAudioFramedMemoryServerMediaSubsession() {
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
    ADTSAudioFramedMemoryServerMediaSubsession* subsess = (ADTSAudioFramedMemoryServerMediaSubsession*)clientData;
    subsess->afterPlayingDummy1();
}

void ADTSAudioFramedMemoryServerMediaSubsession::afterPlayingDummy1() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
    ADTSAudioFramedMemoryServerMediaSubsession* subsess = (ADTSAudioFramedMemoryServerMediaSubsession*)clientData;
    subsess->checkForAuxSDPLine1();
}

void ADTSAudioFramedMemoryServerMediaSubsession::checkForAuxSDPLine1() {
    nextTask() = NULL;

    char const* dasl;
    if (fAuxSDPLine != NULL) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = NULL;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                (TaskFunc*)checkForAuxSDPLine, this);
    }
}

char const* ADTSAudioFramedMemoryServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
        // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
        // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the file:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource* ADTSAudioFramedMemoryServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    estBitrate = 32; // kbps, estimate

    FramedSource* resultSource = NULL;
    AudioFramedMemorySource* originalSource = NULL;
    FramedFilter* previousSource = (FramedFilter*)fReplicator->inputSource();

    // Iterate back into the filter chain until a source is found that.
    // has a sample frequency and expected to be a WAVAudioFifoSource.
    for (int x = 0; x < 10; x++) {
        if (((AudioFramedMemorySource*)(previousSource))->samplingFrequency() != 0) {
#ifdef DEBUG
            printf("AudioFramedMemorySource found at x = %d\n", x);
#endif
            originalSource = (AudioFramedMemorySource*)(previousSource);
            break;
        }
        previousSource = (FramedFilter*)previousSource->inputSource();
    }
#ifdef DEBUG
    printf("fReplicator->inputSource() = %p\n", originalSource);
#endif
    resultSource = fReplicator->createStreamReplica();
    if (resultSource == NULL) {
        fprintf(stderr, "Failed to create stream replica\n");
        Medium::close(resultSource);
        return NULL;
    } else {
        sprintf(fConfigStr, originalSource->configStr());
#ifdef DEBUG
        fprintf(stderr, "createStreamReplica completed successfully\n");
#endif

        return resultSource;
    }
}

RTPSink* ADTSAudioFramedMemoryServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                    unsigned char rtpPayloadTypeIfDynamic,
                    FramedSource* inputSource) {

    return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
                                        rtpPayloadTypeIfDynamic,
                                        16000,
                                        "audio", "AAC-hbr", fConfigStr,
                                        1);
}
