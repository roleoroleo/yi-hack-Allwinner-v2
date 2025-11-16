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

#include "OnDemandServerMediaSubsession_BC.hh"
#include <GroupsockHelper.hh>

OnDemandServerMediaSubsession_BC
::OnDemandServerMediaSubsession_BC(UsageEnvironment& env,
				   Boolean reuseFirstSource,
				   portNumBits initialPortNum,
				   Boolean multiplexRTCPWithRTP)
    : ServerMediaSubsession(env),
      fSDPLines(NULL), fMIKEYStateMessage(NULL), fMIKEYStateMessageSize(0),
      fReuseFirstSource(reuseFirstSource),
      fMultiplexRTCPWithRTP(multiplexRTCPWithRTP), fLastStreamToken(NULL),
      fAppHandlerTask(NULL), fAppHandlerClientData(NULL) {

    fDestinationsHashTable = HashTable::create(ONE_WORD_HASH_KEYS);
    if (fMultiplexRTCPWithRTP) {
        fInitialPortNum = initialPortNum;
    } else {
        // Make sure RTP ports are even-numbered:
        fInitialPortNum = (initialPortNum+1)&~1;
    }
    gethostname(fCNAME, sizeof fCNAME);
    fCNAME[sizeof fCNAME-1] = '\0'; // just in case
}

OnDemandServerMediaSubsession_BC::~OnDemandServerMediaSubsession_BC() {
    delete[] fMIKEYStateMessage;
    delete[] fSDPLines;

    // Clean out the destinations hash table:
    while (1) {
        Destinations* destinations
          = (Destinations*)(fDestinationsHashTable->RemoveNext());
        if (destinations == NULL) break;
        delete destinations;
    }
    delete fDestinationsHashTable;
}

char const* OnDemandServerMediaSubsession_BC::sdpLines(int addressFamily) {
    if (fSDPLines == NULL) {
        unsigned estBitrate;

        // The file sink should bind to specific type
        // We can know the type once we invoke the createNewStreamDestination() implemented by descendant
        MediaSink* mediaSink = createNewStreamDestination(0, estBitrate);
        if (mediaSink == NULL) return NULL; // file not found

        Groupsock* dummyGroupsock = createGroupsock(nullAddress(addressFamily), 0);
        unsigned char rtpPayloadType = 96 + trackNumber() - 1; // if dynamic
        RTPSource* dummyRTPSource = createNewRTPSource(dummyGroupsock, rtpPayloadType, mediaSink);
        if (dummyRTPSource != NULL) {
          //if (dummyRTPSource->estimatedBitrate() > 0) estBitrate = dummyRTPSource->estimatedBitrate();
          setSDPLinesFromMediaSink(dummyRTPSource, mediaSink, estBitrate);
          closeStreamSource(dummyRTPSource);
        }
        delete dummyGroupsock;
        Medium::close(mediaSink);
    }

    return fSDPLines;
}

