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
// on demand, to an AAC audio file in ADTS format
// Implementation

#include "ADTSAudioFileServerMediaSubsession_BC.hh"
#include "ADTS2PCMFileSink.hh"
#include "MPEG4GenericRTPSource.hh"

extern int debug;
ADTSAudioFileServerMediaSubsession_BC*
ADTSAudioFileServerMediaSubsession_BC::createNew(UsageEnvironment& env,
						 char const* fileName,
						 Boolean reuseFirstSource,
						 int sampleRate, int numChannels) {
    return new ADTSAudioFileServerMediaSubsession_BC(env, fileName, reuseFirstSource,
                                                     sampleRate, numChannels);
}

ADTSAudioFileServerMediaSubsession_BC
::ADTSAudioFileServerMediaSubsession_BC(UsageEnvironment& env,
				       char const* fileName, Boolean reuseFirstSource,
				       int sampleRate, int numChannels)
  : FileServerMediaSubsession_BC(env, fileName, reuseFirstSource),
    fSampleRate(sampleRate), fNumChannels(numChannels),
    fAuxSDPLine(NULL), fRTPTimestampFrequency(sampleRate) {
}

ADTSAudioFileServerMediaSubsession_BC
::~ADTSAudioFileServerMediaSubsession_BC() {
}

MediaSink* ADTSAudioFileServerMediaSubsession_BC
::createNewStreamDestination(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = 8; // kbps, estimate

    return ADTS2PCMFileSink::createNew(envir(), fFileName, fSampleRate, fNumChannels);
}

RTPSource* ADTSAudioFileServerMediaSubsession_BC
::createNewRTPSource(Groupsock* rtpGroupsock,
		     unsigned char rtpPayloadTypeIfDynamic,
		     MediaSink* outputSink) {


    // Create the data source: a "MPEG4 Generic RTP source"
    MPEG4GenericRTPSource *rtpSource;
    //unsigned char rtpPayloadFormat = 97; // a dynamic payload type
    fRTPPayloadFormat = rtpPayloadTypeIfDynamic;
    rtpSource = MPEG4GenericRTPSource::createNew(envir(), rtpGroupsock,
            fRTPPayloadFormat,
            fRTPTimestampFrequency,
            "audio", "aac-hbr",
            13,   // unsigned sizeLength
            3,    // unsigned indexLength,
            3);   // unsigned indexDeltaLength

    return rtpSource;
}

char const* ADTSAudioFileServerMediaSubsession_BC::getAuxSDPLineForBackChannel(MediaSink* mediaSink, RTPSource* rtpSource)
{
    ADTS2PCMFileSink* sink = NULL;
    char pTmpStr[1024] = {0};

    if(fAuxSDPLine != NULL)  return fAuxSDPLine;

    if (mediaSink != NULL) {
        sink = (ADTS2PCMFileSink*) mediaSink;
        sprintf(pTmpStr, "a=fmtp:%d streamtype=5;profile-level-id=1;mode=aac-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=%s\r\n", fRTPPayloadFormat, sink->getConfigStr());
    }
    fAuxSDPLine = strDup(pTmpStr);

    if (debug) fprintf(stderr, "ADTSAudioFileServerMediaSubsession_BC::getAuxSDPLine() line:%d, pTmpStr=%s\n", __LINE__, pTmpStr);

    return fAuxSDPLine;
}

FramedSource* ADTSAudioFileServerMediaSubsession_BC
::createNewStreamSource(unsigned clientSessionId,
			unsigned& estBitrate) {
    return NULL;
}

RTPSink* ADTSAudioFileServerMediaSubsession_BC
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* inputSource) {

    return NULL;
}
