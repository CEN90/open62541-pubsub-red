#ifndef REDUNDANCY_STATE_H
#define REDUNDANCY_STATE_H

#include <open62541/types.h>

typedef struct {
    UA_Int16 nmSequenceNr;
    UA_Int16 dswSequenceNr;

    UA_UInt32 applicationStatesSize;
    UA_KeyValuePair *applicationStates;
} RedundancyState_s;

extern UA_DataType RedundancyStateType;

void
getRedundancyState(UA_DataType *type, UA_NodeId typeNodeId);

#endif