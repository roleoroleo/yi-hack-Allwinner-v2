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

#ifndef _PCM_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_BC_HH
#define _PCM_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_BC_HH

#ifndef _FILE_SERVER_MEDIA_SUBSESSION_BC_HH
#include "FileServerMediaSubsession_BC.hh"
#endif

class PCMAudioFileServerMediaSubsession_BC: public FileServerMediaSubsession_BC {
public:
    static PCMAudioFileServerMediaSubsession_BC*
    createNew(UsageEnvironment& env, char const* fileName, Boolean reuseFirstSource,
	      int sampleRate, int numChannels, int law, Boolean enableSpeaker);

protected:
    PCMAudioFileServerMediaSubsession_BC(UsageEnvironment& env,
					 char const* fileName, Boolean reuseFirstSource,
					 int sampleRate, int numChannels, int law, Boolean enableSpeaker);
      // called only by createNew();
    virtual ~PCMAudioFileServerMediaSubsession_BC();

protected: // redefined virtual functions
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
						unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
				      unsigned char rtpPayloadTypeIfDynamic,
				      FramedSource* inputSource);

    virtual MediaSink* createNewStreamDestination(unsigned clientSessionId,
						  unsigned& estBitrate);
    // "estBitrate" is the stream's estimated bitrate, in kbps
    virtual RTPSource* createNewRTPSource(Groupsock* rtpGroupsock,
					  unsigned char rtpPayloadTypeIfDynamic,
					  MediaSink* outputSink);

private:
    int fSampleRate;
    int fNumChannels;
    int fLaw;
    Boolean fEnableSpeaker;
    char* fAuxSDPLine;
    unsigned char fRTPPayloadFormat;
    unsigned fRTPTimestampFrequency;

};

#endif
