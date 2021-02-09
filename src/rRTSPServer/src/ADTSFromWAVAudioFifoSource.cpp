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
// Implementation

#include "ADTSFromWAVAudioFifoSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "presentationTime.hh"
#include "fdk-aac/aacenc_lib.h"

#include <fcntl.h>

//#define DEBUG 1

////////// ADTSFromWAVAudioFifoSource //////////

static unsigned const samplingFrequencyTable[16] = {
    96000, 88200, 64000, 48000,
    44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000,
    7350, 0, 0, 0
};

ADTSFromWAVAudioFifoSource*
ADTSFromWAVAudioFifoSource::createNew(UsageEnvironment& env, char const* fileName) {
    FILE* fid = NULL;
    int ErrorStatus;

    do {
#ifdef DEBUG
        fprintf(stderr, "Creating new ADTSFromWAVAudioFifoSource\n");
#endif
        fid = OpenInputFile(env, fileName);
        if (fid == NULL) break;

/*
        // Set non blocking
        if (fcntl(fileno(fid), F_SETFL, O_NONBLOCK) != 0) {
            fclose(fid);
            break;
        };
*/
        // Set a static profile
        u_int8_t profile = 1;
        // Set a static sampling_frequency_index
        u_int8_t sampling_frequency_index = 11;
        // Set a static channel_configuration
        u_int8_t channel_configuration = 1;
#ifdef DEBUG
        fprintf(stderr, "Setting profile: profile %d, "
            "sampling_frequency_index %d => samplingFrequency %d, "
            "channel_configuration %d\n",
            profile,
            sampling_frequency_index, samplingFrequencyTable[sampling_frequency_index],
            channel_configuration);
#endif
        return new ADTSFromWAVAudioFifoSource(env, fid, profile,
                                   sampling_frequency_index, channel_configuration);
    } while (0);

    // An error occurred:
    CloseInputFile(fid);
    return NULL;
}

