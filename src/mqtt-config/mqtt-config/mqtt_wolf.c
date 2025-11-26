#ifndef USE_MOSQUITTO
#include "mqtt-config.h"
#include "mqtt.h"

static MqttNet wolf_net;
static MqttClient wolf_client;
static mqtt_conf_t *mqtt_conf;

word16 packet_id = 0;

unsigned char tx_buf[MQTT_MAX_PACKET_SZ];
unsigned char rx_buf[MQTT_MAX_PACKET_SZ];

enum conn_states {CONN_DISCONNECTED, CONN_CONNECTING, CONN_CONNECTED};
static enum conn_states conn_state;

static int init_wolf_instance();
static int mqtt_tls_cb(MqttClient* client);
static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
                           byte msg_new, byte msg_done);

static mqtt_conf_t *mqtt_conf;

extern int debug;
extern int run;

/*******************************************************************************
*
* WolfMqtt functions
*
*******************************************************************************/

struct net_ctx {
    int sockfd;
};
struct net_ctx ctx;

static int net_connect(void *context, const char *host, word16 port, int timeout_ms)
{
    (void)timeout_ms;
    struct net_ctx *ctx = (struct net_ctx *) context;
    struct addrinfo hints, *res = NULL;
    char portstr[8];

    snprintf(portstr, sizeof(portstr), "%u", (unsigned) port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, portstr, &hints, &res) != 0) {
        fprintf(stderr, "Error in getaddrinfo\n");
        return -1;
    }

    ctx->sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ctx->sockfd < 0) {
        fprintf(stderr, "Error in socket\n");
        freeaddrinfo(res);
        return -2;
    }

    if (connect(ctx->sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        fprintf(stderr,  "Error in connect\n");
        close(ctx->sockfd);
        ctx->sockfd = -1;
        freeaddrinfo(res);
        return -3;
    }

    freeaddrinfo(res);

    return MQTT_CODE_SUCCESS;
}

static int net_write(void *context, const byte *buf, int buf_len, int timeout_ms)
{
    (void) timeout_ms;
    struct net_ctx *ctx = (struct net_ctx *) context;
    int sent = send(ctx->sockfd, buf, buf_len, 0);
    if (sent < 0) {
        fprintf(stderr, "Error in send\n");
    }

    return sent;
}

static int net_read(void *context, byte *buf, int buf_len, int timeout_ms)
{
    (void)timeout_ms;
    struct net_ctx *ctx = (struct net_ctx *) context;
    struct timeval tv_old, tv_new;
    socklen_t tv_old_size;
    getsockopt(ctx->sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv_old, &tv_old_size);
    tv_new.tv_sec = timeout_ms / 1000;
    tv_new.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv_new, sizeof(tv_new));
    int r = recv(ctx->sockfd, buf, buf_len, 0);
    int e = errno;
    if (r < 0) {
        if (e == EAGAIN) {
            r = MQTT_CODE_ERROR_TIMEOUT;
        } else {
            fprintf(stderr, "Error in recv %d (%d)\n", r, e);
        }
    }
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv_old, sizeof(tv_old));

    return r;
}


static int net_disconnect(void *context)
{
    struct net_ctx *ctx = (struct net_ctx *) context;
    if (ctx->sockfd >= 0) close(ctx->sockfd);
    ctx->sockfd = -1;

    return MQTT_CODE_SUCCESS;
}

void mqtt_set_conf(mqtt_conf_t *conf)
{
    mqtt_conf = conf;
}

int init_mqtt(void)
{
    int rc;

    rc = init_wolf_instance();
    if(rc != 0)
    {
        fprintf(stderr, "Can't create wolf mqtt instance.\n");
        return -2;
    }

    conn_state = CONN_DISCONNECTED;

    return 0;
}

void stop_mqtt(void)
{
    MqttTopic topics[1];
    topics[0].topic_filter = mqtt_conf->mqtt_prefix_cmnd;
    MqttUnsubscribe wolf_unsubscribe;
    memset(&wolf_unsubscribe, 0, sizeof(wolf_unsubscribe));
    wolf_unsubscribe.packet_id = ++packet_id;
    wolf_unsubscribe.topic_count = 1;
    wolf_unsubscribe.topics = topics;
    MqttClient_Unsubscribe(&wolf_client, &wolf_unsubscribe);
    MqttClient_Disconnect(&wolf_client);
    MqttClient_NetDisconnect(&wolf_client);
    MqttClient_DeInit(&wolf_client);
}

