#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifdef USE_MOSQUITTO
#include <mosquitto.h>
#else
#include <wolfmqtt/mqtt_client.h>
#include <wolfmqtt/mqtt_packet.h>
#include <wolfmqtt/mqtt_socket.h>
#include <wolfmqtt/mqtt_types.h>
#endif

//#include <mqttv4.h>

//#define EMPTY_TOPIC         "EMPTY_TOPIC"
#define MQTT_MAX_PACKET_SZ  2048
#define MQTT_PING_INTERVAL  30
#define RECONNECT_DELAY     3000
#define DEFAULT_CON_TIMEOUT 5000
#define DEFAULT_CMD_TIMEOUT 30000

void mqtt_set_conf(mqtt_conf_t *conf);
int init_mqtt(void);
void stop_mqtt(void);
int init_mosquitto_instance();
void mqtt_loop(void);
#ifdef USE_MOSQUITTO
void connect_callback(struct mosquitto *mosq, void *obj, int result);
void disconnect_callback(struct mosquitto *mosq, void *obj, int result);
void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
#endif
void mqtt_check_connection();
int mqtt_connect();
