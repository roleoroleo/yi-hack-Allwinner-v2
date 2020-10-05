/*
 * Copyright (c) 2020 roleo.
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
 * Dump h264 content from /dev/shm/fshare_frame_buffer and copy it to
 * a circular buffer.
 * Then send the circular buffer to live555.
 */

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#include "H264VideoCBMemoryServerMediaSubsession.hh"
#include "WAVAudioFifoServerMediaSubsession.hh"
#include "WAVAudioFifoSource.hh"
#include "StreamReplicator.hh"
#include "DummySink.hh"

#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#include "rRTSPServer.h"

unsigned char IDR[]               = {0x65, 0xB8};
unsigned char NAL_START[]         = {0x00, 0x00, 0x00, 0x01};
unsigned char IDR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88};
unsigned char PFR_START[]         = {0x00, 0x00, 0x00, 0x01, 0x41};
unsigned char SPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x67};
unsigned char PPS_START[]         = {0x00, 0x00, 0x00, 0x01, 0x68};
unsigned char SPS_COMMON[]        = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00};
unsigned char SPS_640X360[]       = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x14,
                                       0x96, 0x54, 0x05, 0x01, 0x7B, 0xCB, 0x37, 0x01,
                                       0x01, 0x01, 0x02};
unsigned char SPS_1920X1080[]     = {0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x00, 0x20,
                                       0x96, 0x54, 0x03, 0xC0, 0x11, 0x2F, 0x2C, 0xDC,
                                       0x04, 0x04, 0x04, 0x08};

//unsigned char *addr;                      /* Pointer to shared memory region (header) */
int debug;                                  /* Set to 1 to debug this .c */
int resolution;
int audio;
int port;
//unsigned char *buf_start;

//unsigned char *output_buffer = NULL;
//u_int64_t output_buffer_size = 0;
cb_input_buffer input_buffer;
cb_output_buffer output_buffer_low;
cb_output_buffer output_buffer_high;

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = True;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

void cb_dest_memcpy(cb_output_buffer *dest, unsigned char *src, size_t n)
{
    unsigned char *uc_dest = dest->write_index;

    if (uc_dest + n > dest->buffer + dest->size) {
        memcpy(uc_dest, src, dest->buffer + dest->size - uc_dest);
        memcpy(dest->buffer, src + (dest->buffer + dest->size - uc_dest), n - (dest->buffer + dest->size - uc_dest));
        dest->write_index = n + uc_dest - dest->size;
    } else {
        memcpy(uc_dest, src, n);
        dest->write_index += n;
    }
    if (dest->write_index == dest->buffer + dest->size) {
        dest->write_index = dest->buffer;
    }
}

void cb_memcpy(cb_output_buffer *dest, cb_input_buffer *src, size_t n)
{
    unsigned char *uc_src = src->read_index;

    if (uc_src + n > src->buffer + src->size) {
        cb_dest_memcpy(dest, uc_src, src->buffer + src->size - uc_src);
        cb_dest_memcpy(dest, src->buffer + src->offset, n - (src->buffer + src->size - uc_src));
        src->read_index = src->offset + n + uc_src - src->size;
    } else {
        cb_dest_memcpy(dest, uc_src, n);
        src->read_index += n;
    }
}

// The second argument is the circular buffer
int cb_memcmp(unsigned char *str1, unsigned char*str2, size_t n)
{
    int ret;

    if (str2 + n > input_buffer.buffer + input_buffer.size) {
        ret = memcmp(str1, str2, input_buffer.buffer + input_buffer.size - str2);
        if (ret != 0) return ret;
        ret = memcmp(str1 + (input_buffer.buffer + input_buffer.size - str2), input_buffer.buffer, n - (input_buffer.buffer + input_buffer.size - str2));
    } else {
        ret = memcmp(str1, str2, n);
    }

    return ret;
}

/* Locate a string in the circular buffer */
unsigned char *cb_memmem(unsigned char *src, int src_len, unsigned char *what, int what_len)
{
    unsigned char *p;
    unsigned char *buf = input_buffer.buffer + input_buffer.offset;
    int buf_size = input_buffer.size;

    if (src_len >= 0) {
        p = (unsigned char*) memmem(src, src_len, what, what_len);
    } else {
        // From src to the end of the buffer
        p = (unsigned char*) memmem(src, buf + buf_size - src, what, what_len);
        if (p == NULL) {
            // And from the start of the buffer size src_len
            p = (unsigned char*) memmem(buf, src + src_len - buf, what, what_len);
        }
    }
    return p;
}