static int init_wolf_instance()
{
    int rc;

    /* Prepare network callbacks */
    ctx.sockfd = -1;
    memset(&wolf_net, 0, sizeof(wolf_net));
    wolf_net.context = &ctx;
    wolf_net.connect = net_connect;
    wolf_net.read = net_read;
    wolf_net.write = net_write;
    wolf_net.disconnect = net_disconnect;

    /* Initialize MqttClient structure */
    memset(&wolf_client, 0, sizeof(wolf_client));
    rc = MqttClient_Init(&wolf_client, &wolf_net, mqtt_message_cb,
        tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf),
        DEFAULT_CMD_TIMEOUT);
    if (rc != MQTT_CODE_SUCCESS) {
        fprintf(stderr, "Error in MqttClient_Init: %s (%d)\n",
            MqttClient_ReturnCodeToString(rc), rc);
        return -2;
    }

    return 0;
}

void mqtt_loop(void) {
    int rc;
    int counter = 0;

    rc = MqttClient_WaitMessage(&wolf_client, 1000);

    if (!run) return;

    if (rc == MQTT_CODE_ERROR_TIMEOUT) {
        // no messages, normal
    } else if (rc != MQTT_CODE_SUCCESS) {
        conn_state = CONN_DISCONNECTED;
        fprintf(stderr, "Connection lost (rc=%d). Reconnect...\n", rc);

        // try to reconnect
        usleep(RECONNECT_DELAY * 1000);

        MqttTopic topics[1];
        topics[0].topic_filter = mqtt_conf->mqtt_prefix_cmnd;
        MqttUnsubscribe wolf_unsubscribe;
        memset(&wolf_unsubscribe, 0, sizeof(wolf_unsubscribe));
        wolf_unsubscribe.packet_id = ++packet_id;
        wolf_unsubscribe.topic_count = 1;
        wolf_unsubscribe.topics = topics;
        MqttClient_Unsubscribe(&wolf_client, &wolf_unsubscribe);
        MqttClient_Disconnect(&wolf_client);
        MqttClient_NetDisconnect(&wolf_client);
        if (mqtt_connect() != MQTT_CODE_SUCCESS) {
            fprintf(stderr, "Retry...\n");
            return;
        }
    }

    // Periodic ping
    counter++;
    if (counter >= MQTT_PING_INTERVAL) {
        counter = 0;
        fprintf(stderr, "Ping the broker...\n");
        rc = MqttClient_Ping(&wolf_client);
        if (rc != MQTT_CODE_SUCCESS) {
            fprintf(stderr, "Ping failed rc=%d, reconnect...\n", rc);

            // try to reconnect
            usleep(RECONNECT_DELAY * 1000);

            MqttTopic topics[1];
            topics[0].topic_filter = mqtt_conf->mqtt_prefix_cmnd;
            MqttUnsubscribe wolf_unsubscribe;
            memset(&wolf_unsubscribe, 0, sizeof(wolf_unsubscribe));
            wolf_unsubscribe.packet_id = ++packet_id;
            wolf_unsubscribe.topic_count = 1;
            wolf_unsubscribe.topics = topics;
            MqttClient_Unsubscribe(&wolf_client, &wolf_unsubscribe);
            MqttClient_Disconnect(&wolf_client);
            MqttClient_NetDisconnect(&wolf_client);
            if (mqtt_connect() != MQTT_CODE_SUCCESS) {
                fprintf(stderr, "Retry...\n");
                return;
            }
        }
    }
}

