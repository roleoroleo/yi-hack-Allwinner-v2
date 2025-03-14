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
 * Filter for converting between raw PCM audio and aLaw
 */

#include "aLawAudioFilter.hh"

////////// 16-bit PCM (in various byte orders) -> 8-bit a-Law //////////

aLawFromPCMAudioSource* aLawFromPCMAudioSource
::createNew(UsageEnvironment& env, FramedSource* inputSource, int byteOrdering) {
    // "byteOrdering" must be 0, 1, or 2:
    if (byteOrdering < 0 || byteOrdering > 2) {
        env.setResultMsg("aLawFromPCMAudioSource::createNew(): bad \"byteOrdering\" parameter");
        return NULL;
    }
    return new aLawFromPCMAudioSource(env, inputSource, byteOrdering);
}

aLawFromPCMAudioSource
::aLawFromPCMAudioSource(UsageEnvironment& env, FramedSource* inputSource,
                         int byteOrdering)
    : FramedFilter(env, inputSource),
      fByteOrdering(byteOrdering), fInputBuffer(NULL), fInputBufferSize(0) {
}

aLawFromPCMAudioSource::~aLawFromPCMAudioSource() {
    delete[] fInputBuffer;
}

void aLawFromPCMAudioSource::doGetNextFrame() {
    // Figure out how many bytes of input data to ask for, and increase
    // our input buffer if necessary:
    unsigned bytesToRead = fMaxSize*2; // because we're converting 16 bits->8
    if (bytesToRead > fInputBufferSize) {
        delete[] fInputBuffer; fInputBuffer = new unsigned char[bytesToRead];
        fInputBufferSize = bytesToRead;
    }

    // Arrange to read samples into the input buffer:
    fInputSource->getNextFrame(fInputBuffer, bytesToRead,
                               afterGettingFrame, this,
                               FramedSource::handleClosure, this);
}

void aLawFromPCMAudioSource
::afterGettingFrame(void* clientData, unsigned frameSize,
                    unsigned numTruncatedBytes,
                    struct timeval presentationTime,
                    unsigned durationInMicroseconds) {
    aLawFromPCMAudioSource* source = (aLawFromPCMAudioSource*)clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes,
                               presentationTime, durationInMicroseconds);
}

#define CLIP 32767

static unsigned char aLawFrom16BitLinear(u_int16_t sample) {
    unsigned char sign = (sample >> 8) & 0x80;
    if (sign != 0) sample = -sample; // get the magnitude

    if (sample > CLIP) sample = CLIP; // clip the magnitude

    unsigned char exponent = 7;
    for (int exponentMask = 0x4000; (sample & exponentMask) == 0 && exponent > 0; exponentMask = exponentMask >> 1) {
        exponent--;
    }

    unsigned char mantissa;
    if (exponent < 2)
        mantissa = (sample >> 4) & 0x0F;
    else
        mantissa = (sample >> (exponent + 3)) & 0x0F;

    unsigned char result = (sign | (exponent << 4) | mantissa);
    result = result^0xD5;

    return result;
}

void aLawFromPCMAudioSource
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                     struct timeval presentationTime,
                     unsigned durationInMicroseconds) {
    // Translate raw 16-bit PCM samples (in the input buffer)
    // into aLaw samples (in the output buffer).
    unsigned numSamples = frameSize/2;
    switch (fByteOrdering) {
        case 0: { // host order
            u_int16_t* inputSample = (u_int16_t*)fInputBuffer;
            for (unsigned i = 0; i < numSamples; ++i) {
                fTo[i] = aLawFrom16BitLinear(inputSample[i]);
            }
            break;
        }
        case 1: { // little-endian order
            for (unsigned i = 0; i < numSamples; ++i) {
                u_int16_t const newValue = (fInputBuffer[2*i+1]<<8)|fInputBuffer[2*i];
                fTo[i] = aLawFrom16BitLinear(newValue);
            }
            break;
        }
        case 2: { // network (i.e., big-endian) order
            for (unsigned i = 0; i < numSamples; ++i) {
                u_int16_t const newValue = (fInputBuffer[2*i]<<8)|fInputBuffer[2*i+i];
                fTo[i] = aLawFrom16BitLinear(newValue);
            }
            break;
        }
    }

    // Complete delivery to the client:
    fFrameSize = numSamples;
    fNumTruncatedBytes = numTruncatedBytes;
    fPresentationTime = presentationTime;
    fDurationInMicroseconds = durationInMicroseconds;
    afterGetting(this);
}


////////// a-Law -> 16-bit PCM (in host order) //////////

PCMFromaLawAudioSource* PCMFromaLawAudioSource
::createNew(UsageEnvironment& env, FramedSource* inputSource) {
    return new PCMFromaLawAudioSource(env, inputSource);
}

PCMFromaLawAudioSource
::PCMFromaLawAudioSource(UsageEnvironment& env,
			 FramedSource* inputSource)
    : FramedFilter(env, inputSource),
      fInputBuffer(NULL), fInputBufferSize(0) {
}

PCMFromaLawAudioSource::~PCMFromaLawAudioSource() {
    delete[] fInputBuffer;
}

void PCMFromaLawAudioSource::doGetNextFrame() {
    // Figure out how many bytes of input data to ask for, and increase
    // our input buffer if necessary:
    unsigned bytesToRead = fMaxSize/2; // because we're converting 8 bits->16
    if (bytesToRead > fInputBufferSize) {
        delete[] fInputBuffer; fInputBuffer = new unsigned char[bytesToRead];
        fInputBufferSize = bytesToRead;
    }

    // Arrange to read samples into the input buffer:
    fInputSource->getNextFrame(fInputBuffer, bytesToRead,
                               afterGettingFrame, this,
                               FramedSource::handleClosure, this);
}

void PCMFromaLawAudioSource
::afterGettingFrame(void* clientData, unsigned frameSize,
                    unsigned numTruncatedBytes,
                    struct timeval presentationTime,
                    unsigned durationInMicroseconds) {
    PCMFromaLawAudioSource* source = (PCMFromaLawAudioSource*)clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes,
                               presentationTime, durationInMicroseconds);
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

void PCMFromaLawAudioSource
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                     struct timeval presentationTime,
                     unsigned durationInMicroseconds) {
    // Translate aLaw samples (in the input buffer)
    // into 16-bit PCM samples (in the output buffer), in host order.
    unsigned numSamples = frameSize;
    u_int16_t* outputSample = (u_int16_t*)fTo;
    for (unsigned i = 0; i < numSamples; ++i) {
        outputSample[i] = linear16FromaLaw(fInputBuffer[i]);
    }

    // Complete delivery to the client:
    fFrameSize = numSamples*2;
    fNumTruncatedBytes = numTruncatedBytes;
    fPresentationTime = presentationTime;
    fDurationInMicroseconds = durationInMicroseconds;
    afterGetting(this);
}
