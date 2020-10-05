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
// A WAV audio file source
// NOTE: Samples are returned in little-endian order (the same order in which
// they were stored in the file).
// C++ header

#ifndef _WAV_AUDIO_FIFO_SOURCE_HH
#define _WAV_AUDIO_FIFO_SOURCE_HH

#ifndef _AUDIO_INPUT_DEVICE_HH
#include "AudioInputDevice.hh"
#endif

#define  WA_PCM 0x01
#define  WA_PCMA 0x06
#define  WA_PCMU 0x07
#define  WA_IMA_ADPCM 0x11
#define  WA_UNKNOWN 0x12

class WAVAudioFifoSource: public AudioInputDevice {
public:

  static WAVAudioFifoSource* createNew(UsageEnvironment& env,
					char const* fileName);

  unsigned numPCMBytes() const;
  void setScaleFactor(int scale);
  void seekToPCMByte(unsigned byteNumber);
  void limitNumBytesToStream(unsigned numBytesToStream);
      // if "numBytesToStream" is >0, then we limit the stream to that number of bytes, before treating it as EOF

  unsigned char getAudioFormat();

protected:
  WAVAudioFifoSource(UsageEnvironment& env, FILE* fid);
	// called only by createNew()

  virtual ~WAVAudioFifoSource();

  static void fileReadableHandler(WAVAudioFifoSource* source, int mask);
  void doReadFromFile();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  virtual void doStopGettingFrames();
  virtual Boolean setInputPort(int portIndex);
  virtual double getAverageLevel() const;

protected:
  unsigned fPreferredFrameSize;

private:
  FILE* fFid;
  double fPlayTimePerSample; // useconds
  Boolean fFidIsSeekable;
  unsigned fLastPlayTime; // useconds
  Boolean fHaveStartedReading;
  unsigned fWAVHeaderSize;
  unsigned fFileSize;
  int fScaleFactor;
  Boolean fLimitNumBytesToStream;
  unsigned fNumBytesToStream; // used iff "fLimitNumBytesToStream" is True
  unsigned char fAudioFormat;
};

#endif
