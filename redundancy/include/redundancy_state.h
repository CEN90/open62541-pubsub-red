#ifndef REDUNDANCY_STATE_H
#define REDUNDANCY_STATE_H

#include <open62541/types.h>

typedef struct {
    UA_Int16 nmSequenceNr;
    UA_Int16 dswSequenceNr;

    size_t applicationStatesSize;
    UA_KeyValuePair *applicationStates;
} RedundancyState_s;

void registerRedundancyStateType(UA_Server *server);

#endif