static int mqtt_tls_cb(MqttClient* client)
{
    client->tls.ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (client->tls.ctx) {
        wolfSSL_CTX_set_verify(client->tls.ctx, WOLFSSL_VERIFY_PEER, NULL);

        /* Load CA certificate file */
        if (wolfSSL_CTX_load_verify_locations(client->tls.ctx, CA_CERT, NULL) != SSL_SUCCESS) {
            fprintf(stderr, "wolfSSL_CTX_load_verify_locations failed\n");
            return WOLFSSL_FAILURE;
        }
    }

    /* Optional: load client certificate and key for mutual TLS */
    if ((access(CLIENT_CERT, F_OK) == 0) && (access(CLIENT_KEY, F_OK) == 0)) {
        if (wolfSSL_CTX_use_certificate_file(client->tls.ctx, CLIENT_CERT, WOLFSSL_FILETYPE_PEM) != SSL_SUCCESS) {
            fprintf(stderr, "wolfSSL_CTX_use_certificate_file failed\n");
            return WOLFSSL_FAILURE;
        }
        if (wolfSSL_CTX_use_PrivateKey_file(client->tls.ctx, CLIENT_KEY, WOLFSSL_FILETYPE_PEM) != SSL_SUCCESS) {
            fprintf(stderr, "wolfSSL_CTX_use_PrivateKey_file failed\n");
            return WOLFSSL_FAILURE;
        }
    }

    fprintf(stderr, "MQTT TLS setup completed\n");

    return WOLFSSL_SUCCESS;
}

