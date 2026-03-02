#ifndef OPEN62541_PUBSUB_REDUNDANCY_H
#define OPEN62541_PUBSUB_REDUNDANCY_H

#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

typedef struct {
    UA_Int64 state;
} State_s;

typedef struct {
    UA_Int32 something;
} connectionConfig_s;

const UA_DateTime HEARBEATIMEOUT = 10;
UA_Boolean isPrimary;

UA_StatusCode
setupHeartbeat(const char *ipAddress, int port, int *heartbeatSockfd);

UA_StatusCode
sendHeartbeat(void);

UA_StatusCode
checkHeartbeat(int sock);

int
initHeartbeatListener(int port);

UA_StatusCode
syncState(State_s *state);

UA_StatusCode
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config);

UA_StatusCode
setupPubSub(void);

#endif
