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
// File sinks to a G711 audio file, converted finally
// to a PCM file
// 16 KHz, 16 bit, mono
// Implementation

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#endif
#include "PCMFileSink.hh"
#include "GroupsockHelper.hh"
#include "OutputFile.hh"
#include "uLawAudioFilter.hh"
#include "aLawAudioFilter.hh"
#include "Speaker.hh"

////////// PCMFileSink //////////

extern int debug;

// ulaw and alaw functions
static int16_t linear16FromuLaw(unsigned char uLawByte) {
    unsigned char sign = 0;
    unsigned char position = 0;
    int16_t result = 0;

    u_int16_t BIAS = 33;

    uLawByte = ~uLawByte;
    if (uLawByte & 0x80) {
        uLawByte &= ~(1 << 7);
        sign = -1;
    }
    position = ((uLawByte & 0xF0) >> 4) + 5;
    result = ((1 << position) | ((uLawByte & 0x0F) << (position - 4)) | (1 << (position - 5))) - BIAS;

    return (sign == 0) ? (result) : (-(result));
}

static int16_t alaw_decode[256] = {
    -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
     -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
    -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
    -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
    -22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
    -30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
    -11008, -10496, -12032, -11520, -8960, -8448, -9984, -9472,
    -15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
    -344, -328, -376, -360, -280, -264, -312, -296,
    -472, -456, -504, -488, -408, -392, -440, -424,
    -88, -72, -120, -104, -24, -8, -56, -40,
    -216, -200, -248, -232, -152, -136, -184, -168,
    -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
    -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
    -688, -656, -752, -720, -560, -528, -624, -592,
    -944, -912, -1008, -976, -816, -784, -880, -848,
    5504, 5248, 6016, 5760, 4480, 4224, 4992, 4736,
    7552, 7296, 8064, 7808, 6528, 6272, 7040, 6784,
    2752, 2624, 3008, 2880, 2240, 2112, 2496, 2368,
    3776, 3648, 4032, 3904, 3264, 3136, 3520, 3392,
    22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
    30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
    11008, 10496, 12032, 11520, 8960, 8448, 9984, 9472,
    15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
    344, 328, 376, 360, 280, 264, 312, 296,
    472, 456, 504, 488, 408, 392, 440, 424,
    88, 72, 120, 104, 24, 8, 56, 40,
    216, 200, 248, 232, 152, 136, 184, 168,
    1376, 1312, 1504, 1440, 1120, 1056, 1248, 1184,
    1888, 1824, 2016, 1952, 1632, 1568, 1760, 1696,
    688, 656, 752, 720, 560, 528, 624, 592,
    944, 912, 1008, 976, 816, 784, 880, 848
};

static u_int16_t linear16FromaLaw(unsigned char aLawByte) {
    return alaw_decode[aLawByte];
}

// PCMFileSink class implementation
PCMFileSink::PCMFileSink(UsageEnvironment& env, FILE* fid,
                         int destSampleRate, int srcLaw, unsigned bufferSize)
    : FileSink(env, fid, bufferSize, NULL), fDestSampleRate(destSampleRate),
      fSrcLaw(srcLaw), fPacketCounter(0) {

    fSpeaker = Speaker::createNew();
    fPCMBuffer = new int16_t[bufferSize];
    fLastSample = 0;
}

PCMFileSink::~PCMFileSink() {
    delete[] fPCMBuffer;
    if (fSpeaker != NULL)
        delete fSpeaker;
}

PCMFileSink* PCMFileSink::createNew(UsageEnvironment& env,
                                    char const* fileName, int destSampleRate,
                                    int srcLaw, unsigned bufferSize) {

    if ((destSampleRate != 8000) && (destSampleRate != 16000)) {
        fprintf(stderr, "PCMFileSink::createNew(): The sample rate is not supported\n");
        return NULL;
    }

    do {
        FILE* fid;
        fid = OpenOutputFile(env, fileName);
        if (fid == NULL) break;

        return new PCMFileSink(env, fid, destSampleRate, srcLaw, bufferSize);
    } while (0);

    return NULL;
}

Boolean PCMFileSink::continuePlaying() {
    // Call parent
    return FileSink::continuePlaying();
}

void PCMFileSink::addData(unsigned char* data, unsigned dataSize,
                               struct timeval presentationTime) {
    int16_t distance;

    // fPCMBuffer must be 4x larger than dataSize: 8 KHz -> 16 KHz and 8 bit -> 16 bit
    if (dataSize * 2 * (fDestSampleRate / 8000) > fBufferSize) {
        fprintf(stderr, "PCMFileSink::addData(): The input frame data was too large for our buffer size (%d).\n", fBufferSize);
        return;
    }

    // Convert xLaw to PCM and write to our file
    // Oversample from 8 KHz to 16 KHz if necessary
    if (fOutFid != NULL && data != NULL) {
        if (fDestSampleRate == 16000) {
            for (unsigned i = 0; i < dataSize * 2; ++i) {
                if (i % 2 == 0) {
                    if (fSrcLaw == ULAW) {
                        distance = linear16FromuLaw(data[i / 2]) - fLastSample;
                    } else {
                        distance = linear16FromaLaw(data[i / 2]) - fLastSample;
                    }
                } else {
                    distance = 0 - fLastSample;
                }
                fLastSample += distance * FILTER_M;
                fPCMBuffer[i] = fLastSample * 2;
            }

            if (fSpeaker != NULL)
                fSpeaker->switchSpeaker(SPEAKER_ON);

            fwrite(fPCMBuffer, sizeof(int16_t), dataSize * 2, fOutFid);
        } else  if (fDestSampleRate == 8000) {
            for (unsigned i = 0; i < dataSize; ++i) {
                if (fSrcLaw == ULAW) {
                    fPCMBuffer[i] = linear16FromuLaw(data[i]);
                } else {
                    fPCMBuffer[i] = linear16FromaLaw(data[i]);
                }
            }

            if (fSpeaker != NULL)
                fSpeaker->switchSpeaker(SPEAKER_ON);

            fwrite(fPCMBuffer, sizeof(u_int16_t), dataSize, fOutFid);
        }
    }
}

void PCMFileSink::afterGettingFrame(unsigned frameSize,
                                         unsigned numTruncatedBytes,
                                         struct timeval presentationTime) {

    if (numTruncatedBytes > 0) {
        fprintf(stderr, "PCMFileSink::afterGettingFrame(): The input frame data was too large for our buffer size (%d).\n", fBufferSize);
        fprintf(stderr, "%d bytes of trailing data was dropped! Correct this by increasing the \"bufferSize\" parameter in the \"createNew()\" call to at least %d\n",
                numTruncatedBytes, fBufferSize + numTruncatedBytes);
    }
    addData(fBuffer, frameSize, presentationTime);

    if (fOutFid == NULL || fflush(fOutFid) == EOF) {
        // The output file has closed.  Handle this the same way as if the input source had closed:
        if (fSource != NULL) fSource->stopGettingFrames();
        onSourceClosure();
        return;
    }

    // Then try getting the next frame:
    continuePlaying();
}