static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
                           byte msg_new, byte msg_done)
{
    char topic_prefix[MAX_LINE_LENGTH], topic[MAX_LINE_LENGTH], file[MAX_KEY_LENGTH], param[MAX_KEY_LENGTH];
    char conf_file[256];
    char *slash;
    int len;
    char ipc_cmd_param[2];
    char cmd_line[1024];
    char *s;
    char value[MAX_VALUE_LENGTH];

    if (debug) printf("Received message for topic '%s'\n", msg->topic_name);
    /*
     * Check if the argument matches the subscription
     */
/*    mosquitto_topic_matches_sub(mqtt_conf->mqtt_prefix_cmnd, message->topic, &match);
    if (!match) {
        return;
    }*/

    /*
     * Check if the topic is in the form topic_prefix/file/parameter
     */
    memset(topic_prefix, '\0', MAX_LINE_LENGTH);
    memset(topic, '\0', MAX_LINE_LENGTH);
    memset(file, '\0', MAX_KEY_LENGTH);
    memset(param, '\0', MAX_KEY_LENGTH);

    // Remove /#
    len = strlen(mqtt_conf->mqtt_prefix_cmnd) - 2;
    if (len >= MAX_LINE_LENGTH) {
        if (debug) printf("Message topic exceeds buffer size\n");
        return -1;
    }
    strncpy(topic_prefix, mqtt_conf->mqtt_prefix_cmnd, len);
    if (strncasecmp(topic_prefix, msg->topic_name, len) != 0) {
        return -1;
    }
    if (strlen(msg->topic_name) < len + 2) {
        return -1;
    }
    if (strchr(msg->topic_name, '/') == NULL) {
        return -1;
    }
    strcpy(topic, &(msg->topic_name)[len + 1]);
    slash = strchr(topic, '/');
    if (slash == NULL) {
        strcpy(file, topic);
        if (strlen(file) == 0) {
            if (debug) printf("Wrong message subtopic\n");
            return -1;
        }
        if ((msg->buffer == NULL) || (msg->buffer_len == 0)) {
            // Send response with a dump of the configuration
            sprintf(cmd_line, "%s %s", CONF2MQTT_SCRIPT, file);
            if (debug) printf("Running system command \"%s\"\n", cmd_line);
            system(cmd_line);
        }
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    if (slash - topic >= MAX_KEY_LENGTH) {
        if (debug) printf("Message subtopic exceeds buffer size\n");
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    strncpy(file, topic, slash - topic);
    if (strlen(slash + 1) >= MAX_KEY_LENGTH) {
        if (debug) printf("Message subtopic exceeds buffer size\n");
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    strcpy(param, slash + 1);
    if (strlen(param) == 0) {
        if (debug) printf("Param is empty\n");
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    if ((msg->buffer == NULL) || (msg->buffer_len == 0)) {
        if (debug) printf("Payload is empty\n");
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    if (msg->buffer_len > MAX_VALUE_LENGTH - 1) {
        if (debug) printf("Payload is too long\n");
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    // Convert to upper case before validating and saving
    s = param;
    while (*s) {
        *s = toupper((unsigned char) *s);
        s++;
    }
    // Convert buffer to null terminated string
    memset(value, '\0', sizeof(value));
    memcpy(value, msg->buffer, msg->buffer_len);
    if (debug) printf("Validating: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, value);
    if (validate_param(file, param, value) == 1) {
        // Check if we need to run ipc_cmd
        if (extract_param(ipc_cmd_param, file, param, 8) == 0) {
            if (ipc_cmd_param[0] != '\0') {
                sprintf(cmd_line, ipc_cmd_param, value);
                if (debug) printf("Running system command \"%s\"\n", cmd_line);
                system(cmd_line);
            } else {
                sprintf(conf_file, "%s/%s.conf", CONF_FILE_PATH, file);
                if (debug) fprintf(stderr, "Updating file \"%s\", parameter \"%s\" with value \"%s\"\n", file, param, (char *) msg->buffer);
                config_replace(conf_file, param, (char *) msg->buffer);
            }
        }
    } else {
        printf("Validation error: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, (char *) msg->buffer);
        return -1;
    }

    return MQTT_CODE_SUCCESS;
}

void mqtt_check_connection()
{

}

int mqtt_connect()
{
    int rc;

    fprintf(stderr, "Trying to connect...\n");

    /* Set TLS flag so that MqttClient_NetConnect will attempt TLS when requested */
    if (mqtt_conf->tls) {
        rc = MqttClient_Flags(&wolf_client, 0, MQTT_CLIENT_FLAG_IS_TLS);
    }

    /* Prepare CONNECT packet */
    MqttConnect wolf_connect = {0};
    memset(&wolf_connect, '\0', sizeof(wolf_connect));
    wolf_connect.client_id = mqtt_conf->client_id;
    wolf_connect.keep_alive_sec = mqtt_conf->keepalive;
    wolf_connect.clean_session = 1;

    if ((mqtt_conf->user != NULL) && (strcmp(mqtt_conf->user, "") != 0)) {
        wolf_connect.username = mqtt_conf->user;
        wolf_connect.password = mqtt_conf->password;
    }

    /* Prepare subscribe struct */
    MqttSubscribe wolf_subscribe;
    memset(&wolf_subscribe, '\0', sizeof(wolf_subscribe));
    MqttTopic topics[1];

    /* Build list of topics */
    topics[0].topic_filter = mqtt_conf->mqtt_prefix_cmnd;
    topics[0].qos = mqtt_conf->qos;

    /* Subscribe Topic */
    wolf_subscribe.packet_id = ++packet_id;
    wolf_subscribe.topic_count = 1;
    wolf_subscribe.topics = topics;

    conn_state = CONN_DISCONNECTED;

    do
    {
        /* Connect to broker with TLS */
        rc = MqttClient_NetConnect(&wolf_client, mqtt_conf->host, mqtt_conf->port,
            DEFAULT_CON_TIMEOUT, mqtt_conf->tls, mqtt_conf->tls==1? mqtt_tls_cb : NULL);
        if (rc != MQTT_CODE_SUCCESS) {
            fprintf(stderr, "MQTT socket connect error: %s (%d)\n",
                MqttClient_ReturnCodeToString(rc), rc);
            usleep(500*1000);
            continue;
        }

        rc = MqttClient_Connect(&wolf_client, &wolf_connect);
        if (rc != MQTT_CODE_SUCCESS) {
            fprintf(stderr, "MQTT connect error: %s (%d)\n",
                MqttClient_ReturnCodeToString(rc), rc);
            usleep(500*1000);
            continue;
        }

        fprintf(stderr, "Subscribing topic %s\n", mqtt_conf->mqtt_prefix_cmnd);
        rc = MqttClient_Subscribe(&wolf_client, &wolf_subscribe);
        if (rc != MQTT_CODE_SUCCESS) {
            fprintf(stderr, "Unable to subscribe to the broker: %s (%d)\n",
                MqttClient_ReturnCodeToString(rc), rc);
            continue;
        }

    } while(rc != MQTT_CODE_SUCCESS);

    conn_state = CONN_CONNECTED;

    fprintf(stderr, "Connected!\n");

    return 0;
}
#endif //USE_MOSQUITTO
