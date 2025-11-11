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
 * A ServerMediaSubsession object
 */

#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_BC_HH
#define _ON_DEMAND_SERVER_MEDIA_SUBSESSION_BC_HH

#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#include "OnDemandServerMediaSubsession.hh"
#endif
#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _RTP_SINK_HH
#include "RTPSink.hh"
#endif
#ifndef _BASIC_UDP_SINK_HH
#include "BasicUDPSink.hh"
#endif
#ifndef _RTCP_HH
#include "RTCP.hh"
#endif

#ifndef _RTP_SOURCE_HH
#include "RTPSource.hh"
#endif
#ifndef _BASIC_UDP_SOURCE_HH
#include "BasicUDPSource.hh"
#endif
#ifndef _FILE_SINK_HH
#include "FileSink.hh"
#endif

class OnDemandServerMediaSubsession_BC: public ServerMediaSubsession {
public:
//  void setSubsessionAsBackChannel();

protected: // we're a virtual base class
  OnDemandServerMediaSubsession_BC(UsageEnvironment& env, Boolean reuseFirstSource,
				portNumBits initialPortNum = 6970,
				Boolean multiplexRTCPWithRTP = False);
  virtual ~OnDemandServerMediaSubsession_BC();

protected: // redefined virtual functions
  virtual char const* sdpLines(int addressFamily);
  virtual void getStreamParameters(unsigned clientSessionId,
				   struct sockaddr_storage const& clientAddress,
                                   Port const& clientRTPPort,
                                   Port const& clientRTCPPort,
				   int tcpSocketNum,
                                   unsigned char rtpChannelId,
                                   unsigned char rtcpChannelId,
				   TLSState* tlsState,
                                   struct sockaddr_storage& destinationAddress,
				   u_int8_t& destinationTTL,
                                   Boolean& isMulticast,
                                   Port& serverRTPPort,
                                   Port& serverRTCPPort,
                                   void*& streamToken);
  virtual void startStream(unsigned clientSessionId, void* streamToken,
			   TaskFunc* rtcpRRHandler,
			   void* rtcpRRHandlerClientData,
			   unsigned short& rtpSeqNum,
                           unsigned& rtpTimestamp,
			   ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                           void* serverRequestAlternativeByteHandlerClientData);
  virtual void pauseStream(unsigned clientSessionId, void* streamToken);
//  virtual void seekStream(unsigned clientSessionId, void* streamToken, double& seekNPT, double streamDuration, u_int64_t& numBytes);
//  virtual void seekStream(unsigned clientSessionId, void* streamToken, char*& absStart, char*& absEnd);
//  virtual void nullSeekStream(unsigned clientSessionId, void* streamToken,
//			      double streamEndTime, u_int64_t& numBytes);
//  virtual void setStreamScale(unsigned clientSessionId, void* streamToken, float scale);
//  virtual float getCurrentNPT(void* streamToken);
  virtual MediaSink* getStreamSink(void* streamToken);
//  virtual FramedSource* getStreamSource(void* streamToken);
  virtual void getRTPSinkandRTCP(void* streamToken,
				 RTPSink *& rtpSink, RTCPInstance *& rtcp);
//  virtual void deleteStream(unsigned clientSessionId, void*& streamToken);

protected: // new virtual functions, possibly redefined by subclasses
//  virtual char const* getAuxSDPLine(RTPSink* rtpSink,
//				    FramedSource* inputSource);
  virtual char const* getAuxSDPLineForBackChannel(MediaSink* mediaSink,
                                      RTPSource* rtpSource);
  virtual void seekStreamSource(FramedSource* inputSource, double& seekNPT, double streamDuration, u_int64_t& numBytes);
    // This routine is used to seek by relative (i.e., NPT) time.
    // "streamDuration", if >0.0, specifies how much data to stream, past "seekNPT".  (If <=0.0, all remaining data is streamed.)
    // "numBytes" returns the size (in bytes) of the data to be streamed, or 0 if unknown or unlimited.
  virtual void seekStreamSource(FramedSource* inputSource, char*& absStart, char*& absEnd);
    // This routine is used to seek by 'absolute' time.
    // "absStart" should be a string of the form "YYYYMMDDTHHMMSSZ" or "YYYYMMDDTHHMMSS.<frac>Z".
    // "absEnd" should be either NULL (for no end time), or a string of the same form as "absStart".
    // These strings may be modified in-place, or can be reassigned to a newly-allocated value (after delete[]ing the original).
  virtual void setStreamSourceScale(FramedSource* inputSource, float scale);
  virtual void setStreamSourceDuration(FramedSource* inputSource, double streamDuration, u_int64_t& numBytes);
  virtual void closeStreamSource(FramedSource* inputSource);
  virtual void closeStreamSink(MediaSink *outputSink);

protected: // new virtual functions, defined by all subclasses
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
					      unsigned& estBitrate) = 0;
      // "estBitrate" is the stream's estimated bitrate, in kbps
//  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
//				    unsigned char rtpPayloadTypeIfDynamic,
//				    FramedSource* inputSource) = 0;
  virtual MediaSink* createNewStreamDestination(unsigned clientSessionId,
		          unsigned& estBitrate);
      // "estBitrate" is the stream's estimated bitrate, in kbps
  virtual RTPSource* createNewRTPSource(Groupsock* rtpGroupsock,
		    unsigned char rtpPayloadTypeIfDynamic,
		    MediaSink* outputSink);				    