unsigned char *cb_move(unsigned char *buf, int offset)
{
    buf += offset;
    if ((offset > 0) && (buf > input_buffer.buffer + input_buffer.size))
        buf -= (input_buffer.size - input_buffer.offset);
    if ((offset < 0) && (buf < input_buffer.buffer + input_buffer.offset))
        buf += (input_buffer.size - input_buffer.offset);

    return buf;
}

void *capture(void *ptr)
{
    unsigned char *addr;
    unsigned char *buf_idx_1, *buf_idx_2;
    unsigned char *buf_idx_w, *buf_idx_tmp;
    unsigned char *buf_idx_start = NULL;
    FILE *fFid;

    int frame_len = -1;
    int frame_res = -1;

    int i;
    cb_output_buffer *cb_current;
    int write_enable = 0;

    // Opening an existing file
    fFid = fopen(input_buffer.filename, "r");
    if ( fFid == NULL ) {
        fprintf(stderr, "could not open file %s\n", input_buffer.filename);
        free(output_buffer_low.buffer);
        free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }

    // Map file to memory
    addr = (unsigned char*) mmap(NULL, input_buffer.size, PROT_READ, MAP_SHARED, fileno(fFid), 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "error mapping file %s\n", input_buffer.filename);
        fclose(fFid);
        free(output_buffer_low.buffer);
        free(output_buffer_high.buffer);
        exit(EXIT_FAILURE);
    }
    input_buffer.buffer = addr;
    if (debug) fprintf(stderr, "mapping file %s, size %d, to %08x\n", input_buffer.filename, input_buffer.size, (unsigned int) addr);

    // Closing the file
    if (debug) fprintf(stderr, "closing the file %s\n", input_buffer.filename);
    fclose(fFid) ;

    buf_idx_1 = addr + input_buffer.offset;
    buf_idx_w = 0;

    if (debug) fprintf(stderr, "starting capture main loop\n");

    // Infinite loop
    while (1) {
        memcpy(&i, addr + 16, sizeof(i));
        buf_idx_w = addr + input_buffer.offset + i;
//        if (debug) fprintf(stderr, "buf_idx_w: %08x\n", (unsigned int) buf_idx_w);
        buf_idx_tmp = cb_memmem(buf_idx_1, buf_idx_w - buf_idx_1, NAL_START, sizeof(NAL_START));
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_1 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_1: %08x\n", (unsigned int) buf_idx_1);

        buf_idx_tmp = cb_memmem(buf_idx_1 + 1, buf_idx_w - (buf_idx_1 + 1), NAL_START, sizeof(NAL_START));
        if (buf_idx_tmp == NULL) {
            usleep(MILLIS_25);
            continue;
        } else {
            buf_idx_2 = buf_idx_tmp;
        }
//        if (debug) fprintf(stderr, "found buf_idx_2: %08x\n", (unsigned int) buf_idx_2);

        if (write_enable) {
            if (frame_res == RESOLUTION_LOW) {
                cb_current = &output_buffer_low;
            } else if (frame_res == RESOLUTION_HIGH) {
                cb_current = &output_buffer_high;
            } else {
                cb_current = NULL;
            }
            if (debug) fprintf(stderr, "frame_len: %d - cb_current->size: %d\n", frame_len, cb_current->size);

            if (cb_current != NULL) {
                if (frame_len > (signed) cb_current->size) {
                    fprintf(stderr, "frame size exceeds buffer size\n");
                } else {
                    pthread_mutex_lock(&(cb_current->mutex));
                    input_buffer.read_index = buf_idx_start;
//                    if (debug) fprintf(stderr, "frame_len: %d - frame_counter: %d - buffer_filled: %d - resolution: %d\n", frame_len, frame_counter,
//                                        (cb_current->write_index - cb_current->read_index + cb_current->size) % cb_current->size + frame_len, cb_current->resolution);
                    cb_memcpy(cb_current, &input_buffer, frame_len);
                    pthread_mutex_unlock(&(cb_current->mutex));
                }
            }
        }

        if (cb_memcmp(SPS_COMMON, buf_idx_1, sizeof(SPS_COMMON)) == 0) {
            // SPS frame
            write_enable = 1;
            buf_idx_1 = cb_move(buf_idx_1, - (6 + FRAME_HEADER_SIZE));
            if (buf_idx_1[17] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[17] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                write_enable = 0;
            }
            memcpy(&frame_len, buf_idx_1, 4);
            frame_len -= 6;                                                              // -6 only for SPS
            buf_idx_1 = cb_move(buf_idx_1, 6 + FRAME_HEADER_SIZE);
//            if (debug) fprintf(stderr, "SPS   detected - frame_len_prev: %d - frame_counter: %d - buffer_filled: %d\n", frame_len_prev, frame_counter,
//                                (output_buffer.write_index - output_buffer.read_index + output_buffer.size) % output_buffer.size + frame_len_prev);
            buf_idx_start = buf_idx_1;
        } else if ((cb_memcmp(PPS_START, buf_idx_1, sizeof(PPS_START)) == 0) ||
                        (cb_memcmp(IDR_START, buf_idx_1, sizeof(IDR_START)) == 0) ||
                        (cb_memcmp(PFR_START, buf_idx_1, sizeof(PFR_START)) == 0)) {
            // PPS, IDR and PFR frames
            write_enable = 1;
            buf_idx_1 = cb_move(buf_idx_1, -FRAME_HEADER_SIZE);
            if (buf_idx_1[17] == 8) {
                frame_res = RESOLUTION_LOW;
            } else if (buf_idx_1[17] == 4) {
                frame_res = RESOLUTION_HIGH;
            } else {
                write_enable = 0;
            }
            memcpy(&frame_len, buf_idx_1, 4);
            buf_idx_1 = cb_move(buf_idx_1, FRAME_HEADER_SIZE);
            buf_idx_start = buf_idx_1;
        } else {
            write_enable = 0;
        }

        buf_idx_1 = buf_idx_2;
    }

    // Unreacheable path

    // Unmap file from memory
    if (munmap(addr, input_buffer.size) == -1) {
        fprintf(stderr, "error munmapping file");
    } else {
        if (debug) fprintf(stderr, "unmapping file %s, size %d, from %08x\n", BUFFER_FILE, input_buffer.size, (unsigned int) addr);
    }

    return NULL;
}