void OnDemandServerMediaSubsession_BC
::getStreamParameters(unsigned clientSessionId,
		      struct sockaddr_storage const& clientAddress,
		      Port const& clientRTPPort,
		      Port const& clientRTCPPort,
		      int tcpSocketNum,
		      unsigned char rtpChannelId,
		      unsigned char rtcpChannelId,
		      TLSState* tlsState,
		      struct sockaddr_storage& destinationAddress,
		      u_int8_t& /*destinationTTL*/,
		      Boolean& isMulticast,
		      Port& serverRTPPort,
		      Port& serverRTCPPort,
		      void*& streamToken) {

    if (addressIsNull(destinationAddress)) {
        // normal case - use the client address as the destination address:
        destinationAddress = clientAddress;
    }
    isMulticast = False;

    if (fLastStreamToken != NULL && fReuseFirstSource) {
        // Special case: Rather than creating a new 'StreamState_BC',
        // we reuse the one that we've already created:
        serverRTPPort = ((StreamState_BC*)fLastStreamToken)->serverRTPPort();
        serverRTCPPort = ((StreamState_BC*)fLastStreamToken)->serverRTCPPort();
        ++((StreamState_BC*)fLastStreamToken)->referenceCount();
        streamToken = fLastStreamToken;
    } else {
        // Normal case: Create a new media source:
        unsigned streamBitrate = 0;
        MediaSink* mediaSink = NULL;
        mediaSink = createNewStreamDestination(clientSessionId, streamBitrate);
        Groupsock* rtpGroupsock = NULL;
        Groupsock* rtcpGroupsock = NULL;
        RTPSource* rtpSource = NULL;
        BasicUDPSource* udpSource = NULL;

        if (clientRTPPort.num() != 0 || tcpSocketNum >= 0) { // Normal case: Create destinations
            portNumBits serverPortNum;
            if (clientRTCPPort.num() == 0) {
                // We're streaming raw UDP (not RTP). Create a single groupsock:
                NoReuse dummy(envir()); // ensures that we skip over ports that are already in use
                for (serverPortNum = fInitialPortNum; ; ++serverPortNum) {
                    serverRTPPort = serverPortNum;
                    rtpGroupsock = createGroupsock(nullAddress(destinationAddress.ss_family), serverRTPPort);
                    if (rtpGroupsock->socketNum() >= 0) break; // success
                }

                udpSource = BasicUDPSource::createNew(envir(), rtpGroupsock);
            } else {
                // Normal case: We're streaming RTP (over UDP or TCP).  Create a pair of
                // groupsocks (RTP and RTCP), with adjacent port numbers (RTP port number even).
                // (If we're multiplexing RTCP and RTP over the same port number, it can be odd or even.)
                NoReuse dummy(envir()); // ensures that we skip over ports that are already in use
                for (portNumBits serverPortNum = fInitialPortNum; ; ++serverPortNum) {
                    serverRTPPort = serverPortNum;
                    rtpGroupsock = createGroupsock(nullAddress(destinationAddress.ss_family), serverRTPPort);
                    if (rtpGroupsock->socketNum() < 0) {
                        delete rtpGroupsock;
                        continue; // try again
                    }

                    if (fMultiplexRTCPWithRTP) {
                        // Use the RTP 'groupsock' object for RTCP as well:
                        serverRTCPPort = serverRTPPort;
                        rtcpGroupsock = rtpGroupsock;
                    } else {
                        // Create a separate 'groupsock' object (with the next (odd) port number) for RTCP:
                        serverRTCPPort = ++serverPortNum;
                        rtcpGroupsock = createGroupsock(nullAddress(destinationAddress.ss_family), serverRTCPPort);
                        if (rtcpGroupsock->socketNum() < 0) {
                            delete rtpGroupsock;
                            delete rtcpGroupsock;
                            continue; // try again
                        }
                    }

                    break; // success
                }

                unsigned char rtpPayloadType = 96 + trackNumber()-1; // if dynamic
                rtpSource = mediaSink == NULL ? NULL
                  : createNewRTPSource(rtpGroupsock, rtpPayloadType, mediaSink);
            }

            // Turn off the destinations for each groupsock.  They'll get set later
            // (unless TCP is used instead):
            if (rtpGroupsock != NULL) rtpGroupsock->removeAllDestinations();
            if (rtcpGroupsock != NULL) rtcpGroupsock->removeAllDestinations();

            if (rtpGroupsock != NULL) {
                // Try to use a big send buffer for RTP -  at least 0.1 second of
                // specified bandwidth and at least 50 KB
                unsigned rtpBufSize = streamBitrate * 25 / 2; // 1 kbps * 0.1 s = 12.5 bytes
                if (rtpBufSize < 50 * 1024) rtpBufSize = 50 * 1024;
                increaseSendBufferTo(envir(), rtpGroupsock->socketNum(), rtpBufSize);
            }
        }

        // Set up the state of the stream.  The stream will get started later:
        streamToken = fLastStreamToken
          = new StreamState_BC(*this, serverRTPPort, serverRTCPPort, //rtpSink, udpSink,
			       streamBitrate, //mediaSource,
			       rtpGroupsock, rtcpGroupsock,
			       rtpSource, udpSource, mediaSink);
    }

    // Record these destinations as being for this client session id:
    Destinations* destinations;
    if (tcpSocketNum < 0) { // UDP
        destinations = new Destinations(destinationAddress, clientRTPPort, clientRTCPPort);
    } else { // TCP
        destinations = new Destinations(tcpSocketNum, rtpChannelId, rtcpChannelId, tlsState);
    }
    fDestinationsHashTable->Add((char const*)clientSessionId, destinations);
}

