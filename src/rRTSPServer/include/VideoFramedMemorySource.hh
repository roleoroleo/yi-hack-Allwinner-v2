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
 * A class for streaming data from a queue
 */

#ifndef _FRAMED_MEMORY_SOURCE_HH
#define _FRAMED_MEMORY_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include <rRTSPServer.h>

class VideoFramedMemorySource: public FramedSource {
public:
    static VideoFramedMemorySource* createNew(UsageEnvironment& env,
                                                int hNumber,
                                                output_queue *qBuffer,
                                                Boolean useTimeForPres,
                                                unsigned playTimePerFrame = 0);

    void seekToByteAbsolute(u_int64_t byteNumber, u_int64_t numBytesToStream = 0);
    void seekToByteRelative(int64_t offset, u_int64_t numBytesToStream = 0);
    static void doGetNextFrameTask(void *clientData);
    void doGetNextFrameEx();

protected:
    VideoFramedMemorySource(UsageEnvironment& env,
                                int hNumber,
                                output_queue *qBuffer,
                                Boolean useTimeForPres,
                                unsigned playTimePerFrame);
        // called only by createNew()

    virtual ~VideoFramedMemorySource();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();

private:
    int fHNumber;
    output_queue *fQBuffer;
    u_int64_t fCurIndex;
    Boolean fUseTimeForPres;
    unsigned fPlayTimePerFrame;
    unsigned fLastPlayTime;
    Boolean fLimitNumBytesToStream;
    u_int64_t fNumBytesToStream; // used iff "fLimitNumBytesToStream" is True
    Boolean fHaveStartedReading;
};

#endif
