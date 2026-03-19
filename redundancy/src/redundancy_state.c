#include <open62541/types.h>
#include <open62541/types_generated.h>
#include "../include/redundancy_state.h"



static UA_DataTypeMember members[4] = {
    {
        .memberName = "nmSequenceNr",
        .memberType = &UA_TYPES[UA_TYPES_INT16],
        .padding = 0,
        .isArray = false
    },
    {
        .memberName = "dsmSequenceNr",
        .memberType = &UA_TYPES[UA_TYPES_INT16],
        .padding = 0,
        .isArray = false
    },
    {
        .memberName = "applicationStatesSize",
        .memberType = &UA_TYPES[UA_TYPES_UINT32],
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

// UA_DataType RedundancyStateType = {
//     .memSize = sizeof(RedundancyState_s),
//     .typeKind = UA_DATATYPEKIND_STRUCTURE,
//     .pointerFree = false,
//     .overlayable = false,
//     .membersSize = 3,
//     .members = members,
//     .typeName = "RedundancyState"
// };


void
getRedundancyState(UA_DataType *type, UA_NodeId typeNodeId) {
    UA_DataType RedundancyStateType = {
        .typeId = typeNodeId,
        .memSize = sizeof(RedundancyState_s),
        .typeKind = UA_DATATYPEKIND_STRUCTURE,
        .pointerFree = false,
        .overlayable = false,
        .membersSize = 4,
        .members = members,
        .typeName = "RedundancyState"
    };
    
    *type = RedundancyStateType;
}
