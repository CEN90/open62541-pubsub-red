#ifndef REDUNDANCY_STATE_H
#define REDUNDANCY_STATE_H

#include <open62541/types.h>
#include <open62541/server.h>

typedef struct {
    UA_Int16 nmSequenceNr;
    UA_Int16 dswSequenceNr;

    size_t applicationStatesSize;
    UA_Int64 *applicationStates;
} RedundancyState_s;

UA_StatusCode registerRedundancyStateType(UA_Server *server);

const UA_DataType* getRedundancyType(UA_Server *server);

#endif
