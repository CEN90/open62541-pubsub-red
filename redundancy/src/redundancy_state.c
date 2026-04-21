#include <open62541/types.h>
#include <open62541/types_generated.h>
#include <open62541/plugin/log_stdout.h>
#include "../include/redundancy_state.h"

static UA_NodeId redundancyTypeId = {0};

UA_StatusCode registerRedundancyStateType(UA_Server *server) {
    UA_UInt16 ns = UA_Server_addNamespace(server, "RedundancyState");

    UA_NodeId typeNodeId = UA_NODEID_NUMERIC(ns, 10002);
    UA_NodeId_copy(&typeNodeId, &redundancyTypeId);
    UA_DataTypeAttributes dtAttr = UA_DataTypeAttributes_default;
    dtAttr.displayName = UA_LOCALIZEDTEXT("en-US", "RedundancyStateType");
    UA_StatusCode addTypeStatus = UA_Server_addDataTypeNode(
        server, typeNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_STRUCTURE),
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
        UA_QUALIFIEDNAME(ns, "RedundancyStateType"),
        dtAttr, NULL, NULL);
    if(addTypeStatus != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to add DataTypeNode: 0x%08x", addTypeStatus);
        return UA_STATUSCODE_BAD;
    }

    UA_StructureDefinition structDef;
    UA_StructureDefinition_init(&structDef);
    structDef.structureType = UA_STRUCTURETYPE_STRUCTURE;
    structDef.baseDataType  = UA_NODEID_NUMERIC(0, UA_NS0ID_STRUCTURE);
    structDef.defaultEncodingId = UA_NODEID_NUMERIC(ns, 10003);

    structDef.fieldsSize = 3;
    structDef.fields = (UA_StructureField*)
        UA_Array_new(structDef.fieldsSize, &UA_TYPES[UA_TYPES_STRUCTUREFIELD]);
    if(!structDef.fields) return UA_STATUSCODE_BAD;
    for(size_t i = 0; i < structDef.fieldsSize; i++)
        UA_StructureField_init(&structDef.fields[i]);

    structDef.fields[0].name      = UA_STRING("nmSequenceNr");
    structDef.fields[0].dataType  = UA_TYPES[UA_TYPES_INT16].typeId;
    structDef.fields[0].valueRank = UA_VALUERANK_SCALAR;

    structDef.fields[1].name      = UA_STRING("dswSequenceNr");
    structDef.fields[1].dataType  = UA_TYPES[UA_TYPES_INT16].typeId;
    structDef.fields[1].valueRank = UA_VALUERANK_SCALAR;

    structDef.fields[2].name      = UA_STRING("applicationStates");
    structDef.fields[2].dataType  = UA_TYPES[UA_TYPES_INT64].typeId;
    structDef.fields[2].valueRank = UA_VALUERANK_ONE_DIMENSION;

    UA_StructureDescription sd;
    UA_StructureDescription_init(&sd);
    sd.dataTypeId   = typeNodeId;
    sd.name         = UA_QUALIFIEDNAME(ns, "RedundancyStateType");
    sd.structureDefinition = structDef;

    UA_ExtensionObject ext;
    UA_ExtensionObject_init(&ext);
    ext.encoding = UA_EXTENSIONOBJECT_DECODED;
    ext.content.decoded.type = &UA_TYPES[UA_TYPES_STRUCTUREDESCRIPTION];
    ext.content.decoded.data = &sd;

    UA_StatusCode retval = UA_Server_addDataTypeFromDescription(server, &ext);

    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Failed to add RedundancyState type: 0x%08x", retval);
        return UA_STATUSCODE_BAD;
    }

    return UA_STATUSCODE_GOOD;
}

const UA_DataType*
getRedundancyType(UA_Server *server) {
    const UA_DataTypeArray *dataTypes = UA_Server_getDataTypes(server);
    while(dataTypes) {
        for(size_t i = 0; i < dataTypes->typesSize; i++) {
            if(UA_NodeId_equal(&dataTypes->types[i].typeId, &redundancyTypeId))
                return &dataTypes->types[i];
        }
        dataTypes = dataTypes->next;
    }
    return NULL;
}
