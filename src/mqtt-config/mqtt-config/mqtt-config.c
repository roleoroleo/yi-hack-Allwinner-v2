#include "mqtt-config.h"
#include "mqtt.h"

mqtt_conf_t mqtt_conf;
int run;
int debug = 1;

/*
 * pipe signal handler
 */
void handle_pipe_signal(int s)
{
    printf("Pipe Signal Handler\n");
}

/*
 * signal handler to terminate program
 */
void handle_signal(int s)
{
    run = 0;
    printf("Signal Handler\n");
}

void handle_config(const char *key, const char *value)
{
    int nvalue;

    if(strcmp(key, "MQTT_IP") == 0) {
        if (strlen(value) < sizeof(mqtt_conf.host))
            strcpy(mqtt_conf.host, value);
    } else if(strcmp(key, "MQTT_CLIENT_ID")==0) {
        mqtt_conf.client_id = malloc((char) strlen(value) + 1 + 2);
        sprintf(mqtt_conf.client_id, "%s_c", value);
    } else if(strcmp(key, "MQTT_PREFIX")==0) {
        mqtt_conf.mqtt_prefix_cmnd = malloc((char) strlen(value) + 1 + 7);
        sprintf(mqtt_conf.mqtt_prefix_cmnd, "%s/cmnd/#", value);
    } else if(strcmp(key, "MQTT_USER") == 0) {
        mqtt_conf.user = malloc((char) strlen(value) + 1);
        strcpy(mqtt_conf.user, value);
    } else if(strcmp(key, "MQTT_PASSWORD") == 0) {
        mqtt_conf.password = malloc((char) strlen(value) + 1);
        strcpy(mqtt_conf.password, value);
    } else if(strcmp(key, "MQTT_PORT") == 0) {
        errno = 0;
        nvalue = strtol(value, NULL, 10);
        if(errno == 0)
            mqtt_conf.port = nvalue;
    } else if(strcmp(key, "MQTT_TLS") == 0) {
        errno = 0;
        nvalue = strtol(value, NULL, 10);
        if(errno == 0)
            mqtt_conf.tls = nvalue;
#ifdef USE_MOSQUITTO
        mqtt_conf.tls = 0;
#endif
    } else if(strcmp(key, "MQTT_KEEPALIVE") == 0) {
        errno = 0;
        nvalue = strtol(value, NULL, 10);
        if(errno == 0)
            mqtt_conf.keepalive = nvalue;
    } else if(strcmp(key, "MQTT_QOS") == 0) {
        errno = 0;
        nvalue = strtol(value, NULL, 10);
        if(errno == 0)
            mqtt_conf.qos = nvalue;
    } else {
        if (debug) {
            printf("Ignoring key: %s - value: %s\n", key, value);
        }
    }
}

int mqtt_free_conf(mqtt_conf_t *conf){
    if (conf != NULL) {
        if (conf->client_id != NULL) free(conf->client_id);
        if (conf->mqtt_prefix_cmnd != NULL) free(conf->mqtt_prefix_cmnd);
        if (conf->user != NULL) free(conf->user);
        if (conf->password != NULL) free(conf->password);
    }
    return 0;
}

int mqtt_init_conf(mqtt_conf_t *conf)
{
    FILE *fp;

    strcpy(conf->host, "127.0.0.1");
    strcpy(conf->bind_address, "0.0.0.0");
    conf->user = NULL;
    conf->password = NULL;
    conf->port = 1883;
    conf->tls = 0;
    conf->qos = 1;
    conf->keepalive = 120;
    conf->mqtt_prefix_cmnd = NULL;

    fp = open_conf_file(MQTT_CONF_FILE);
    if (fp == NULL)
        return -1;

    config_set_handler(&handle_config);
    config_parse(fp);

    close_conf_file(fp);

    return 0;
}

int main(int argc, char *argv[])
{
    int ret;

    run = 1;

    if (mqtt_init_conf(&mqtt_conf) < 0)
    {
        mqtt_free_conf(&mqtt_conf);
        return -1;
    }
    mqtt_set_conf(&mqtt_conf);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, handle_pipe_signal);
    /*
     * Initialize library. Call it before any other function.
     */
    ret = init_mqtt();

    if (ret == 0) {
        if (mqtt_connect() != 0)
        {
            mqtt_free_conf(&mqtt_conf);
            return -2;
        }

        while(run)
        {
            mqtt_check_connection();
            mqtt_loop();
            usleep(500*1000);
        }

        stop_mqtt();
    }
    mqtt_free_conf(&mqtt_conf);

    return 0;
}
