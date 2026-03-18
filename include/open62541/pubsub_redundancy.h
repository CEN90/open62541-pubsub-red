#ifndef OPEN62541_PUBSUB_REDUNDANCY_H
#define OPEN62541_PUBSUB_REDUNDANCY_H

#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#define MAGICNUMBER 42
#define IPADDRLEN 16

typedef struct {
    UA_Int16 nmSequenceNr;
    UA_Int16 dsmSequenceNr;

    size_t applicationStatesSize;
    UA_KeyValuePair *applicationStates;
} RedundancyState_s;

typedef struct {
    const int *port;
    const char *ipAddress;
} heartbeatConfig_s;

extern UA_Boolean isPrimary;

UA_StatusCode
syncState(RedundancyState_s *state, UA_Boolean *isPrimary, UA_Server *server,
          UA_NodeId writerGroupIdent);

UA_StatusCode
init(UA_Boolean *isPrimary, RedundancyState_s *state, heartbeatConfig_s *config);

#endif
