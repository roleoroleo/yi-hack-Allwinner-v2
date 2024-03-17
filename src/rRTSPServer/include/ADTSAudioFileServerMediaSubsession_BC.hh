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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from an AAC audio file in ADTS format
// C++ header

#ifndef _ADTS_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_BC_HH
#define _ADTS_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_BC_HH

#ifndef _FILE_SERVER_MEDIA_SUBSESSION_BC_HH
#include "FileServerMediaSubsession_BC.hh"
#endif

class ADTSAudioFileServerMediaSubsession_BC: public FileServerMediaSubsession_BC {
public:
  static ADTSAudioFileServerMediaSubsession_BC*
  createNew(UsageEnvironment& env, char const* fileName, Boolean reuseFirstSource,
	    int sampleRate, int numChannels);

protected:
  ADTSAudioFileServerMediaSubsession_BC(UsageEnvironment& env,
				       char const* fileName, Boolean reuseFirstSource,
				       int sampleRate, int numChannels);
      // called only by createNew();
  virtual ~ADTSAudioFileServerMediaSubsession_BC();

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
  virtual char const* getAuxSDPLineForBackChannel(MediaSink* mediaSink, RTPSource* rtpSource);

private:
  int fSampleRate;
  int fNumChannels;
  char* fAuxSDPLine;
  unsigned char fRTPPayloadFormat;
  unsigned fRTPTimestampFrequency;
};

#endif
