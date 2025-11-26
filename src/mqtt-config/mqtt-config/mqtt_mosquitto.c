#ifdef USE_MOSQUITTO
#include "mqtt-config.h"
#include "mqtt.h"

struct mosquitto *mosq;
enum conn_states {CONN_DISCONNECTED, CONN_CONNECTING, CONN_CONNECTED};
static enum conn_states conn_state;

static mqtt_conf_t *mqtt_conf;

extern int debug;

void mqtt_set_conf(mqtt_conf_t *conf)
{
    mqtt_conf = conf;
}

int init_mqtt(void)
{
    int ret;

    ret=mosquitto_lib_init();
    if(ret!=0)
    {
        fprintf(stderr, "Can't initialize mosquitto library.\n");
        return -1;
    }

    ret=init_mosquitto_instance();
    if(ret!=0)
    {
        fprintf(stderr, "Can't create mosquitto instance.\n");
        return -2;
    }

    conn_state=CONN_DISCONNECTED;

    return 0;
}

void stop_mqtt(void)
{
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}

int init_mosquitto_instance()
{
    mosq = mosquitto_new(mqtt_conf->client_id, true, NULL);

    if(!mosq){
        switch(errno){
        case ENOMEM:
            fprintf(stderr, "Error: Out of memory.\n");
            break;
        case EINVAL:
            fprintf(stderr, "Error: Invalid id.\n");
            break;
        }
        mosquitto_lib_cleanup();
        return -1;
    }

    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_disconnect_callback_set(mosq, disconnect_callback);
    mosquitto_message_callback_set(mosq, message_callback);

    return 0;
}

void mqtt_loop(void)
{
    if(mosq!=NULL)
        mosquitto_loop(mosq, -1, 1);
}

/*
 * connection callback
 * mosq   - mosquitto instance
 * obj    - user data for mosquitto_new
 * result - return code:
 *  0 - success
 *  1 - connection denied (incorrect protocol version)
 *  2 - connection denied (wrong id)
 *  3 - connection denied (broker not available)
 *  4-255 - reserved
 */
void connect_callback(struct mosquitto *mosq, void *obj, int result)
{
    printf("Callback, connected with return code rc=%d\n", result);

    if(result==MOSQ_ERR_SUCCESS)
    {
        conn_state=CONN_CONNECTED;
    }
    else
    {
        conn_state=CONN_DISCONNECTED;
    }
}

void disconnect_callback(struct mosquitto *mosq, void *obj, int result)
{
    conn_state=CONN_DISCONNECTED;
}

