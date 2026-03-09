#ifndef OPEN62541_PUBSUB_REDUNDANCY_H
#define OPEN62541_PUBSUB_REDUNDANCY_H

#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#define MAGICNUMBER 42
#define IPADDRLEN 16

typedef struct {
    UA_Int64 state;
} State_s;

typedef struct {
    const int *port;
    const char *ipAddress;
} connectionConfig_s;

extern UA_Boolean isPrimary;

UA_StatusCode
syncState(State_s *state);

UA_StatusCode
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config);

#endif
