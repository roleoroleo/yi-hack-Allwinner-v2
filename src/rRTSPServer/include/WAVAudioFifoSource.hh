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
                                         char const* fileName,
                                         unsigned samplingFrequency,
                                         unsigned char numChannels,
                                         unsigned char bitsPerSample);

    unsigned numPCMBytes() const;
    void setScaleFactor(int scale);
    void seekToPCMByte(unsigned byteNumber);
    void limitNumBytesToStream(unsigned numBytesToStream);
      // if "numBytesToStream" is >0, then we limit the stream to that number of bytes, before treating it as EOF

    unsigned char getAudioFormat();

protected:
    WAVAudioFifoSource(UsageEnvironment& env, FILE* fid,
                     unsigned samplingFrequency, unsigned char numChannels,
                     unsigned char bitsPerSample);
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
    void cleanFifo();

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