void OnDemandServerMediaSubsession_BC::startStream(unsigned clientSessionId,
					void* streamToken,
					TaskFunc* rtcpRRHandler,
					void* rtcpRRHandlerClientData,
					unsigned short& rtpSeqNum,
					unsigned& rtpTimestamp,
					ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
					void* serverRequestAlternativeByteHandlerClientData) {

    StreamState_BC* streamState = (StreamState_BC*)streamToken;
    Destinations* destinations
      = (Destinations*)(fDestinationsHashTable->Lookup((char const*)clientSessionId));
    if (streamState != NULL) {
        streamState->startPlaying(destinations, clientSessionId,
				  rtcpRRHandler, rtcpRRHandlerClientData,
				  serverRequestAlternativeByteHandler, serverRequestAlternativeByteHandlerClientData);
        RTPSource* rtpSource = streamState->rtpSource(); // alias
        if (rtpSource != NULL) {
            rtpSeqNum = rtpSource->curPacketRTPSeqNum();
            rtpTimestamp = rtpSource->curPacketRTPTimestamp();
        }
    }
}

void OnDemandServerMediaSubsession_BC::pauseStream(unsigned /*clientSessionId*/,
						   void* streamToken) {

    // Pausing isn't allowed if multiple clients are receiving data from
    // the same source:
    if (fReuseFirstSource) return;

    StreamState_BC* streamState = (StreamState_BC*)streamToken;
    if (streamState != NULL) streamState->pause();
}

MediaSink* OnDemandServerMediaSubsession_BC::getStreamSink(void* streamToken) {

    if (streamToken == NULL) return NULL;

    StreamState_BC* streamState = (StreamState_BC*)streamToken;
    return streamState->mediaSink();
}

void OnDemandServerMediaSubsession_BC
::getRTPSinkandRTCP(void* streamToken,
		    RTPSink *& rtpSink, RTCPInstance *& rtcp) {
    rtpSink = NULL;
    rtcp = NULL;
}

char const* OnDemandServerMediaSubsession_BC
::getAuxSDPLineForBackChannel(MediaSink* mediaSink, RTPSource* rtpSource)
{
    return NULL;
    //return mediaSink == NULL ? NULL : mediaSink->auxSDPLine();
}

void OnDemandServerMediaSubsession_BC::seekStreamSource(FramedSource* /*inputSource*/,
							double& /*seekNPT*/,
							double /*streamDuration*/,
							u_int64_t& numBytes) {

    // Default implementation: Do nothing
    numBytes = 0;
}

void OnDemandServerMediaSubsession_BC::seekStreamSource(FramedSource* /*inputSource*/,
							char*& absStart, char*& absEnd) {

    // Default implementation: do nothing (but delete[] and assign "absStart" and "absEnd" to NULL, to show that we don't handle this)
    delete[] absStart; absStart = NULL;
    delete[] absEnd; absEnd = NULL;
}

void OnDemandServerMediaSubsession_BC
::setStreamSourceScale(FramedSource* /*inputSource*/, float /*scale*/) {

    // Default implementation: Do nothing
}

void OnDemandServerMediaSubsession_BC
::setStreamSourceDuration(FramedSource* /*inputSource*/, double /*streamDuration*/, u_int64_t& numBytes) {

    // Default implementation: Do nothing
    numBytes = 0;
}

void OnDemandServerMediaSubsession_BC::closeStreamSource(FramedSource *inputSource) {

    Medium::close(inputSource);
}

