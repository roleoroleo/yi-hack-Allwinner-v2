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
// on demand, from an AAC fifo file in ADTS format
// Implementation

#include "ADTSFromWAVAudioFifoServerMediaSubsession.hh"
#include "ADTSFromWAVAudioFifoSource.hh"
#include "MPEG4GenericRTPSink.hh"
#include "FramedFilter.hh"

//#define DEBUG 1

ADTSFromWAVAudioFifoServerMediaSubsession*
ADTSFromWAVAudioFifoServerMediaSubsession::createNew(UsageEnvironment& env,
					     StreamReplicator* replicator,
					     Boolean reuseFirstSource) {
  return new ADTSFromWAVAudioFifoServerMediaSubsession(env, replicator, reuseFirstSource);
}

ADTSFromWAVAudioFifoServerMediaSubsession
::ADTSFromWAVAudioFifoServerMediaSubsession(UsageEnvironment& env,
				    StreamReplicator* replicator, Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession(env, reuseFirstSource),
    fReplicator(replicator) {

#ifdef DEBUG
  printf("New ADTSFromWAVAudioFifoServerMediaSubsession\n");
#endif
}

ADTSFromWAVAudioFifoServerMediaSubsession
::~ADTSFromWAVAudioFifoServerMediaSubsession() {
}

FramedSource* ADTSFromWAVAudioFifoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
#ifdef DEBUG
      printf("ADTSFromWAVAudioFifoServerMediaSubsession createNewStreamSource\n");
#endif
  FramedSource* resultSource = NULL;
  ADTSFromWAVAudioFifoSource* originalSource = NULL;
  FramedFilter* previousSource = (FramedFilter*)fReplicator->inputSource();
  estBitrate = 12; // kbps, estimate

  // Iterate back into the filter chain until a source is found that 
  // has a sample frequency and expected to be a ADTSFromWAVAudioFifoSource.
  for (int x = 0; x < 10; x++) {
#ifdef DEBUG
    printf("x = %d\n", x);
#endif
    if (((ADTSFromWAVAudioFifoSource*)(previousSource))->samplingFrequency() > 0) {
#ifdef DEBUG
      printf("ADTSFromWAVAudioFifoSource found at x = %d\n", x);
#endif
      originalSource = (ADTSFromWAVAudioFifoSource*)(previousSource);
      break;
    }
    previousSource = (FramedFilter*)previousSource->inputSource();
  }
#ifdef DEBUG
  printf("fReplicator->inputSource() = %p\n", originalSource);
#endif
  resultSource = fReplicator->createStreamReplica();

  if (resultSource == NULL) {
    printf("Failed to create stream replica\n");
    Medium::close(resultSource);
    return NULL;
  }
  else {
    fSamplingFrequency = originalSource->samplingFrequency();
    fNumChannels = originalSource->numChannels();
    memcpy(fConfigStr, originalSource->configStr(), 5);

#ifdef DEBUG
    printf("Original source samplingFrequency: %d numChannels: %d configStr: %s\n",
            originalSource->samplingFrequency(), originalSource->numChannels(), originalSource->configStr());
#endif

    estBitrate = 12; // kbps, estimate
    return resultSource;
  }
}

RTPSink* ADTSFromWAVAudioFifoServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* /*inputSource*/) {

#ifdef DEBUG
        fprintf(stderr, "samplingFrequency %d\nconfigStr %s\nnumChannels %d\n",
            fSamplingFrequency,
            fConfigStr,
            fNumChannels);
#endif
  return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
					rtpPayloadTypeIfDynamic,
					fSamplingFrequency,
					"audio", "AAC-hbr", fConfigStr,
					fNumChannels);
}
