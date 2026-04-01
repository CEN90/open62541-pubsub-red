#include <open62541/types.h>
#include <open62541/types_generated.h>
#include "../include/redundancy_state.h"

void registerRedundancyStateType(UA_Server *server) {
    UA_StructureDefinition structDef;
    UA_StructureDefinition_init(&structDef);

    structDef.structureType = UA_STRUCTURETYPE_STRUCTURE;
    structDef.baseDataType = UA_NODEID_NUMERIC(0, UA_NS0ID_STRUCTURE);

    structDef.fieldsSize = 3;
    structDef.fields = UA_Array_new(3, &UA_TYPES[UA_TYPES_STRUCTUREFIELD]);

    UA_StructureField_init(&structDef.fields[0]);
    structDef.fields[0].name = UA_STRING("nmSequenceNr");
    structDef.fields[0].dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_INT16);
    structDef.fields[0].valueRank = -1;

    UA_StructureField_init(&structDef.fields[1]);
    structDef.fields[1].name = UA_STRING("dswSequenceNr");
    structDef.fields[1].dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_INT16);
    structDef.fields[1].valueRank = -1;

    UA_StructureField_init(&structDef.fields[2]);
    structDef.fields[2].name = UA_STRING("applicationStates");
    structDef.fields[2].dataType = UA_NODEID_NUMERIC(0, UA_NS0ID_KEYVALUEPAIR);
    structDef.fields[2].valueRank = 1; // array

    UA_ExtensionObject ext;
    UA_ExtensionObject_init(&ext);
    ext.encoding = UA_EXTENSIONOBJECT_DECODED;
    ext.content.decoded.type = &UA_TYPES[UA_TYPES_STRUCTUREDEFINITION];
    ext.content.decoded.data = &structDef;

    UA_StatusCode retval = UA_Server_addDataTypeFromDescription(server, &ext);
    if(retval != UA_STATUSCODE_GOOD)
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to register RedundancyStateType");

    UA_Array_delete(structDef.fields, structDef.fieldsSize, &UA_TYPES[UA_TYPES_STRUCTUREFIELD]);
}