protected: // new virtual functions, may be redefined by a subclass:
  virtual Groupsock* createGroupsock(struct sockaddr_storage const& addr, Port port);
  virtual RTCPInstance* createRTCP(Groupsock* RTCPgs, unsigned totSessionBW, /* in kbps */
				   unsigned char const* cname, RTPSink* sink);

public:
  void multiplexRTCPWithRTP() { fMultiplexRTCPWithRTP = True; }
    // An alternative to passing the "multiplexRTCPWithRTP" parameter as True in the constructor

  void setRTCPAppPacketHandler(RTCPAppHandlerFunc* handler, void* clientData);
    // Sets a handler to be called if a RTCP "APP" packet arrives from any future client.
    // (Any current clients are not affected; any "APP" packets from them will continue to be
    // handled by whatever handler existed when the client sent its first RTSP "PLAY" command.)
    // (Call with (NULL, NULL) to remove an existing handler - for future clients only)

  void sendRTCPAppPacket(u_int8_t subtype, char const* name,
			 u_int8_t* appDependentData, unsigned appDependentDataSize);
    // Sends a custom RTCP "APP" packet to the most recent client (if "reuseFirstSource" was False),
    // or to all current clients (if "reuseFirstSource" was True).
    // The parameters correspond to their
    // respective fields as described in the RTP/RTCP definition (RFC 3550).
    // Note that only the low-order 5 bits of "subtype" are used, and only the first 4 bytes
    // of "name" are used.  (If "name" has fewer than 4 bytes, or is NULL,
    // then the remaining bytes are '\0'.)

protected:
//  void setSDPLinesFromRTPSink(RTPSink* rtpSink, FramedSource* inputSource,
//			      unsigned estBitrate);
      // used to implement "sdpLines()"
  void setSDPLinesFromMediaSink(RTPSource* rtpSource, MediaSink* mediaSink,
	          unsigned estBitrate);
  char* getRtpMapLine(RTPSource* rtpSource) const;

protected:
  char* fSDPLines;
  u_int8_t* fMIKEYStateMessage; // used if we're streaming SRTP
  unsigned fMIKEYStateMessageSize; // ditto
  HashTable* fDestinationsHashTable; // indexed by client session id

private:
//  Boolean fAreBackChannel;
  Boolean fReuseFirstSource;
  portNumBits fInitialPortNum;
  Boolean fMultiplexRTCPWithRTP;
  void* fLastStreamToken;
  char fCNAME[100]; // for RTCP
  RTCPAppHandlerFunc* fAppHandlerTask;
  void* fAppHandlerClientData;
  friend class StreamState_BC;
};


// A class that represents the state of an ongoing stream.  This is used only internally, in the implementation of
// "OnDemandServerMediaSubsession_BC", but we expose the definition here, in case subclasses of "OnDemandServerMediaSubsession_BC"
// want to access it.

class StreamState_BC {
public:
  StreamState_BC(OnDemandServerMediaSubsession_BC& master,
              Port const& serverRTPPort, Port const& serverRTCPPort,
	      //RTPSink* rtpSink, BasicUDPSink* udpSink,
	      unsigned totalBW, //FramedSource* mediaSource,
	      Groupsock* rtpGS, Groupsock* rtcpGS,
	      RTPSource* rtpSource, BasicUDPSource* udpSource, MediaSink* mediaSink); 
  virtual ~StreamState_BC();

  void startPlaying(Destinations* destinations, unsigned clientSessionId,
		    TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
		    ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
                    void* serverRequestAlternativeByteHandlerClientData);
  void pause();
  void sendRTCPAppPacket(u_int8_t subtype, char const* name,
			 u_int8_t* appDependentData, unsigned appDependentDataSize);
  void endPlaying(Destinations* destinations, unsigned clientSessionId);
  void reclaim();

  unsigned& referenceCount() { return fReferenceCount; }

  Port const& serverRTPPort() const { return fServerRTPPort; }
  RTPSource* rtpSource() const { return fRTPSource; }
  MediaSink* mediaSink() const {return fMediaSink; }
  Port const& serverRTCPPort() const { return fServerRTCPPort; }

//  RTPSink* rtpSink() const { return fRTPSink; }
  RTCPInstance* rtcpInstance() const { return fRTCPInstance; }

  float streamDuration() const { return fStreamDuration; }

//  FramedSource* mediaSource() const { return fMediaSource; }
  float& startNPT() { return fStartNPT; }

private:
  OnDemandServerMediaSubsession_BC& fMaster;
  Boolean fAreCurrentlyPlaying;
  unsigned fReferenceCount;

  Port fServerRTPPort, fServerRTCPPort;

//  RTPSink* fRTPSink;
//  BasicUDPSink* fUDPSink;

  float fStreamDuration;
  unsigned fTotalBW;
  RTCPInstance* fRTCPInstance;

//  FramedSource* fMediaSource;
  float fStartNPT; // initial 'normal play time'; reset after each seek

  Groupsock* fRTPgs;
  Groupsock* fRTCPgs;

  RTPSource*        fRTPSource;
  BasicUDPSource*   fUDPSource;
  MediaSink*         fMediaSink;
};

#endif
