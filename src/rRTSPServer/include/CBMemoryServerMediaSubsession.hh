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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from circular buffer.
// C++ header

#ifndef _CB_MEMORY_SERVER_MEDIA_SUBSESSION_HH
#define _CB_MEMORY_SERVER_MEDIA_SUBSESSION_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#include "OnDemandServerMediaSubsession.hh"
#endif

#include "rRTSPServer.h"

class CBMemoryServerMediaSubsession: public OnDemandServerMediaSubsession {
protected: // we're a virtual base class
    CBMemoryServerMediaSubsession(UsageEnvironment& env,
                            cb_output_buffer *cbBuffer,
                            Boolean reuseFirstSource);
    virtual ~CBMemoryServerMediaSubsession();

protected:
    cb_output_buffer *fBuffer;
};

#endif