void OnDemandServerMediaSubsession_BC::closeStreamSink(MediaSink *outputSink) {

    Medium::close(outputSink);
}

Groupsock* OnDemandServerMediaSubsession_BC
::createGroupsock(struct sockaddr_storage const& addr, Port port) {

    // Default implementation; may be redefined by subclasses:
    return new Groupsock(envir(), addr, port, 255);
}

RTCPInstance* OnDemandServerMediaSubsession_BC
::createRTCP(Groupsock* RTCPgs, unsigned totSessionBW, /* in kbps */
	     unsigned char const* cname, RTPSink* sink) {

    // Default implementation; may be redefined by subclasses:
    return RTCPInstance::createNew(envir(), RTCPgs, totSessionBW, cname, sink, NULL/*we're a server*/);
}

void OnDemandServerMediaSubsession_BC
::setRTCPAppPacketHandler(RTCPAppHandlerFunc* handler, void* clientData) {

    fAppHandlerTask = handler;
    fAppHandlerClientData = clientData;
}

void OnDemandServerMediaSubsession_BC
::sendRTCPAppPacket(u_int8_t subtype, char const* name,
		    u_int8_t* appDependentData, unsigned appDependentDataSize) {

    StreamState_BC* streamState = (StreamState_BC*)fLastStreamToken;
    if (streamState != NULL) {
        streamState->sendRTCPAppPacket(subtype, name, appDependentData, appDependentDataSize);
    }
}

char* OnDemandServerMediaSubsession_BC::getRtpMapLine(RTPSource* rtpSource) const {

    // assume numChannels = 1;
    int vNumChannels=1;
    if (rtpSource->rtpPayloadFormat() >= 96) { // the payload format type is dynamic
        char* encodingParamsPart;
        if (vNumChannels != 1) {
            encodingParamsPart = new char[1 + 20 /* max int len */];
            sprintf(encodingParamsPart, "/%d", vNumChannels);
        } else {
          encodingParamsPart = strDup("");
        }
        char const* const rtpmapFmt = "a=rtpmap:%d %s/%d%s\r\n";
        unsigned rtpmapFmtSize = strlen(rtpmapFmt)
          + 3 /* max char len */ + strlen("mpeg4-generic")
          + 20 /* max int len */ + strlen(encodingParamsPart);
        char* rtpmapLine = new char[rtpmapFmtSize];
        sprintf(rtpmapLine, rtpmapFmt,
            rtpSource->rtpPayloadFormat(), "mpeg4-generic",
            rtpSource->timestampFrequency(), encodingParamsPart);
        delete[] encodingParamsPart;

        return rtpmapLine;
    } else {
        // The payload format is staic, so there's no "a=rtpmap:" line:
        return strDup("");
    }
}

void OnDemandServerMediaSubsession_BC
::setSDPLinesFromMediaSink(RTPSource* rtpSource, MediaSink* mediaSink, unsigned estBitrate) {

    if (rtpSource == NULL) {
        printf("%s line:%d\n", __FUNCTION__, __LINE__);
        return;
    }
    char const* mediaType = "audio";
    unsigned char rtpPayloadType = rtpSource->rtpPayloadFormat();
    struct sockaddr_storage const& addressForSDP = rtpSource->RTPgs()->groupAddress();
    portNumBits portNumForSDP = ntohs(rtpSource->RTPgs()->port().num());

    AddressString ipAddressStr(addressForSDP);

    char* rtpmapLine = getRtpMapLine(rtpSource);
    char const* rangeLine = rangeSDPLine();

    // TODO: fix me
    //char const* auxSDPLine = "a=fmtp:96 streamtype=5;\r\n";
    char const* auxSDPLine = getAuxSDPLineForBackChannel(mediaSink, rtpSource);

    if (auxSDPLine == NULL) auxSDPLine = "";

    char const* const sdpFmt =
        "m=%s %u RTP/AVP %d\r\n"
        "c=IN IP4 %s\r\n"
        "b=AS:%u\r\n"
        "%s"
        "%s"
        "%s"
        "a=sendonly\r\n"
        "a=control:%s\r\n";
    unsigned sdpFmtSize = strlen(sdpFmt)
        + strlen(mediaType) + 5 /* max short len */ + 3 /* max char len */
        + strlen(ipAddressStr.val())
        + 20 /* max int len */
        + strlen(rtpmapLine)
        + strlen(rangeLine)
        + strlen(auxSDPLine)
        + strlen(trackId());
    char* sdpLines = new char[sdpFmtSize];
    sprintf(sdpLines, sdpFmt,
        mediaType, // m= <media>
        portNumForSDP, // m= <port>
        rtpPayloadType, // m= <fmt list>
        ipAddressStr.val(), // c= address
        estBitrate, // b=AS:<bandwidth>
        rtpmapLine, // a=rtpmap:... (if present)
        rangeLine, // a=range:... (if present)
        auxSDPLine, // optional extra SDP line
        trackId()); // a=control:<track-id>
    delete[] (char*)rangeLine; delete[] rtpmapLine;

    fSDPLines = strDup(sdpLines);
    delete[] sdpLines;  
}

