/*
 * Copyright (c) 2024 roleo.
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
 * A ServerMediaSubsession object that creates new, unicast, RTPSource
 * on demand, to a PCM Audio
 */

#include "PCMAudioFileServerMediaSubsession_BC.hh"
#include "SimpleRTPSource.hh"
#include "PCMFileSink.hh"

PCMAudioFileServerMediaSubsession_BC*
PCMAudioFileServerMediaSubsession_BC::createNew(UsageEnvironment& env,
                                                char const* fileName,
                                                Boolean reuseFirstSource,
                                                int sampleRate, int numChannels,
                                                int law, Boolean enableSpeaker) {
    return new PCMAudioFileServerMediaSubsession_BC(env, fileName, reuseFirstSource,
                                                    sampleRate, numChannels,
                                                    law, enableSpeaker);
}

PCMAudioFileServerMediaSubsession_BC
::PCMAudioFileServerMediaSubsession_BC(UsageEnvironment& env,
                                       char const* fileName, Boolean reuseFirstSource,
                                       int sampleRate, int numChannels,
                                       int law, Boolean enableSpeaker)
  : FileServerMediaSubsession_BC(env, fileName, reuseFirstSource),
    fSampleRate(sampleRate), fNumChannels(numChannels), fLaw(law),
    fEnableSpeaker(enableSpeaker),
    fAuxSDPLine(NULL), fRTPTimestampFrequency(sampleRate) {
}

PCMAudioFileServerMediaSubsession_BC
::~PCMAudioFileServerMediaSubsession_BC() {
}

MediaSink* PCMAudioFileServerMediaSubsession_BC
::createNewStreamDestination(unsigned clientSessionId, unsigned& estBitrate) {
    estBitrate = 8; // kbps, estimate

    return PCMFileSink::createNew(envir(), fFileName, fSampleRate, fLaw, fEnableSpeaker);
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
