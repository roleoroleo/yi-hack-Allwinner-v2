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
// Copyright (c) 2020 roleo.  All rights reserved.
// Filters for converting between raw PCM audio and aLaw
// Implementation

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

static u_int16_t linear16FromaLaw(unsigned char aLawByte) {
//  static int const exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
    aLawByte ^= 0xD5;

    int sign = aLawByte & 0x80;
    unsigned char exponent = (aLawByte>>4) & 0x07;
    unsigned char mantissa = aLawByte & 0x0F;

    mantissa <<= 4;
    mantissa += 8;
    if (exponent != 0)
        mantissa += 0x100;
    if (exponent > 1)
        mantissa <<= (exponent -1);

    u_int16_t result = (u_int16_t) (sign == 0 ? mantissa : -mantissa);

    return result;
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
