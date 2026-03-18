#ifndef OPEN62541_PUBSUB_REDUNDANCY_H
#define OPEN62541_PUBSUB_REDUNDANCY_H

#include <open62541/server_pubsub.h>
#include <open62541/server.h>
#include <open62541/types.h>

#include "pubsub_heartbeat.h"
#include "redundancy_state.h"



#define MAGICNUMBER 42
#define IPADDRLEN 16

typedef struct {
    const int *port;
    const char *ipAddress;
} connectionConfig_s;

extern UA_Boolean isPrimary;

UA_StatusCode
syncState(RedundancyState_s *state, UA_Boolean const *isPrimary, UA_Server *server,
          UA_NodeId writerGroupIdent, UA_NodeId dataSetWriterId);

UA_StatusCode
init(UA_Boolean *isPrimary, RedundancyState_s *state, HeartbeatConfig *config);

#endif