MediaSink* OnDemandServerMediaSubsession_BC::createNewStreamDestination(unsigned clientSessionId,
                                                                       unsigned& estBitrate) {
    return NULL;
}

// "estBitrate" is the stream's estimated bitrate, in kbps
RTPSource* OnDemandServerMediaSubsession_BC::createNewRTPSource(Groupsock* rtpGroupsock,
                                                                             unsigned char rtpPayloadTypeIfDynamic,
                                                                             MediaSink* outputSink)
{
  return NULL;
}


////////// StreamState_BC implementation //////////

static void afterPlayingStreamState_BC(void* clientData) {
    StreamState_BC* streamState = (StreamState_BC*)clientData;
    if (streamState->streamDuration() == 0.0) {
        // When the input stream ends, tear it down.  This will cause a RTCP "BYE"
        // to be sent to each client, teling it that the stream has ended.
        // (Because the stream didn't have a known duration, there was no other
        //  way for clients to know when the stream ended.)
        streamState->reclaim();
    }
    // Otherwise, keep the stream alive, in case a client wants to
    // subsequently re-play the stream starting from somewhere other than the end.
    // (This can be done only on streams that have a known duration.)
}

StreamState_BC::StreamState_BC(OnDemandServerMediaSubsession_BC& master,
                         Port const& serverRTPPort, Port const& serverRTCPPort,
			 unsigned totalBW, Groupsock* rtpGS,
			 Groupsock* rtcpGS, RTPSource* rtpSource,
			 BasicUDPSource* udpSource, MediaSink* mediaSink)
    : fMaster(master), fAreCurrentlyPlaying(False), fReferenceCount(1),
      fServerRTPPort(serverRTPPort), fServerRTCPPort(serverRTCPPort),
      fStreamDuration(master.duration()), fTotalBW(totalBW),
      fRTCPInstance(NULL) /* created later */, fStartNPT(0.0), fRTPgs(rtpGS), fRTCPgs(rtcpGS),
      fRTPSource(rtpSource), fUDPSource(udpSource), fMediaSink(mediaSink) {
}

StreamState_BC::~StreamState_BC() {
    reclaim();
}

