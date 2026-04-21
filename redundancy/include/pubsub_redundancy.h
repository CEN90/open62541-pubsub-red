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

typedef struct {
    UA_Boolean *isPrimary;
    UA_Server *server;
    HeartbeatConfig *config;
    UA_NodeId writerGroupIdent;
    UA_NodeId dataSetWriterId;
    UA_UInt32 appStateSize;
    UA_Int64 *applicationStates;
} syncThreadArgs_s;

static UA_Boolean *isPrimary;
static UA_Server *server;
static UA_NodeId writerGroupIdent, dataSetWriterId;

UA_StatusCode
syncState(UA_UInt32 appStateSize, const UA_Int64 *applicationStates);

static void *
initSyncThreads(void *arg);

UA_StatusCode
initStateSync(UA_Boolean *isPrimary, UA_Server *_server,
            HeartbeatConfig *config, UA_NodeId writerGroupIdent,
            UA_NodeId dataSetWriterId, UA_UInt32 appStateSize,
            UA_Int64 *applicationStates);

#endif
