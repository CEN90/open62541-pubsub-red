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

void
keep_alive(void);

void
sync(State_s *state);

void
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config);

void
setupPubSub(void);

#endif
