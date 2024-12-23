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
 * PCM File Sink
 */

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
    static int const exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
    uLawByte = ~uLawByte;

    int sign = (uLawByte & 0x80) != 0;
    unsigned char exponent = (uLawByte >> 4) & 0x07;
    unsigned char mantissa = uLawByte & 0x0F;

    int16_t result = exp_lut[exponent] + (mantissa << (exponent + 3));
    if (sign) result = -result;
    return result;
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
                         int destSampleRate, int srcLaw,
                         Boolean enableSpeaker, unsigned bufferSize)
    : FileSink(env, fid, bufferSize, NULL), fDestSampleRate(destSampleRate),
      fSrcLaw(srcLaw), fPacketCounter(0) {

    if (enableSpeaker) {
        fSpeaker = Speaker::createNew();
    } else {
        fSpeaker = NULL;
    }
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
                                    int srcLaw, Boolean enableSpeaker,
                                    unsigned bufferSize) {

    if ((destSampleRate != 8000) && (destSampleRate != 16000)) {
        fprintf(stderr, "PCMFileSink::createNew(): The sample rate is not supported\n");
        return NULL;
    }

    do {
        FILE* fid;
        fid = OpenOutputFile(env, fileName);
        if (fid == NULL) break;

        return new PCMFileSink(env, fid, destSampleRate, srcLaw, enableSpeaker, bufferSize);
    } while (0);

    return NULL;
}

Boolean PCMFileSink::continuePlaying() {
    // Call parent
    return FileSink::continuePlaying();
}

void PCMFileSink::addData(unsigned char* data, unsigned dataSize,
                               struct timeval presentationTime) {
    double distance;

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
                        distance = (((double) linear16FromuLaw(data[i / 2])) * 2.0) - fLastSample;
                    } else {
                        distance = (((double) linear16FromaLaw(data[i / 2])) * 2.0) - fLastSample;
                    }
                } else {
                    distance = 0.0 - fLastSample;
                }
                fLastSample += distance * FILTER_M;
                fPCMBuffer[i] = (int16_t) fLastSample;
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
