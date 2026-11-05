/*
 * Copyright (c) 2026 roleo.
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
#include "rRTSPServer.h"

#include "Speaker.hh"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

////////// ADTS2PCMFileSink //////////

extern int debug;

// Open the output fifo O_WRONLY | O_NONBLOCK:
// open() never blocks the event loop; if nobody is reading the fifo it
// fails immediately (ENXIO) instead of hanging until a reader appears
static FILE* openOutputFifo(UsageEnvironment& env, char const* fileName) {
    int fd = open(fileName, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "%lld: unable to open output fifo %s: %s\n",
                current_timestamp(), fileName, strerror(errno));
        env.setResultMsg("unable to open output fifo \"", fileName, "\"");
        return NULL;
    }
    FILE* fid = fdopen(fd, "wb");
    if (fid == NULL) {
        close(fd);
        env.setResultMsg("unable to fdopen output fifo \"", fileName, "\"");
    }

    return fid;
}

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
      fNumChannels(numChannels), fPacketCounter(0), fSpeaker(NULL), fOutputClosed(False) {

    if (debug & 16) fprintf(stderr, "%lld: ADTS2PCMFileSink - Starting sink\n", current_timestamp());

    int i, ret;
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

    fAACDecoder = AACInitDecoder();
    if (fAACDecoder) {
        memset(&fAACFrameInfo, 0, sizeof(_AACFrameInfo));
        fAACFrameInfo.nChans = 1;
        fAACFrameInfo.sampRateCore = sampleRate;
        fAACFrameInfo.profile = AAC_PROFILE_LC;

        ret = AACSetRawBlockParams(fAACDecoder, 0, &fAACFrameInfo);
        if (ret == ERR_AAC_NONE) {
            if (enableSpeaker) {
                fSpeaker = Speaker::createNew();
            } else {
                fSpeaker = NULL; 
            }
        } else {
            fprintf(stderr, "%lld: ADTS2PCMFileSink - AACSetRawBlockParams failed: %d", current_timestamp(), ret);
        }
    } else {
        fprintf(stderr, "%lld: ADTS2PCMFileSink - Couldn't open AAC decoder\n", current_timestamp());
    }
}

ADTS2PCMFileSink::~ADTS2PCMFileSink() {
    if (fSpeaker != NULL)
        delete fSpeaker;

    if (!fAACDecoder)
    {
        return;
    }

    AACFreeDecoder(fAACDecoder);
    fAACDecoder = NULL;
    memset(&fAACFrameInfo, 0, sizeof(AACFrameInfo));
    if (debug & 16) fprintf(stderr, "%lld: ADTS2PCMFileSink - AAC decoder closed successfully.\n", current_timestamp());

    return;
}

ADTS2PCMFileSink* ADTS2PCMFileSink::createNew(UsageEnvironment& env,
                                              char const* fileName,
                                              int sampleRate, int numChannels,
                                              Boolean enableSpeaker, unsigned bufferSize) {
    do {
        FILE* fid;
        fid = openOutputFifo(env, fileName);
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
    int size = (int) dataSize;
    int i;

    if (debug & 16) fprintf(stderr, "%lld: ADTS2PCMFileSink - addData - size: %u\n", current_timestamp(), dataSize);

    // Write to our file:
    if (fOutFid != NULL && data != NULL) {

        if (!fAACDecoder)
        {
            fprintf(stderr, "%lld: ADTS2PCMFileSink - AAC decoder instance is not initialized\n", current_timestamp());
            return;
        }

        if (dataSize < 7)
        {
            if (debug & 16) fprintf(stderr, "%lld: ADTS2PCMFileSink - Input buffer too small for AAC header (%d < 7)\n", current_timestamp(), dataSize);
            return;
        }

        int ret = AACDecode(fAACDecoder, &data, &size, fPCMBuffer);

        if (ret < 0)
        {
            if (ret != ERR_AAC_INDATA_UNDERFLOW)
            {
                fprintf(stderr, "%lld: ADTS2PCMFileSink - AAC decode failed: %d\n", current_timestamp(), ret);
            }
            return;
        }

        // Get frame info to determine actual output properties
        AACFrameInfo frameInfoOut;
        AACGetLastFrameInfo(fAACDecoder, &frameInfoOut);

        if (debug & 16) {
            fprintf(stderr, "Sample rate: %d\n", frameInfoOut.sampRateOut);
            fprintf(stderr, "Number of samples: %d\n", frameInfoOut.outputSamps);
            fprintf(stderr, "Bits per sample: %d\n", frameInfoOut.bitsPerSample);
            fprintf(stderr, "Nummer of channels: %d\n", frameInfoOut.nChans);
        }

        if (fSpeaker != NULL)
            fSpeaker->switchSpeaker(SPEAKER_ON);

        // The output fifo is mono: keep only the first channel and,
        // if the decoder doubled the rate (SBR), drop every other sample.
        // Done in place: the write index never overtakes the read index.
        int nChans = (frameInfoOut.nChans > 0) ? frameInfoOut.nChans : 1;
        int numFrames = frameInfoOut.outputSamps / nChans;
        int decimation = (fSampleRate == frameInfoOut.sampRateOut / 2) ? 2 : 1;
        int numOut = 0;
        for (i = 0; i < numFrames; i += decimation) {
            fPCMBuffer[numOut++] = fPCMBuffer[i * nChans];
        }
        writeNonBlocking(fPCMBuffer, numOut * sizeof(short));
    }
}

void ADTS2PCMFileSink::writeNonBlocking(void const* buf, unsigned nbytes) {
    if (fOutFid == NULL || nbytes == 0) return;
    ssize_t n = write(fileno(fOutFid), buf, nbytes);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        // Reader gone (EPIPE) or bad fd: mark the output as closed.
        fOutputClosed = True;
    }
    // Partial writes / EAGAIN (fifo full): intentionally drop the excess
}

void ADTS2PCMFileSink::afterGettingFrame(unsigned frameSize,
                                         unsigned numTruncatedBytes,
                                         struct timeval presentationTime) {

    if (debug & 16) fprintf(stderr, "%lld: ADTS2PCMFileSink - afterGettingFrame\n", current_timestamp());

    if (numTruncatedBytes > 0) {
        fprintf(stderr, "%lld: ADTS2PCMFileSink::afterGettingFrame(): The input frame data was too large for our buffer size (%d).\n", current_timestamp(), fBufferSize);
        fprintf(stderr, "%lld: %d bytes of trailing data was dropped! Correct this by increasing the \"bufferSize\" parameter in the \"createNew()\" call to at least %d\n",
                current_timestamp(), numTruncatedBytes, fBufferSize + numTruncatedBytes);
    }
    addData(fBuffer, frameSize, presentationTime);

    if (fOutFid == NULL || fOutputClosed) {
        // The output fifo has closed.  Handle this the same way as if the input source had closed:
        if (fSource != NULL) fSource->stopGettingFrames();
        onSourceClosure();
        return;
    }

    // Then try getting the next frame:
    continuePlaying();
}
