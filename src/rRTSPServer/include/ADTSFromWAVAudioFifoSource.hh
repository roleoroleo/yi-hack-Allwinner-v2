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
// A source object for WAV fifo file converted in AAC/ADTS format
// C++ header

#ifndef _ADTS_FROM_WAV_AUDIO_FIFO_SOURCE_HH
#define _ADTS_FROM_WAV_AUDIO_FIFO_SOURCE_HH

#ifndef _FRAMED_FILE_SOURCE_HH
#include "FramedFileSource.hh"
#endif

#include "fdk-aac/aacenc_lib.h"

class ADTSFromWAVAudioFifoSource: public FramedSource {
public:
  static ADTSFromWAVAudioFifoSource* createNew(UsageEnvironment& env,
				       char const* fileName);

  unsigned samplingFrequency() const { return fSamplingFrequency; }
  unsigned numChannels() const { return fNumChannels; }
  char const* configStr() const { return fConfigStr; }

private:
  ADTSFromWAVAudioFifoSource(UsageEnvironment& env, FILE* fid, u_int8_t profile,
		      u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration);
	// called only by createNew()

  virtual ~ADTSFromWAVAudioFifoSource();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();

private:
  FILE *fFid;
  unsigned fSamplingFrequency;
  unsigned fNumChannels;
  unsigned fuSecsPerFrame;
  char fConfigStr[5];

  // fdk-aac
  HANDLE_AACENCODER fHAacEncoder;
  AACENC_InfoStruct fEncInfo;

  INT_PCM fInputBuffer[2048];
  UCHAR fAncillaryBuffer[50];
  AACENC_MetaData fMetaDataSetup;
  UCHAR fOutputBuffer[1024];

  UCHAR fPermanentOutputBuffer[1024];
  UINT fPermanentOutputBufferSize;

  void *fInBuffer[3];
  INT fInBufferIds[3];
  INT fInBufferSize[3];
  INT fInBufferElSize[3];
  void *fOutBuffer[1];
  INT fOutBufferIds[1];
  INT fOutBufferSize[1];
  INT fOutBufferElSize[1];

  AACENC_BufDesc fInBufDesc;
  AACENC_BufDesc fOutBufDesc;

  AACENC_InArgs fInargs;
  AACENC_OutArgs fOutargs;
};

#endif
