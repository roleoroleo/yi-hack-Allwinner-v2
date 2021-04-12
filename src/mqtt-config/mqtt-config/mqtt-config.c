#include "mqtt-config.h"

mqtt_conf_t conf;

int run = 1;

/*
 * signal handler to terminate program
 */
void handle_signal(int s)
{
    run = 0;
    printf("Signal Handler\n");
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

    debug = 1;

    if (debug) fprintf(stderr, "Received message '%.*s' for topic '%s'\n", message->payloadlen, (char*) message->payload, message->topic);
    /*
     * Check if the argument matches the subscription
     */
    mosquitto_topic_matches_sub(conf.mqtt_prefix_config, message->topic, &match);
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

    len = strlen(conf.mqtt_prefix_config) - 2;
    if (len >= MAX_LINE_LENGTH) {
        if (debug) fprintf(stderr, "Message topic exceeds buffer size\n");
        return;
    }
    strncpy(topic_prefix, conf.mqtt_prefix_config, len);
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
        return;
    }
    if (slash - topic >= MAX_KEY_LENGTH) {
        if (debug) fprintf(stderr, "Message subtopic exceeds buffer size\n");
        return;
    }
    strncpy(file, topic, slash - topic);
    if (strlen(slash + 1) >= MAX_KEY_LENGTH) {
        if (debug) fprintf(stderr, "Message subtopic exceeds buffer size\n");
        return;
    }
    strcpy(param, slash + 1);
    if (strlen(message->payload) == 0) {
        if (debug) fprintf(stderr, "Payload is empty\n");
        return;
    }
    if (debug) fprintf(stderr, "Validating: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, (char *) message->payload);
    if (validate_param(file, param, message->payload) == 1) {
        sprintf(conf_file, "%s/%s.conf", CONF_FILE_PATH, file);
        if (debug) fprintf(stderr, "Updating file \"%s\", parameter \"%s\" with value \"%s\"\n", file, param, (char *) message->payload);
        config_replace(conf_file, param, message->payload);
    } else {
        fprintf(stderr, "Validation error: file \"%s\", parameter \"%s\", value \"%s\"\n", file, param, (char *) message->payload);
    }
}

int main(int argc, char *argv[])
{
    debug = 1;

    /*
     * Client instance
     */
    struct mosquitto *mosq;

    if (mqtt_init_conf(&conf) < 0)
        return -1;
    stop_config();

    int rc = 0;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    /*
     * Initialize library. Call it before any other function.
     */
    mosquitto_lib_init();

    /*
     * Create a new client instance
     */
    mosq = mosquitto_new(conf.client_id, true, 0);

    if(mosq) {
        /* 
         * Set connection credentials.
         */
        if (conf.user!=NULL && strcmp(conf.user, "") != 0) {
            rc = mosquitto_username_pw_set(mosq, conf.user, conf.password);

            if(rc != MOSQ_ERR_SUCCESS) {
                printf("Unable to set the auth parameters (%s).\n", mosquitto_strerror(rc));
                return -2;
            }
        }

        /* 
         * Set connection callback.
         * It's called when the broker send CONNACK message.
         */
        mosquitto_connect_callback_set(mosq, connect_callback);
        /* 
         * Set subscription callback.
         * It's called when the broker send a subscribed message.
         */
        mosquitto_message_callback_set(mosq, message_callback);
        /* 
         * Connect to the broker.
         */
        rc = mosquitto_connect(mosq, conf.host, conf.port, 15);
        if (rc != MOSQ_ERR_SUCCESS) {
            printf("Unable to connect to the broker\n");
            return -3;
        }
        if (conf.mqtt_prefix_config == NULL) {
            printf("Wrong topic prefix, please check configuration.\n");
            return -4;
        }
        /*
         * Subscribe a topic.
         * mosq   - mosquitto instance
         * NULL   - optional message id
         * topic  - topic to subscribe
         * 0      - QoS Quality of service
         */ 
        rc = mosquitto_subscribe(mosq, NULL, conf.mqtt_prefix_config, 0);
        if (rc != MOSQ_ERR_SUCCESS) {
            printf("Unable to subscribe to the broker\n");
            return -5;
        }

        /*
         * Infinite loop waiting for incoming messages
         */
        while (run) {
            /*
             * Main loop for the client.
             * Mandatory to keep active the communication between the
             * client and the broker.
             * mosq - mosquitto instance
             * -1   - Max wait time for select() call (ms)
             *        Set = 0 for immediate return
             *        Set < 0 for default vaule (1000 ms)
             * 1    - Unused parameter
             */
            rc = mosquitto_loop(mosq, -1, 1);
            /*
             * If rc != 0 there was an error.
             * Wait 5 seconds and reconnect to the broker.
             */
            if (run && rc) {
                printf("Connection error! Try again...\n");
                sleep(5);
                mosquitto_reconnect(mosq);
            }
        }
        /*
         * Free memory related to mosquitto instance.
         */
        mosquitto_destroy(mosq);
    }
    /*
     * Free mosquitto library resources.
     */
    mosquitto_lib_cleanup();

    return rc;
}

void handle_config(const char *key, const char *value)
{
    int nvalue;

    if(strcmp(key, "MQTT_IP") == 0) {
        if (strlen(value) < sizeof(conf.host))
            strcpy(conf.host, value);
    } else if(strcmp(key, "MQTT_CLIENT_ID")==0) {
        conf.client_id = malloc((char) strlen(value) + 1 + 2);
        sprintf(conf.client_id, "%s_c", value);
    } else if(strcmp(key, "MQTT_USER") == 0) {
        conf.user = malloc((char) strlen(value) + 1);
        strcpy(conf.user, value);
    } else if(strcmp(key, "MQTT_PASSWORD") == 0) {
        conf.password = malloc((char) strlen(value) + 1);
        strcpy(conf.password, value);
    } else if(strcmp(key, "MQTT_PORT") == 0) {
        errno = 0;
        nvalue = strtol(value, NULL, 10);
        if(errno == 0)
            conf.port = nvalue;
    } else if(strcmp(key, "MQTT_PREFIX_CONFIG") == 0) {
        // Reserve space for "/#" at the end
        conf.mqtt_prefix_config = malloc((char) strlen(value) + 1 + 2);
        strcpy(conf.mqtt_prefix_config, value);
        int len = strlen(conf.mqtt_prefix_config);
        if (conf.mqtt_prefix_config[len - 1] == '/') {
            conf.mqtt_prefix_config[len] = '#';
            conf.mqtt_prefix_config[len + 1] = '\0';
        } else {
            conf.mqtt_prefix_config[len] = '/';
            conf.mqtt_prefix_config[len + 1] = '#';
            conf.mqtt_prefix_config[len + 2] = '\0';
        }
    } else {
        if (debug) {
            fprintf(stderr, "Ignoring key: %s - value: %s\n", key, value);
        }
    }
}

int mqtt_init_conf(mqtt_conf_t *conf)
{
    strcpy(conf->host, "127.0.0.1");
    strcpy(conf->bind_address, "0.0.0.0");
    conf->user = NULL;
    conf->password = NULL;
    conf->port = 1883;
    conf->mqtt_prefix_config = NULL;

    if (init_config(MQTT_CONF_FILE) != 0)
    {
        printf("Cannot open config file\n");
        return -1;
    }

    config_set_handler(&handle_config);
    config_parse();

    return 0;
}
