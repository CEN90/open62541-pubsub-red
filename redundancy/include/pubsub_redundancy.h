#ifndef OPEN62541_PUBSUB_REDUNDANCY_H
#define OPEN62541_PUBSUB_REDUNDANCY_H

#include <open62541/server_pubsub.h>
#include <open62541/server.h>
#include <open62541/types.h>
#include <open62541/util.h>

#include "pubsub_heartbeat.h"



#define MAGICNUMBER 42
#define IPADDRLEN 16

typedef struct {
    const int *port;
    const char *ipAddress;
} connectionConfig_s;

static UA_Boolean *isPrimary;
static UA_Server *server;
static UA_NodeId writerGroupIdent, dataSetWriterId;

UA_StatusCode
syncState(size_t appStateSize, UA_KeyValuePair *applicationStates);

UA_StatusCode
initStateSync(UA_Boolean *_isPrimary, UA_Server *_server,
            HeartbeatConfig *config, UA_NodeId _writerGroupIdent,
            UA_NodeId _dataSetWriterId, size_t appStateVars,
            UA_KeyValuePair *applicationStates);

#endif
