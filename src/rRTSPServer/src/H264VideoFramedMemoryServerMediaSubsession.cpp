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
 * on demand, from a H264 Elementary Stream video memory.
 */

#include "H264VideoFramedMemoryServerMediaSubsession.hh"
#include "H264VideoRTPSink.hh"
#include "VideoFramedMemorySource.hh"
#include "H264VideoStreamDiscreteFramer.hh"

H264VideoFramedMemoryServerMediaSubsession*
H264VideoFramedMemoryServerMediaSubsession::createNew(UsageEnvironment& env,
                                                output_queue *qBuffer,
                                                Boolean useTimeForPres,
                                                Boolean reuseFirstSource) {
    return new H264VideoFramedMemoryServerMediaSubsession(env, qBuffer, useTimeForPres, reuseFirstSource);
}

H264VideoFramedMemoryServerMediaSubsession::H264VideoFramedMemoryServerMediaSubsession(UsageEnvironment& env,
                                                                        output_queue *qBuffer,
                                                                        Boolean useTimeForPres,
                                                                        Boolean reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      fQBuffer(qBuffer), fUseTimeForPres(useTimeForPres), fAuxSDPLine(NULL), fDoneFlag(0), fDummyRTPSink(NULL) {
}

H264VideoFramedMemoryServerMediaSubsession::~H264VideoFramedMemoryServerMediaSubsession() {
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void* clientData) {
    H264VideoFramedMemoryServerMediaSubsession* subsess = (H264VideoFramedMemoryServerMediaSubsession*)clientData;
    subsess->afterPlayingDummy1();
}

void H264VideoFramedMemoryServerMediaSubsession::afterPlayingDummy1() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void checkForAuxSDPLine(void* clientData) {
    H264VideoFramedMemoryServerMediaSubsession* subsess = (H264VideoFramedMemoryServerMediaSubsession*)clientData;
    subsess->checkForAuxSDPLine1();
}

void H264VideoFramedMemoryServerMediaSubsession::checkForAuxSDPLine1() {
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

char const* H264VideoFramedMemoryServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource) {
    if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == NULL) { // we're not already setting it up for another, concurrent stream
        // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
        // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the source:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource* H264VideoFramedMemoryServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
    if (fQBuffer->type == 360)
        estBitrate = 200; // kbps, estimate
    else if (fQBuffer->type == 1080)
        estBitrate = 700; // kbps, estimate
    else
        estBitrate = 500; // kbps, estimate

    // Create the video source:
    VideoFramedMemorySource* memorySource = VideoFramedMemorySource::createNew(envir(), 264, fQBuffer, fUseTimeForPres, 50000);
    if (memorySource == NULL) return NULL;

    // Create a framer for the Video Elementary Stream:
    return H264VideoStreamDiscreteFramer::createNew(envir(), memorySource, false, false);
}

RTPSink* H264VideoFramedMemoryServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
                    unsigned char rtpPayloadTypeIfDynamic,
                    FramedSource* /*inputSource*/) {
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
