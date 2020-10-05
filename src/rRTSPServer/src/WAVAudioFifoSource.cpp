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
// A WAV audio fifo source
// Implementation

#include "WAVAudioFifoSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"

#include <fcntl.h>
#include <sys/ioctl.h>

////////// WAVAudioFifoSource //////////

WAVAudioFifoSource*
WAVAudioFifoSource::createNew(UsageEnvironment& env, char const* fileName) {
  do {
    FILE* fid = OpenInputFile(env, fileName);
    if (fid == NULL) break;

    // Set non blocking
    if (fcntl(fileno(fid), F_SETFL, O_NONBLOCK) != 0) {
        fclose(fid);
        break;
    };

    // Clean fifo content
    unsigned char null[4];
    while (fread(null, 1, sizeof(null), fid) > 0) {}

    WAVAudioFifoSource* newSource = new WAVAudioFifoSource(env, fid);
    if (newSource != NULL && newSource->bitsPerSample() == 0) {
      // The WAV file header was apparently invalid.
      Medium::close(newSource);
      break;
    }

    newSource->fFileSize = (unsigned)GetFileSize(fileName, fid);

    return newSource;
  } while (0);

  return NULL;
}

unsigned WAVAudioFifoSource::numPCMBytes() const {
  if (fFileSize < fWAVHeaderSize) return 0;
  return fFileSize - fWAVHeaderSize;
}

void WAVAudioFifoSource::setScaleFactor(int scale) {
  if (!fFidIsSeekable) return; // we can't do 'trick play' operations on non-seekable files

  fScaleFactor = scale;

  if (fScaleFactor < 0 && TellFile64(fFid) > 0) {
    // Because we're reading backwards, seek back one sample, to ensure that
    // (i)  we start reading the last sample before the start point, and
    // (ii) we don't hit end-of-file on the first read.
    int bytesPerSample = (fNumChannels*fBitsPerSample)/8;
    if (bytesPerSample == 0) bytesPerSample = 1;
    SeekFile64(fFid, -bytesPerSample, SEEK_CUR);
  }
}

void WAVAudioFifoSource::seekToPCMByte(unsigned byteNumber) {
  byteNumber += fWAVHeaderSize;
  if (byteNumber > fFileSize) byteNumber = fFileSize;

  SeekFile64(fFid, byteNumber, SEEK_SET);
}

void WAVAudioFifoSource::limitNumBytesToStream(unsigned numBytesToStream) {
  fNumBytesToStream = numBytesToStream;
  fLimitNumBytesToStream = fNumBytesToStream > 0;
}

unsigned char WAVAudioFifoSource::getAudioFormat() {
  return fAudioFormat;
}


#define nextc fgetc(fid)

static Boolean get4Bytes(FILE* fid, u_int32_t& result) { // little-endian
  int c0, c1, c2, c3;
  if ((c0 = nextc) == EOF || (c1 = nextc) == EOF ||
      (c2 = nextc) == EOF || (c3 = nextc) == EOF) return False;
  result = (c3<<24)|(c2<<16)|(c1<<8)|c0;
  return True;
}

static Boolean get2Bytes(FILE* fid, u_int16_t& result) {//little-endian
  int c0, c1;
  if ((c0 = nextc) == EOF || (c1 = nextc) == EOF) return False;
  result = (c1<<8)|c0;
  return True;
}

static Boolean skipBytes(FILE* fid, int num) {
  while (num-- > 0) {
    if (nextc == EOF) return False;
  }
  return True;
}

WAVAudioFifoSource::WAVAudioFifoSource(UsageEnvironment& env, FILE* fid)
  : AudioInputDevice(env, 0, 0, 0, 0)/* set the real parameters later */,
    fFid(fid), fFidIsSeekable(False), fLastPlayTime(0), fHaveStartedReading(False), fWAVHeaderSize(0), fFileSize(0),
    fScaleFactor(1), fLimitNumBytesToStream(False), fNumBytesToStream(0), fAudioFormat(WA_UNKNOWN) {

  // Header vaules: 8 Khz, 16 bit,  mono
  fWAVHeaderSize = 0;
  fBitsPerSample = 16;
  fSamplingFrequency = 8000;
  fNumChannels = 1;
  fAudioFormat = (unsigned char)WA_PCM;

  fPlayTimePerSample = 1e6/(double)fSamplingFrequency;

  // Although PCM is a sample-based format, we group samples into
  // 'frames' for efficient delivery to clients.  Set up our preferred
  // frame size to be close to 20 ms, if possible, but always no greater
  // than 1400 bytes (to ensure that it will fit in a single RTP packet)
  unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
  unsigned desiredSamplesPerFrame = (unsigned)(0.02*fSamplingFrequency);
  unsigned samplesPerFrame = desiredSamplesPerFrame < maxSamplesPerFrame ? desiredSamplesPerFrame : maxSamplesPerFrame;
  fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample)/8;

  fFidIsSeekable = FileIsSeekable(fFid);
  // Now that we've finished reading the WAV header, all future reads (of audio samples) from the file will be asynchronous:
  makeSocketNonBlocking(fileno(fFid));
}

