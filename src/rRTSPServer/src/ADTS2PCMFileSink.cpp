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
 * File sink that converts ADTS to PCM
 */

#include "ADTS2PCMFileSink.hh"
#include "GroupsockHelper.hh"
#include "OutputFile.hh"

#include "Speaker.hh"
#include "fdk-aac/aacdecoder_lib.h"

////////// ADTS2PCMFileSink //////////

extern int debug;

unsigned samplingFrequencyTable[16] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350, 0, 0, 0
};

ADTS2PCMFileSink::ADTS2PCMFileSink(UsageEnvironment& env, FILE* fid,
                                   int sampleRate, int numChannels,
                                   Boolean enableSpeaker,
                                   unsigned bufferSize)
    : FileSink(env, fid, bufferSize, NULL), fSampleRate(sampleRate),
      fNumChannels(numChannels), fPacketCounter(0) {

    int i;
    for (i = 0; i < 16; i++) {
        if (sampleRate == (signed) samplingFrequencyTable[i]) {
            fSampleRateIndex = i;
            break;
        }
    }
    if (i == 16) fSampleRateIndex = 8;

    fChannelConfiguration = fNumChannels;
    if (fChannelConfiguration == 8) fChannelConfiguration--;

    // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
    unsigned char audioSpecificConfig[2];
    unsigned int profile = 1;
    u_int8_t const audioObjectType = profile + 1;
    audioSpecificConfig[0] = (audioObjectType << 3) | (fSampleRateIndex >> 1);
    audioSpecificConfig[1] = (fSampleRateIndex << 7) | (fChannelConfiguration << 3);
    sprintf(fConfigStr, "%02X%02X", audioSpecificConfig[0], audioSpecificConfig[1]);

    fAACHandle = aacDecoder_Open(TT_MP4_ADTS, 1);
    if (fAACHandle == NULL) {
        fprintf(stderr, "Couldn't open AAC decoder\n");
    }

    if (enableSpeaker) {
        fSpeaker = Speaker::createNew();
    } else {
        fSpeaker = NULL; 
    }
}

ADTS2PCMFileSink::~ADTS2PCMFileSink() {
    if (fSpeaker != NULL)
        delete fSpeaker;
    aacDecoder_Close(fAACHandle);
}

ADTS2PCMFileSink* ADTS2PCMFileSink::createNew(UsageEnvironment& env,
                                              char const* fileName,
                                              int sampleRate, int numChannels,
                                              Boolean enableSpeaker, unsigned bufferSize) {
    do {
        FILE* fid;
        fid = OpenOutputFile(env, fileName);
        if (fid == NULL) break;

        return new ADTS2PCMFileSink(env, fid, sampleRate, numChannels, enableSpeaker, bufferSize);
    } while (0);

    return NULL;
}

char* ADTS2PCMFileSink::getConfigStr() {
    return fConfigStr;
}

Boolean ADTS2PCMFileSink::continuePlaying() {
    // Call parent
    return FileSink::continuePlaying();
}

void ADTS2PCMFileSink::addData(unsigned char* data, unsigned dataSize,
                               struct timeval presentationTime) {
    // Write to our file:
    if (fOutFid != NULL && data != NULL) {

        unsigned char aacHeader[7];
        unsigned char *aacHeaderPtr = &aacHeader[0];
        unsigned int aacHeaderSize = 7;
        AAC_DECODER_ERROR err;
        unsigned int valid;
        int i;

        dataSize = dataSize + aacHeaderSize;
        aacHeader[0] = 0xFF;
        aacHeader[1] = 0xF1;
        aacHeader[2] = 0x40 | ((fSampleRateIndex << 2) & 0x3c) | ((fChannelConfiguration >> 2) & 0x03);
        aacHeader[3] = ((fChannelConfiguration << 6) & 0xc0) | ((dataSize >> 11) & 0x3);
        aacHeader[4] = (dataSize >> 3) & 0xff;
        aacHeader[5] = ((dataSize << 5) & 0xe0) | 0x1f;
        aacHeader[6] = 0xfc;
        dataSize = dataSize - aacHeaderSize;

        valid = aacHeaderSize;
        err = aacDecoder_Fill(fAACHandle, &aacHeaderPtr, &aacHeaderSize, &valid);
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Fill failed: %x\n", err);
            return;
        }
        valid = dataSize;
        err = aacDecoder_Fill(fAACHandle, &data, &dataSize, &valid);
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Fill failed: %x\n", err);
            return;
        }

        err = aacDecoder_DecodeFrame(fAACHandle, fPCMBuffer, 1024, 0);
//        if (err == AAC_DEC_NOT_ENOUGH_BITS)
//            return;
        if (err != AAC_DEC_OK) {
            fprintf(stderr, "Decode failed: %x\n", err);
            return;
        }
        CStreamInfo *info = aacDecoder_GetStreamInfo(fAACHandle);
        if (debug) {
            fprintf(stderr, "Sample Rate: %d\n", info->sampleRate);
            fprintf(stderr, "Frame Size: %d\n", info->frameSize);
            fprintf(stderr, "Num Channels: %d\n", info->numChannels);
        }

        if (fSpeaker != NULL)
            fSpeaker->switchSpeaker(SPEAKER_ON);

        if (fSampleRate == info->sampleRate / 2) {
            for (i = 0; i < info->frameSize / 2; i++) {
                fPCMBuffer[i] = fPCMBuffer[2 * i];
            }
            fwrite(fPCMBuffer, sizeof(u_int16_t), info->frameSize / 2, fOutFid);
        } else {
            fwrite(fPCMBuffer, sizeof(u_int16_t), info->frameSize, fOutFid);
        }
    }
}

void ADTS2PCMFileSink::afterGettingFrame(unsigned frameSize,
                                         unsigned numTruncatedBytes,
                                         struct timeval presentationTime) {

    if (numTruncatedBytes > 0) {
        fprintf(stderr, "ADTS2PCMFileSink::afterGettingFrame(): The input frame data was too large for our buffer size (%d).\n", fBufferSize);
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