ADTSFromWAVAudioFifoSource
::ADTSFromWAVAudioFifoSource(UsageEnvironment& env, FILE* fid, u_int8_t profile,
                      u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration)
    : FramedSource(env) {
    fFid = fid;
    fSamplingFrequency = samplingFrequencyTable[samplingFrequencyIndex];
    fNumChannels = channelConfiguration == 0 ? 2 : channelConfiguration;
    fuSecsPerFrame
        = (1024/*samples-per-frame*/*1000000) / fSamplingFrequency/*samples-per-second*/;

    // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
    unsigned char audioSpecificConfig[2];
    u_int8_t const audioObjectType = profile + 1;
    audioSpecificConfig[0] = (audioObjectType<<3) | (samplingFrequencyIndex>>1);
    audioSpecificConfig[1] = (samplingFrequencyIndex<<7) | (channelConfiguration<<3);
    sprintf(fConfigStr, "%02X%02X", audioSpecificConfig[0], audioSpecificConfig[1]);
#ifdef DEBUG
    fprintf(stderr, "Read config: audioObjectType %d, samplingFrequencyIndex %d, channelConfiguration %d\n", audioObjectType, samplingFrequencyIndex, channelConfiguration);
#endif

    // Open encoder
    fHAacEncoder = NULL;
    int ErrorStatus = aacEncOpen(&fHAacEncoder, 0, 0);
    if (ErrorStatus != AACENC_OK) {
        fprintf(stderr, "Error opening encoder\n");
        return;
    }

    // Setting parameters
    aacEncoder_SetParam(fHAacEncoder, AACENC_AOT, AOT_AAC_LC); //AOT_AAC_LC or AOT_ER_AAC_LD
    aacEncoder_SetParam(fHAacEncoder, AACENC_BITRATE, 12000);
    aacEncoder_SetParam(fHAacEncoder, AACENC_BITRATEMODE, 0);
    aacEncoder_SetParam(fHAacEncoder, AACENC_SAMPLERATE, 8000);
    aacEncoder_SetParam(fHAacEncoder, AACENC_CHANNELMODE, MODE_1);
    aacEncoder_SetParam(fHAacEncoder, AACENC_TRANSMUX, TT_MP4_ADTS);

    // Init encoder
    ErrorStatus = aacEncEncode(fHAacEncoder, NULL, NULL, NULL, NULL);
    if (ErrorStatus != AACENC_OK) {
        fprintf(stderr, "Error initializing encoder\n");
        return;
    }

    aacEncInfo(fHAacEncoder, &fEncInfo);

    fInBuffer[0] = fInputBuffer;
    fInBuffer[1] = fAncillaryBuffer;
    fInBuffer[2] = &fMetaDataSetup;
    fInBufferIds[0] = IN_AUDIO_DATA;
    fInBufferIds[1] = IN_ANCILLRY_DATA;
    fInBufferIds[2] = IN_METADATA_SETUP;
    fInBufferSize[0] = sizeof(fInputBuffer);
    fInBufferSize[1] = sizeof(fAncillaryBuffer);
    fInBufferSize[2] = sizeof(fMetaDataSetup);
    fInBufferElSize[0] = sizeof(INT_PCM);
    fInBufferElSize[1] = sizeof(UCHAR);
    fInBufferElSize[2] = sizeof(AACENC_MetaData);
    fOutBuffer[0] = fOutputBuffer;
    fOutBufferIds[0] = OUT_BITSTREAM_DATA;
    fOutBufferSize[0] = sizeof(fOutputBuffer);
    fOutBufferElSize[0] = sizeof(UCHAR);

    fInBufDesc.numBufs = sizeof(fInBuffer) / sizeof(void*);
    fInBufDesc.bufs = (void**) &fInBuffer;
    fInBufDesc.bufferIdentifiers = fInBufferIds;
    fInBufDesc.bufSizes = fInBufferSize;
    fInBufDesc.bufElSizes = fInBufferElSize;

    fOutBufDesc.numBufs = sizeof(fOutBuffer) / sizeof(void*);
    fOutBufDesc.bufs = (void**) &fOutBuffer;
    fOutBufDesc.bufferIdentifiers = fOutBufferIds;
    fOutBufDesc.bufSizes = fOutBufferSize;
    fOutBufDesc.bufElSizes = fOutBufferElSize;

    fPermanentOutputBufferSize = 0;

#ifdef DEBUG
    fprintf(stderr, "inargs.numInSamples %d\n", fInargs.numInSamples);
    fprintf(stderr, "encInfo.inputChannels %d\n", fEncInfo.inputChannels);
    fprintf(stderr, "encInfo.frameLength %d\n", fEncInfo.frameLength);
    fprintf(stderr, "sizeof(INT_PCM) %d\n", sizeof(INT_PCM));
    fprintf(stderr, "SAMPLE_BITS %d\n", SAMPLE_BITS);
    fprintf(stderr, "encInfo.inputChannels * encInfo.frameLength %d\n", fEncInfo.inputChannels * fEncInfo.frameLength);
    fprintf(stderr, "FDKmin(encInfo.inputChannels * encInfo.frameLength, sizeof(inputBuffer) / sizeof(INT_PCM) - inargs.numInSamples) %d\n",
                FDKmin(fEncInfo.inputChannels * fEncInfo.frameLength, sizeof(fInputBuffer) / sizeof(INT_PCM) - fInargs.numInSamples));
    fprintf(stderr, "encInfo.frameLength %d\n", fEncInfo.frameLength);
#endif
}

ADTSFromWAVAudioFifoSource::~ADTSFromWAVAudioFifoSource() {
    aacEncClose(&fHAacEncoder);
    CloseInputFile(fFid);
}

