#include <open62541/types.h>
#include <open62541/types_generated.h>
#include "../include/redundancy_state.h"



static UA_DataTypeMember members[3] = {
    {
        .memberName = "nmSequenceNr",
        .memberType = &UA_TYPES[UA_TYPES_INT16],
        .padding = 0,
        .isArray = false
    },
    {
        .memberName = "dswSequenceNr",
        .memberType = &UA_TYPES[UA_TYPES_INT16],
        .padding = 0,
        .isArray = false
    },
    {
        .memberName = "applicationStates",
        .memberType = &UA_TYPES[UA_TYPES_KEYVALUEPAIR],
        .padding = 0,
        .isArray = true,
    }
};

UA_DataType RedundancyStateType = {
    .memSize = sizeof(RedundancyState_s),
    .typeKind = UA_DATATYPEKIND_STRUCTURE,
    .pointerFree = false,
    .overlayable = false,
    .membersSize = 3,
    .members = members,
    .typeName = "RedundancyState"
};