void StreamState_BC
::startPlaying(Destinations* dests, unsigned clientSessionId,
	       TaskFunc* rtcpRRHandler, void* rtcpRRHandlerClientData,
	       ServerRequestAlternativeByteHandler* serverRequestAlternativeByteHandler,
	       void* serverRequestAlternativeByteHandlerClientData) {
    if (dests == NULL) return;

    // TODO: send SR or RR?? we may change to send RR for backchannel server
    if (fRTCPInstance == NULL && fRTPSource != NULL) {

        // Create (and start) a 'RTCP instance' for this RTP sink:
        fRTCPInstance
          = RTCPInstance::createNew(fRTPSource->envir(), fRTCPgs,
            fTotalBW, (unsigned char*)fMaster.fCNAME,
            NULL/* RTPSink */, fRTPSource /* RTPSource */ /* we're a server with backchannel */);
            // Note: This starts RTCP running automatically
    }

    if (dests->isTCP) {
        // Change RTP and RTCP to use the TCP socket instead of UDP:
        if (fRTPSource != NULL) {
            // TODO: check here
            //fRTPSource->addStreamSocket(dests->tcpSocketNum, dests->rtpChannelId);
            fRTPSource->setStreamSocket(dests->tcpSocketNum, dests->rtpChannelId, dests->tlsState);
            RTPInterface::setServerRequestAlternativeByteHandler(fRTPSource->envir(), dests->tcpSocketNum,
              serverRequestAlternativeByteHandler, serverRequestAlternativeByteHandlerClientData);
            // So that we continue to handle RTSP commands from the client
        }
        if (fRTCPInstance != NULL) {
            fRTCPInstance->addStreamSocket(dests->tcpSocketNum, dests->rtcpChannelId, dests->tlsState);
            struct sockaddr_storage tcpSocketNumAsAddress; // hack
            tcpSocketNumAsAddress.ss_family = AF_INET;
            ((sockaddr_in&)tcpSocketNumAsAddress).sin_addr.s_addr = dests->tcpSocketNum;
            fRTCPInstance->setSpecificRRHandler(tcpSocketNumAsAddress, dests->rtcpChannelId,
					    rtcpRRHandler, rtcpRRHandlerClientData);
        }
    } else {
        // Tell the RTP and RTCP 'groupsocks' about this destination
        // (in case they don't already have it):
        if (fRTPgs != NULL) fRTPgs->addDestination(dests->addr, dests->rtpPort, clientSessionId);
        if (fRTCPgs != NULL) fRTCPgs->addDestination(dests->addr, dests->rtcpPort, clientSessionId);
        if (fRTCPInstance != NULL) {
            fRTCPInstance->setSpecificRRHandler(dests->addr, dests->rtcpPort,
		  rtcpRRHandler, rtcpRRHandlerClientData);
        }
    }

    if (fRTCPInstance != NULL) {
        // Hack: Send an initial RTCP "SR" packet, before the initial RTP packet, so that receivers will (likely) be able to
        // get RTCP-synchronized presentation times immediately:
        fRTCPInstance->sendReport();
    }

    if (!fAreCurrentlyPlaying && fMediaSink != NULL) {
        if (fRTPSource != NULL) {
            fMediaSink->startPlaying(*fRTPSource, afterPlayingStreamState_BC, this);
            fAreCurrentlyPlaying = True;
        } else if (fUDPSource != NULL) {
            fMediaSink->startPlaying(*fUDPSource, afterPlayingStreamState_BC, this);
            fAreCurrentlyPlaying = True;
        }
    }
}

void StreamState_BC::pause() {
    if (fRTPSource != NULL) fMediaSink->stopPlaying();
    if (fUDPSource != NULL) fMediaSink->stopPlaying();
    fAreCurrentlyPlaying = False;
}

void StreamState_BC::sendRTCPAppPacket(u_int8_t subtype, char const* name,
				    u_int8_t* appDependentData, unsigned appDependentDataSize) {
    if (fRTCPInstance != NULL) {
        fRTCPInstance->sendAppPacket(subtype, name, appDependentData, appDependentDataSize);
    }
}

void StreamState_BC::reclaim() {
    // Delete allocated media objects
    Medium::close(fRTCPInstance) /* will send a RTCP BYE */; fRTCPInstance = NULL;

    if (fMaster.fLastStreamToken == this) fMaster.fLastStreamToken = NULL;

    fMaster.closeStreamSink(fMediaSink); fMediaSink = NULL;
    Medium::close(fRTPSource); fRTPSource = NULL;
    Medium::close(fUDPSource); fUDPSource = NULL;

    delete fRTPgs;
    if (fRTCPgs != fRTPgs) delete fRTCPgs;
    fRTPgs = NULL; fRTCPgs = NULL;
}