WAVAudioFifoSource::~WAVAudioFifoSource() {
  if (fFid == NULL) return;

  envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));

  CloseInputFile(fFid);
}

void WAVAudioFifoSource::doGetNextFrame() {
  if (feof(fFid) || ferror(fFid) || (fLimitNumBytesToStream && fNumBytesToStream == 0)) {
    handleClosure();
    return;
  }

  fFrameSize = 0; // until it's set later
  if (!fHaveStartedReading) {
    // Await readable data from the file:
    envir().taskScheduler().turnOnBackgroundReadHandling(fileno(fFid),
							 (TaskScheduler::BackgroundHandlerProc*)&fileReadableHandler, this);
    fHaveStartedReading = True;
  }
}

void WAVAudioFifoSource::doStopGettingFrames() {
  envir().taskScheduler().unscheduleDelayedTask(nextTask());
  envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
  fHaveStartedReading = False;
}

void WAVAudioFifoSource::fileReadableHandler(WAVAudioFifoSource* source, int /*mask*/) {
  if (!source->isCurrentlyAwaitingData()) {
    source->doStopGettingFrames(); // we're not ready for the data yet
    return;
  }
  source->doReadFromFile();
}

void WAVAudioFifoSource::doReadFromFile() {
  // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
  if (fLimitNumBytesToStream && fNumBytesToStream < fMaxSize) {
    fMaxSize = fNumBytesToStream;
  }
  if (fPreferredFrameSize < fMaxSize) {
    fMaxSize = fPreferredFrameSize;
  }
  unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
  if (bytesPerSample == 0) bytesPerSample = 1; // because we can't read less than a byte at a time

  // For 'trick play', read one sample at a time; otherwise (normal case) read samples in bulk:
  unsigned bytesToRead = fScaleFactor == 1 ? fMaxSize - fMaxSize%bytesPerSample : bytesPerSample;
  unsigned numBytesRead;

//  int cap, avail;
//  cap = fcntl(fileno(fFid), F_GETPIPE_SZ);
//  ioctl(fileno(fFid), FIONREAD, &avail);
//  fprintf(stderr, "fFrameSize %d - numBytesRead %d - fifo capacity %d - fifo available %d\n", fFrameSize, numBytesRead, cap, avail);
//  if (avail < cap*9/10) {
//    // Clean fifo content
//    fprintf(stderr, "Cleaning fifo\n");
//    unsigned char null[4];
//    while (fread(null, 1, sizeof(null), fFid) > 0) {}
//  }

  while (1) { // loop for 'trick play' only
    while (1) {
      if (fFidIsSeekable) {
        numBytesRead = fread(fTo, 1, bytesToRead, fFid);
      } else {
        // For non-seekable files (e.g., pipes), call "read()" rather than "fread()", to ensure that the read doesn't block:
        numBytesRead = read(fileno(fFid), fTo, bytesToRead);
      }
      if (numBytesRead > 0) {
        break;
      }
      usleep(25000);
    }

    fFrameSize += numBytesRead;
    fTo += numBytesRead;
    fMaxSize -= numBytesRead;
    fNumBytesToStream -= numBytesRead;

    // If we did an asynchronous read, and didn't read an integral number of samples, then we need to wait for another read:
    if (fFrameSize%bytesPerSample > 0) return;

    // If we're doing 'trick play', then seek to the appropriate place for reading the next sample,
    // and keep reading until we fill the provided buffer:
    if (fScaleFactor != 1) {
      SeekFile64(fFid, (fScaleFactor-1)*bytesPerSample, SEEK_CUR);
      if (fMaxSize < bytesPerSample) break;
    } else {
      break; // from the loop (normal case)
    }
  }

#if 0
  // Set the 'presentation time' and 'duration' of this frame:
  if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
    // This is the first frame, so use the current time:
    gettimeofday(&fPresentationTime, NULL);
  } else {
    // Increment by the play time of the previous data:
    unsigned uSeconds = fPresentationTime.tv_usec + fLastPlayTime;
    fPresentationTime.tv_sec += uSeconds/1000000;
    fPresentationTime.tv_usec = uSeconds%1000000;
  }

  // Remember the play time of this data:
  fDurationInMicroseconds = fLastPlayTime
    = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);
#else
  // Use system clock to set presentation time
  gettimeofday(&fPresentationTime, NULL);
#endif

  // Inform the reader that he has data:
  // Because the file read was done from the event loop, we can call the
  // 'after getting' function directly, without risk of infinite recursion:
  FramedSource::afterGetting(this);
}

Boolean WAVAudioFifoSource::setInputPort(int /*portIndex*/) {
  return True;
}

double WAVAudioFifoSource::getAverageLevel() const {
  return 0.0;//##### fix this later
}