void ADTSFromWAVAudioFifoSource::doGetNextFrame() {

    int numInSamples;
    int ErrorStatus;

    // Feed input buffer with 1 frame
    // PCM 16 bit, 8KHz, mono = 2048 bytes to read per frame
    // 1 aac frame every 128 ms
    numInSamples = fread((UCHAR *) fInputBuffer,
                sizeof(INT_PCM),
                fEncInfo.inputChannels * fEncInfo.frameLength,
                fFid);
    if (numInSamples == -1) {
        fprintf(stderr, "End of file reached\n");
        return;
    }

    fInargs.numInSamples = numInSamples;
#ifdef DEBUG
    fprintf(stderr, "fInargs.numInSamples %d\n", fInargs.numInSamples);
    fprintf(stderr, "fOutargs.numInSamples %d\n", fOutargs.numInSamples);
#endif

    // Encode input samples
    ErrorStatus = aacEncEncode(fHAacEncoder, &fInBufDesc, &fOutBufDesc, &fInargs, &fOutargs);
#ifdef DEBUG
    fprintf(stderr, "ErrorStatus %d\n", ErrorStatus);

    fprintf(stderr, "fInargs.numInSamples %d\n", fInargs.numInSamples);
    fprintf(stderr, "fOutargs.numInSamples %d\n", fOutargs.numInSamples);

    fprintf(stderr, "outargs.numOutBytes %d\n", fOutargs.numOutBytes);
#endif
    if (fOutargs.numOutBytes > 0) {

        // Begin by reading the 7-byte fixed_variable headers:
        unsigned char *headers = fOutputBuffer; // length = 7
        unsigned char *payload = fOutputBuffer + 7 * sizeof(unsigned char);

        // Extract important fields from the headers:
        Boolean protection_absent = headers[1]&0x01;
        u_int16_t frame_length
            = ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);
#ifdef DEBUG
        u_int16_t syncword = (headers[0]<<4) | (headers[1]>>4);
        fprintf(stderr, "Read frame: syncword 0x%x, protection_absent %d, frame_length %d\n", syncword, protection_absent, frame_length);
        if (syncword != 0xFFF) fprintf(stderr, "WARNING: Bad syncword!\n");
#endif
        unsigned numBytesToRead
            = frame_length > 7 ? frame_length - 7 : 0;

        // If there's a 'crc_check' field, skip it:
        if (!protection_absent) {
            payload += 2 * sizeof(unsigned char);
            numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
        }

        memmove(&fPermanentOutputBuffer[fPermanentOutputBufferSize],
                    payload, numBytesToRead);
        fPermanentOutputBufferSize += numBytesToRead;

        // Next, set the pointer to the raw frame data:
        if (fPermanentOutputBufferSize > fMaxSize) {
            fNumTruncatedBytes = fPermanentOutputBufferSize - fMaxSize;
            fFrameSize = fMaxSize;
        } else {
            fNumTruncatedBytes = 0;
            fFrameSize = fPermanentOutputBufferSize;
        }

        memmove(fTo, fPermanentOutputBuffer, fFrameSize);
        if (fNumTruncatedBytes > 0) {
            memmove(fPermanentOutputBuffer, &fPermanentOutputBuffer[fFrameSize], fNumTruncatedBytes);
#ifdef DEBUG
            fprintf(stderr, "End of buffer: fFrameSize %d, fNumTruncatedBytes %d\n", fFrameSize, fNumTruncatedBytes);
#endif
        }
        fPermanentOutputBufferSize -= fFrameSize;
    }
    else
    {
        fFrameSize = 0;
        fNumTruncatedBytes = 0;
    }

#ifndef PRES_TIME_CLOCK
    // Set the 'presentation time':
    if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
        // This is the first frame, so use the current time:
        gettimeofday(&fPresentationTime, NULL);
    } else {
        // Increment by the play time of the previous frame:
        unsigned uSeconds = fPresentationTime.tv_usec + fuSecsPerFrame;
        fPresentationTime.tv_sec += uSeconds/1000000;
        fPresentationTime.tv_usec = uSeconds%1000000;
    }
#else
    // Use system clock to set presentation time
    gettimeofday(&fPresentationTime, NULL);
#endif

    fDurationInMicroseconds = fuSecsPerFrame;

    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                                (TaskFunc*)FramedSource::afterGetting, this);
}