/*
 * message callback when a message is published
 * mosq     - mosquitto instance
 * obj      - user data for mosquitto_new
 * message  - message data
 *               The library will free this variable and the related memory
 *               when callback terminates.
 *               The client must copy all needed data.
 */
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
    char topic_prefix[MAX_LINE_LENGTH], topic[MAX_LINE_LENGTH], file[MAX_KEY_LENGTH], param[MAX_KEY_LENGTH];
    char conf_file[256];
    char *slash;
    bool match = 0;
    int len;
    char ipc_cmd_param[2];
    char cmd_line[1024];
    char *s;

    if (debug) printf("Received message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);
    /*
     * Check if the argument matches the subscription
     */
    mosquitto_topic_matches_sub(mqtt_conf->mqtt_prefix_cmnd, message->topic, &match);
    if (!match) {
        return;
    }

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
        return;
    }
    strncpy(topic_prefix, mqtt_conf->mqtt_prefix_cmnd, len);
    if (strncasecmp(topic_prefix, message->topic, len) != 0) {
        return;
    }
    if (strlen(message->topic) < len + 2) {
        return;
    }
    if (strchr(message->topic, '/') == NULL) {
        return;
    }
    strcpy(topic, &(message->topic)[len + 1]);
    slash = strchr(topic, '/');
    if (slash == NULL) {
        strcpy(file, topic);
        if (strlen(file) == 0) {
            if (debug) printf("Wrong message subtopic\n");
            return;
        }
        if ((message->payload == NULL) || (strlen(message->payload) == 0)) {
            // Send response with a dump of the configuration
            sprintf(cmd_line, "%s %s", CONF2MQTT_SCRIPT, file);
            if (debug) printf("Running system command \"%s\"\n", cmd_line);
            system(cmd_line);
        }
        return;
    }
    if (slash - topic >= MAX_KEY_LENGTH) {
        if (debug) printf("Message subtopic exceeds buffer size\n");
        return;
    }
    strncpy(file, topic, slash - topic);
    if (strlen(slash + 1) >= MAX_KEY_LENGTH) {
        if (debug) printf("Message subtopic exceeds buffer size\n");
        return;
    }
    strcpy(param, slash + 1);
    if (strlen(param) == 0) {
        if (debug) printf("Param is empty\n");
        return;
    }
    if ((message->payload == NULL) || (strlen(message->payload) == 0)) {
        if (debug) printf("Payload is empty\n");
        return;
    }
    // Convert to upper case before validating and saving
    s = param;
    while (*s) {
        *s = toupper((unsigned char) *s);
        s++;
    }
    if (debug) printf("Validating: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, (char *) message->payload);
    if (validate_param(file, param, message->payload) == 1) {
        // Check if we need to run ipc_cmd
        if (extract_param(ipc_cmd_param, file, param, 8) == 0) {
            if (ipc_cmd_param[0] != '\0') {
                sprintf(cmd_line, ipc_cmd_param, (char *) message->payload);
                if (debug) printf("Running system command \"%s\"\n", cmd_line);
                system(cmd_line);
            } else {
                sprintf(conf_file, "%s/%s.conf", CONF_FILE_PATH, file);
                if (debug) fprintf(stderr, "Updating file \"%s\", parameter \"%s\" with value \"%s\"\n", file, param, (char *) message->payload);
                config_replace(conf_file, param, message->payload);
            }
        }
    } else {
        printf("Validation error: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, (char *) message->payload);
    }
}

void mqtt_check_connection()
{
    if (conn_state != CONN_CONNECTED) {
        mqtt_connect();
    }
}

int mqtt_connect()
{
    int rc = 0;

    /*
     * Set connection credentials.
     */
    if (mqtt_conf->user!=NULL && strcmp(mqtt_conf->user, "") != 0) {
        rc = mosquitto_username_pw_set(mosq, mqtt_conf->user, mqtt_conf->password);
        if(rc != MOSQ_ERR_SUCCESS) {
            printf("Unable to set the auth parameters (%s).\n", mosquitto_strerror(rc));
            return -2;
        }
    }

    /*
     * Connect to the broker.
     */
    conn_state = CONN_DISCONNECTED;

    while (conn_state != CONN_CONNECTED) {

        if (debug) printf("Connecting to broker %s\n", mqtt_conf->host);
        do
        {
            rc = mosquitto_connect(mosq, mqtt_conf->host, mqtt_conf->port, 15);

            if(rc != MOSQ_ERR_SUCCESS)
                printf("Unable to connect (%s).\n", mosquitto_strerror(rc));

            usleep(500*1000);

        } while(rc != MOSQ_ERR_SUCCESS);

        if (conn_state == CONN_DISCONNECTED)
            conn_state = CONN_CONNECTING;

        /*
         * Subscribe a topic.
         * mosq   - mosquitto instance
         * NULL   - optional message id
         * topic  - topic to subscribe
         * 0      - QoS Quality of service
         */
        if (debug) printf("Subscribing topic %s\n", mqtt_conf->mqtt_prefix_cmnd);
        rc = mosquitto_subscribe(mosq, NULL, mqtt_conf->mqtt_prefix_cmnd, 0);
        if (rc != MOSQ_ERR_SUCCESS) {
            printf("Unable to subscribe to the broker\n");
            break;
        }

        if (conn_state == CONN_CONNECTING)
            conn_state = CONN_CONNECTED;

        /*
         * Infinite loop waiting for incoming messages
         */
        do
        {
            rc = mosquitto_loop(mosq, -1, 1);
            if(conn_state != CONN_CONNECTED)
                usleep(100*1000);
        } while(conn_state == CONN_CONNECTING);
    }

    return 0;
}
#endif //USE_MOSQUITTO
