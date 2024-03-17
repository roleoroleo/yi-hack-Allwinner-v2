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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSource"s
// on demand, to a PCM audio file
// Implementation

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#endif
#include "PCMAudioFileServerMediaSubsession_BC.hh"
#include "SimpleRTPSource.hh"
#include "PCMFileSink.hh"

PCMAudioFileServerMediaSubsession_BC*
PCMAudioFileServerMediaSubsession_BC::createNew(UsageEnvironment& env,
						char const* fileName,
						Boolean reuseFirstSource,
						int sampleRate, int numChannels, int law) {
    return new PCMAudioFileServerMediaSubsession_BC(env, fileName, reuseFirstSource,
                                                    sampleRate, numChannels, law);
}

PCMAudioFileServerMediaSubsession_BC
::PCMAudioFileServerMediaSubsession_BC(UsageEnvironment& env,
				       char const* fileName, Boolean reuseFirstSource,
				       int sampleRate, int numChannels, int law)
  : FileServerMediaSubsession_BC(env, fileName, reuseFirstSource),
    fSampleRate(sampleRate), fNumChannels(numChannels), fLaw(law),
    fAuxSDPLine(NULL), fRTPTimestampFrequency(sampleRate) {
}

PCMAudioFileServerMediaSubsession_BC
::~PCMAudioFileServerMediaSubsession_BC() {
}

MediaSink* PCMAudioFileServerMediaSubsession_BC
::createNewStreamDestination(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = 8; // kbps, estimate

    return PCMFileSink::createNew(envir(), fFileName, fSampleRate, fLaw);
}

RTPSource* PCMAudioFileServerMediaSubsession_BC
::createNewRTPSource(Groupsock* rtpGroupsock,
		     unsigned char rtpPayloadTypeIfDynamic,
		     MediaSink* outputSink) {

    // Create the data source: a "Simple RTP source"
    SimpleRTPSource *rtpSource;
    char mimeStr[16] = {0};

    if (fLaw == ULAW) {
        fRTPPayloadFormat = 0;
        strcpy(mimeStr, "audio/PCMU");
    } else {
        fRTPPayloadFormat = 8;
        strcpy(mimeStr, "audio/PCMA");
    }

    rtpSource = SimpleRTPSource::createNew(envir(), rtpGroupsock,
            fRTPPayloadFormat,
            fRTPTimestampFrequency,
            mimeStr, 0, True);

    return rtpSource;
}

char const* PCMAudioFileServerMediaSubsession_BC::getAuxSDPLineForBackChannel(MediaSink* mediaSink, RTPSource* rtpSource)
{
    char pTmpStr[1024] = {0};

    if(fAuxSDPLine != NULL)  return fAuxSDPLine;

    sprintf(pTmpStr, "a=fmtp:%d\r\n", fRTPPayloadFormat);
    fAuxSDPLine = strDup(pTmpStr);

    return fAuxSDPLine;
}

FramedSource* PCMAudioFileServerMediaSubsession_BC
::createNewStreamSource(unsigned clientSessionId,
			unsigned& estBitrate) {
    return NULL;
}

RTPSink* PCMAudioFileServerMediaSubsession_BC
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* inputSource) {

    return NULL;
}