StreamReplicator* startReplicatorStream(const char* inputAudioFileName, Boolean convertToULaw) {
    // Create a single WAVAudioFifo source that will be replicated for mutliple streams
    WAVAudioFifoSource* wavSource = WAVAudioFifoSource::createNew(*env, inputAudioFileName);
    if (wavSource == NULL) {
        *env << "Failed to create Fifo Source \n";
    }

    // Optionally convert to uLaw pcm
    FramedSource* resultSource;
    if (convertToULaw) {
        resultSource = uLawFromPCMAudioSource::createNew(*env, wavSource, 1/*little-endian*/);
    } else {
        resultSource = EndianSwap16::createNew(*env, wavSource);
    }

    // Create and start the replicator that will be given to each subsession
    StreamReplicator* replicator = StreamReplicator::createNew(*env, resultSource);

    // Begin by creating an input stream from our replicator:
    FramedSource* source = replicator->createStreamReplica();

    // Then create a 'dummy sink' object to receive the replica stream:
    MediaSink* sink = DummySink::createNew(*env, "dummy");

    // Now, start playing, feeding the sink object from the source:
    sink->startPlaying(*source, NULL, NULL);

    return replicator;
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms, char const* streamName, int audio)
{
    char* url = rtspServer->rtspURL(sms);
    UsageEnvironment& env = rtspServer->envir();
    env << "\n\"" << streamName << "\" stream, from memory\n";
    if (audio)
        env << "Audio enabled\n";
    env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;
}

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-r RES] [-p PORT] [-d]\n\n", progname);
    fprintf(stderr, "\t-r RES,  --resolution RES\n");
    fprintf(stderr, "\t\tset resolution: low, high or both (default high)\n");
    fprintf(stderr, "\t-a AUDIO,  --audio AUDIO\n");
    fprintf(stderr, "\t\tset audio: yes or no (default yes)\n");
    fprintf(stderr, "\t-p PORT, --port PORT\n");
    fprintf(stderr, "\t\tset TCP port (default 554)\n");
    fprintf(stderr, "\t-d,      --debug\n");
    fprintf(stderr, "\t\tenable debug\n");
    fprintf(stderr, "\t-h,      --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char *str;
    int nm;
    char user[65];
    char pwd[65];
    int pth_ret;
    int c;
    char *endptr;

    pthread_t capture_thread;

    Boolean convertToULaw = True;
    char const* inputAudioFileName = "/tmp/audio_fifo";
    struct stat stat_buffer;

    // Setting default
    resolution = RESOLUTION_HIGH;
    audio = 1;
    port = 554;
    debug = 0;

    while (1) {
        static struct option long_options[] =
        {
            {"resolution",  required_argument, 0, 'r'},
            {"audio",  required_argument, 0, 'a'},
            {"port",  required_argument, 0, 'p'},
            {"debug",  no_argument, 0, 'd'},
            {"help",  no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "r:a:p:dh",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            if (strcasecmp("low", optarg) == 0) {
                resolution = RESOLUTION_LOW;
            } else if (strcasecmp("high", optarg) == 0) {
                resolution = RESOLUTION_HIGH;
            } else if (strcasecmp("both", optarg) == 0) {
                resolution = RESOLUTION_BOTH;
            }
            break;

        case 'a':
            if (strcasecmp("no", optarg) == 0) {
                audio = 0;
            }
            break;

        case 'p':
            errno = 0;    /* To distinguish success/failure after call */
            port = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN)) || (errno != 0 && port == 0)) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            fprintf (stderr, "debug on\n");
            debug = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return -1;
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    // Get parameters from environment
    str = getenv("RRTSP_RES");
    if (str != NULL) {
        if (strcasecmp("low", str) == 0) {
            resolution = RESOLUTION_LOW;
        } else if (strcasecmp("high", str) == 0) {
            resolution = RESOLUTION_HIGH;
        } else if (strcasecmp("both", str) == 0) {
            resolution = RESOLUTION_BOTH;
        }
    }

    str = getenv("RRTSP_AUDIO");
    if (str != NULL) {
        if (strcasecmp("no", str) == 0) {
            audio = 0;
        }
    }

    str = getenv("RRTSP_PORT");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm >= 0)) {
        port = nm;
    }

    str = getenv("RRTSP_DEBUG");
    if ((str != NULL) && (sscanf (str, "%i", &nm) == 1) && (nm == 1)) {
        debug = nm;
    }

    memset(user, 0, sizeof(user));
    str = getenv("RRTSP_USER");
    if ((str != NULL) && (strlen(str) < sizeof(user))) {
        strcpy(user, str);
    }

    memset(pwd, 0, sizeof(pwd));
    str = getenv("RRTSP_PWD");
    if ((str != NULL) && (strlen(str) < sizeof(pwd))) {
        strcpy(pwd, str);
    }

    // If fifo doesn't exist, disable audio
    if (stat (inputAudioFileName, &stat_buffer) != 0) {
        audio = 0;
    }

    // Fill input and output buffer struct
    strcpy(input_buffer.filename, BUFFER_FILE);
    input_buffer.size = BUF_SIZE;
    input_buffer.offset = BUF_OFFSET;

    output_buffer_low.resolution = RESOLUTION_LOW;
    output_buffer_low.size = OUTPUT_BUFFER_SIZE_LOW;
    output_buffer_low.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_LOW * sizeof(unsigned char));
    output_buffer_low.read_index = output_buffer_low.buffer;
    output_buffer_low.write_index = output_buffer_low.buffer;
    if (output_buffer_low.buffer == NULL) {
        fprintf(stderr, "could not alloc memory\n");
        exit(EXIT_FAILURE);
    }

    output_buffer_high.resolution = RESOLUTION_HIGH;
    output_buffer_high.size = OUTPUT_BUFFER_SIZE_HIGH;
    output_buffer_high.buffer = (unsigned char *) malloc(OUTPUT_BUFFER_SIZE_HIGH * sizeof(unsigned char));
    output_buffer_high.read_index = output_buffer_high.buffer;
    output_buffer_high.write_index = output_buffer_high.buffer;
    if (output_buffer_high.buffer == NULL) {
        fprintf(stderr, "could not alloc memory\n");
        exit(EXIT_FAILURE);
    }

    // Start capture thread
    if (pthread_mutex_init(&(output_buffer_low.mutex), NULL) != 0) { 
        *env << "Failed to create mutex\n";
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&(output_buffer_high.mutex), NULL) != 0) { 
        *env << "Failed to create mutex\n";
        exit(EXIT_FAILURE);
    }
    pth_ret = pthread_create(&capture_thread, NULL, capture, (void*) NULL);
    if (pth_ret != 0) {
        *env << "Failed to create capture thread\n";
        exit(EXIT_FAILURE);
    }
    pthread_detach(capture_thread);

    sleep(2);

    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    UserAuthenticationDatabase* authDB = NULL;

    if ((user[0] != '\0') && (pwd[0] != '\0')) {
        // To implement client access control to the RTSP server, do the following:
        authDB = new UserAuthenticationDatabase;
        authDB->addUserRecord(user, pwd);
        // Repeat the above with each <username>, <password> that you wish to allow
        // access to the server.
    }

    StreamReplicator* replicator;
    if (audio) {
        // Create and start the replicator that will be given to each subsession
        replicator = startReplicatorStream(inputAudioFileName, convertToULaw);
    }

    // Create the RTSP server:
    RTSPServer* rtspServer = RTSPServer::createNew(*env, port, authDB);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    char const* descriptionString = "Session streamed by \"rRTSPServer\"";

    // Set up each of the possible streams that can be served by the
    // RTSP server.  Each such stream is implemented using a
    // "ServerMediaSession" object, plus one or more
    // "ServerMediaSubsession" objects for each audio/video substream.

    // A H.264 video elementary stream:
    if ((resolution == RESOLUTION_HIGH) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_0.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_high
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_high->addSubsession(H264VideoCBMemoryServerMediaSubsession
                                   ::createNew(*env, &output_buffer_high, reuseFirstSource));
        if (audio) {
            sms_high->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, convertToULaw));
        }
        rtspServer->addServerMediaSession(sms_high);

        announceStream(rtspServer, sms_high, streamName, audio);
    }

    // A H.264 video elementary stream:
    if ((resolution == RESOLUTION_LOW) || (resolution == RESOLUTION_BOTH))
    {
        char const* streamName = "ch0_1.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_low
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_low->addSubsession(H264VideoCBMemoryServerMediaSubsession
                                   ::createNew(*env, &output_buffer_low, reuseFirstSource));
        if (audio) {
            sms_low->addSubsession(WAVAudioFifoServerMediaSubsession
                                       ::createNew(*env, replicator, reuseFirstSource, convertToULaw));
        }
        rtspServer->addServerMediaSession(sms_low);

        announceStream(rtspServer, sms_low, streamName, audio);
    }

    // A PCM audio elementary stream:
    if (audio)
    {
        char const* streamName = "ch0_2.h264";

        // First, make sure that the RTPSinks' buffers will be large enough to handle the huge size of DV frames (as big as 288000).
        OutPacketBuffer::maxSize = 300000;

        ServerMediaSession* sms_audio
            = ServerMediaSession::createNew(*env, streamName, streamName,
                                              descriptionString);
        sms_audio->addSubsession(WAVAudioFifoServerMediaSubsession
                                   ::createNew(*env, replicator, reuseFirstSource, convertToULaw));
        rtspServer->addServerMediaSession(sms_audio);

        announceStream(rtspServer, sms_audio, streamName, audio);
    }

    // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
    // Try first with the default HTTP port (80), and then with the alternative HTTP
    // port numbers (8000 and 8080).
/*
    if (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080)) {
        *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
    } else {
        *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
    }
*/

    env->taskScheduler().doEventLoop(); // does not return

    pthread_mutex_destroy(&(output_buffer_low.mutex));
    pthread_mutex_destroy(&(output_buffer_high.mutex));

    // Free buffers
    free(output_buffer_low.buffer);
    free(output_buffer_high.buffer);

    return 0; // only to prevent compiler warning
}
