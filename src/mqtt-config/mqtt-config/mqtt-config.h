#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "mosquitto.h"
#include "config.h"
#include "validate.h"

#define MQTT_CONF_FILE    "/home/yi-hack/etc/mqttv4.conf"
#define CONF_FILE_PATH    "/home/yi-hack/etc"

typedef struct
{
    char       *user;
    char       *password;
    char        host[128];
    char        bind_address[128];
    int         port;
    char       *mqtt_prefix_config;
} mqtt_conf_t;

void handle_signal(int s);
void connect_callback(struct mosquitto *mosq, void *obj, int result);
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
void handle_config(const char *key, const char *value);
int mqtt_init_conf(mqtt_conf_t *conf);
