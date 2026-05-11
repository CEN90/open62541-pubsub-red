#ifndef OPEN62541_PUBSUB_HEARTBEAT_H
#define OPEN62541_PUBSUB_HEARTBEAT_H

#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#include <stdbool.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define HEARTBEAT_MSG "\n"
#define BUFFER_SIZE 1024
#define HEARTBEAT_TIMEOUT_MS 15
#define HEARBEATIMEOUT (HEARTBEAT_TIMEOUT_MS * UA_DATETIME_MSEC)
#define INITSLEEP 2
#define HEARTBEATPERIOD 5000
#define HEARTBEATSLACK 0 
#define HEARTBEATYEETCOUNT 1000

typedef struct {
    const char *ipAddress;
    const int *port;
    int *sockfd;
    UA_Boolean *isPrimary;
} HeartbeatConfig;

void *
initHeartBeat(void *arg);

UA_StatusCode
setupHeartbeat(const char *ipAddress, int port, int *sockfd, UA_Boolean const *isPrimary);

void
runHeartbeat(const char *ipAddress, int port, int *sockfd, UA_Boolean *isPrimary);

UA_StatusCode
setupHeartbeatSender(const char *ipAddress, int port, int *sockfd);

UA_StatusCode
setupHeartbeatReceiver(int port, int *sockfd);

UA_StatusCode
sendHeartbeat(int const *sockfd);

UA_StatusCode
receiveHeartbeat(int sockfd, UA_Boolean *isPrimary, UA_DateTime *prevHbTime);

#endif  // OPEN62541_PUBSUB_HEARTBEAT_H
