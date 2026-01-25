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
 * WAV audio fifo file source
 */

#include "WAVAudioFifoSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "rRTSPServer.h"

#include <fcntl.h>

////////// WAVAudioFifoSource //////////

extern int debug;

WAVAudioFifoSource*
WAVAudioFifoSource::createNew(UsageEnvironment& env, char const* fileName,
                              unsigned samplingFrequency, unsigned char numChannels,
                              unsigned char bitsPerSample) {
    do {
        FILE* fid = OpenInputFile(env, fileName);
        if (fid == NULL) break;

        WAVAudioFifoSource* newSource = new WAVAudioFifoSource(env, fid, samplingFrequency, numChannels, bitsPerSample);
        if (newSource != NULL && newSource->bitsPerSample() == 0) {
            // The WAV file header was apparently invalid.
            Medium::close(newSource);
            break;
        }

        newSource->fFileSize = (unsigned) GetFileSize(fileName, fid);
        newSource->cleanFifo();

        if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - WAVAudioFifoSource created\n", current_timestamp());

        return newSource;
    } while (0);

    return NULL;
}

unsigned WAVAudioFifoSource::numPCMBytes() const {
    if (fFileSize < fWAVHeaderSize) return 0;
    return fFileSize - fWAVHeaderSize;
}

void WAVAudioFifoSource::setScaleFactor(int scale) {
    return; // fifo queue is not seekable
}

void WAVAudioFifoSource::seekToPCMByte(unsigned byteNumber) {
    return; // fifo queue is not seekable
}

void WAVAudioFifoSource::limitNumBytesToStream(unsigned numBytesToStream) {
    fNumBytesToStream = numBytesToStream;
    fLimitNumBytesToStream = fNumBytesToStream > 0;
}

unsigned char WAVAudioFifoSource::getAudioFormat() {
    return fAudioFormat;
}

void WAVAudioFifoSource::cleanFifo() {
    int flags;

    if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - cleaning fifo\n", current_timestamp());

    if (fFid == NULL) return;

    // Set non blocking
    if ((flags = fcntl(fileno(fFid), F_GETFL, 0)) < 0) {
        return;
    };
    if (fcntl(fileno(fFid), F_SETFL, flags | O_NONBLOCK) != 0) {
        return;
    };

    // Clean fifo content
    unsigned char null[4];
    while (read(fileno(fFid), null, sizeof(null)) > 0) {}

    // Restore old blocking
    if (fcntl(fileno(fFid), F_SETFL, flags) != 0) {
        return;
    }

    if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - fifo cleaned\n", current_timestamp());
}

WAVAudioFifoSource::WAVAudioFifoSource(UsageEnvironment& env, FILE* fid,
    unsigned samplingFrequency, unsigned char numChannels, unsigned char bitPerSample)
    : AudioInputDevice(env, 0, 0, 0, 0)/* set the real parameters later */,
      fFid(fid), fLastPlayTime(0), fHaveStartedReading(False), fWAVHeaderSize(0), fFileSize(0),
      fScaleFactor(1), fLimitNumBytesToStream(False), fNumBytesToStream(0), fAudioFormat(WA_UNKNOWN) {

    // Header vaules
    fWAVHeaderSize = 0;
    fAudioFormat = (unsigned char) WA_PCM;
    fSamplingFrequency = samplingFrequency;
    fNumChannels = numChannels;
    fBitsPerSample = bitPerSample;

    fPlayTimePerSample = 1e6/(double)fSamplingFrequency;

    // Although PCM is a sample-based format, we group samples into
    // 'frames' for efficient delivery to clients.  Set up our preferred
    // frame size to be close to 20 ms, if possible, but always no greater
    // than 1400 bytes (to ensure that it will fit in a single RTP packet)
    unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
    unsigned desiredSamplesPerFrame = (unsigned)(0.02*fSamplingFrequency);
    unsigned samplesPerFrame = desiredSamplesPerFrame < maxSamplesPerFrame ? desiredSamplesPerFrame : maxSamplesPerFrame;
//    fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample)/8;
    // Force fPreferredFrameSize = 1024
    fPreferredFrameSize = 1024;

    // Now that we've finished reading the WAV header, all future reads (of audio samples) from the file will be asynchronous:
    makeSocketNonBlocking(fileno(fFid));

    if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - maxSamplesPerFrame %u, desiredSamplesPerFrame %u, samplesPerFrame %u, fPreferredFrameSize %u\n",
            current_timestamp(), maxSamplesPerFrame, desiredSamplesPerFrame, samplesPerFrame, fPreferredFrameSize);
}

WAVAudioFifoSource::~WAVAudioFifoSource() {
    if (fFid == NULL) return;

    envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));

    CloseInputFile(fFid);
}

void WAVAudioFifoSource::doGetNextFrame() {
    if (feof(fFid) || (fLimitNumBytesToStream && fNumBytesToStream == 0)) {
        handleClosure();
        return;
    }

    fFrameSize = 0; // until it's set later
    if (!fHaveStartedReading) {
        if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - doGetNextFrame() 1st start\n", current_timestamp());
        cleanFifo();
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
    struct timeval fCurrentTime;
    gettimeofday(&fCurrentTime, NULL);

    // Try to read as many bytes as will fit in the buffer provided (or "fPreferredFrameSize" if less)
    if (fLimitNumBytesToStream && fNumBytesToStream < fMaxSize) {
        fMaxSize = fNumBytesToStream;
    }
    if (fPreferredFrameSize < fMaxSize) {
        fMaxSize = fPreferredFrameSize;
    }

    if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - doReadFromFile() start - fMaxSize %d\n", current_timestamp(), fMaxSize);

    unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
    if (bytesPerSample == 0) bytesPerSample = 1; // because we can't read less than a byte at a time

    unsigned bytesToRead = fMaxSize - fMaxSize%bytesPerSample;
    unsigned numBytesRead;

    while (1) { // loop for 'trick play' only
        while (1) {
            // For non-seekable files (e.g., pipes), call "read()" rather than "fread()", to ensure that the read doesn't block:
            numBytesRead = read(fileno(fFid), fTo, bytesToRead);
            if (numBytesRead > 0) {
                break;
            }
            usleep(2000);
        }

        fFrameSize += numBytesRead;
        fTo += numBytesRead;
        fMaxSize -= numBytesRead;
        fNumBytesToStream -= numBytesRead;

        // If we did an asynchronous read, and didn't read an integral number of samples, then we need to wait for another read:
        if (fFrameSize%bytesPerSample > 0) return;

        break; // from the loop (normal case)
    }

    if (debug & 8) fprintf(stderr, "%lld: WAVAudioFifoSource - doReadFromFile() - fFrameSize %d - fMaxSize %d\n", current_timestamp(), fFrameSize, fMaxSize);

    if (0) {
        // Set the 'presentation time' and 'duration' of this frame:
        struct timeval newPT;
        gettimeofday(&newPT, NULL);
        if ((fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) || (newPT.tv_sec % 60 == 0)) {
            // At the first frame and every minute use the current time:
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
    } else {
        // Use system clock to set presentation time
        gettimeofday(&fPresentationTime, NULL);
        fDurationInMicroseconds = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);
    }

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
