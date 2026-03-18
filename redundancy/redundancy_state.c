#include <open62541/redundancy_state.h>
#include <open62541/types_generated.h>



static UA_DataTypeMember members[3] = {
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
        .memberName = "applicationStates",
        .memberType = &UA_TYPES[UA_TYPES_KEYVALUEPAIR],
        .padding = 0,
        .isArray = true
    }
};

UA_DataType RedundancyStateType = {
    .typeId = UA_NODEID_STRING(1, "RedundancyState"),
    .memSize = sizeof(RedundancyState_s),
    .typeKind = UA_DATATYPEKIND_STRUCTURE,
    .pointerFree = false,
    .overlayable = false,
    .membersSize = 3,
    .members = members
};