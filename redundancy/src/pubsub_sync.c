#include "../include/pubsub_sync.h"

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include "../include/redundancy_state.h"

static UA_NodeId connectionIdentifier, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent, readerGroupIdentifier, readerIdentifier;
    
static UA_NodeId appWriterGroupId, appDataSetReaderGroupId;
static UA_Server *appServer;

static UA_DataSetReaderConfig readerConfig;
static UA_Boolean lastIsPrimary = UA_FALSE;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl) {
    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 1");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    /* Changed to static publisherId from random generation to identify
     * the publisher on Subscriber side */
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    connectionConfig.publisherId.id.uint16 = PUBLISHERID;
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdentifier);
}

/****************************************************************************************
 * Publisher functions
 ****************************************************************************************
 */

static void
addPublishedDataSet(UA_Server *server) {
    /* The PublishedDataSetConfig contains all necessary public
     * information for the creation of a new PublishedDataSet */
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Prototype PDS");
    /* Create new PublishedDataSet based on the PublishedDataSetConfig. */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig,
                                  &publishedDataSetIdent);
}

static UA_NodeId
addStateVariable(UA_Server *server, RedundancyState_s *stateStruct) {
    const UA_DataType *redundancyType = getRedundancyType(server);
    if(!redundancyType) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "RedundancyState type not found in server");
        return UA_NODEID_NULL;
    }

    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                 "Type found: %s memSize=%u membersSize=%u",
                 redundancyType->typeName,
                 redundancyType->memSize,
                 redundancyType->membersSize);


    if(redundancyType->memSize == 0 || redundancyType->membersSize == 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Type descriptor is corrupt (memSize=%u, members=%u)",
                     redundancyType->memSize, redundancyType->membersSize);
        return UA_NODEID_NULL;
    }

    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "state");
    attr.dataType    = redundancyType->typeId;
    attr.valueRank   = UA_VALUERANK_SCALAR;

    UA_Variant value;
    UA_Variant_init(&value);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                 "sizeof(RedundancyState_s)=%zu redundancyType->memSize=%u",
                 sizeof(RedundancyState_s), redundancyType->memSize);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                 "stateStruct=%p nmSeq=%d dswSeq=%d arraySize=%zu arrayPtr=%p",
                 (void*)stateStruct,
                 stateStruct->nmSequenceNr,
                 stateStruct->dswSequenceNr,
                 stateStruct->applicationStatesSize,
                 (void*)stateStruct->applicationStates);
    UA_StatusCode rc = UA_Variant_setScalarCopy(&value, stateStruct, redundancyType);
    if(rc != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Variant copy failed: 0x%08x", rc);
        return UA_NODEID_NULL;
    }
    attr.value = value;

    UA_NodeId stateNodeId = UA_NODEID_STRING(1, "state");
    UA_StatusCode addRc = UA_Server_addVariableNode(
        server, stateNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
        UA_QUALIFIEDNAME(1, "state"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        attr, NULL, NULL);

    UA_Variant_clear(&value);

    if(addRc != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "addVariableNode failed: 0x%08x", addRc);
        return UA_NODEID_NULL;
    }

    return stateNodeId;
}

static void
addStateDataField(UA_Server *server, RedundancyState_s *stateStruct,
                  UA_NodeId *stateNodeId) {
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;

    *stateNodeId = addStateVariable(server, stateStruct);
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("state");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable = *stateNodeId;
    dataSetFieldConfig.field.variable.publishParameters.attributeId =
        UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dataSetFieldConfig,
                              &dataSetFieldIdent);
}

static void
addWriterGroup(UA_Server *server) {
    /* Now we create a new WriterGroupConfig and add the group to the existing
     * PubSubConnection. */
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = PUBLISHINGINTERVAL;
    writerGroupConfig.writerGroupId = WRITERGROUDID;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;

    /* Change message settings of writerGroup to send PublisherId,
     * WriterGroupId in GroupHeader and DataSetWriterId in PayloadHeader
     * of NetworkMessage */
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask =
        (UA_UadpNetworkMessageContentMask)(UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_SEQUENCENUMBER |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);

    /* The configuration flags for the messages are encapsulated inside the
     * message- and transport settings extension objects. These extension
     * objects are defined by the standard. e.g.
     * UadpWriterGroupMessageDataType */
    UA_ExtensionObject_setValue(&writerGroupConfig.messageSettings, &writerGroupMessage,
                                &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE]);

    UA_Server_addWriterGroup(server, connectionIdentifier, &writerGroupConfig,
                             &writerGroupIdent);
}

static void
addDataSetWriter(UA_Server *server) {
    /* We need now a DataSetWriter within the WriterGroup. This means we must
     * create a new DataSetWriterConfig and add call the addWriterGroup function. */
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = DATASETWRITERID;
    dataSetWriterConfig.keyFrameCount = KEYFRAMECOUNT;

    UA_UadpDataSetWriterMessageDataType dataSetWriterMessage;
    UA_UadpDataSetWriterMessageDataType_init(&dataSetWriterMessage);

    dataSetWriterMessage.dataSetMessageContentMask =
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER;

    UA_ExtensionObject_setValue(&dataSetWriterConfig.messageSettings,
                                &dataSetWriterMessage,
                                &UA_TYPES[UA_TYPES_UADPDATASETWRITERMESSAGEDATATYPE]);

    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

/****************************************************************************************
 * Subscriber functions
 ****************************************************************************************
 */

static void
addReaderGroup(UA_Server *server) {
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    UA_Server_addReaderGroup(server, connectionIdentifier, &readerGroupConfig,
                             &readerGroupIdentifier);
}

static void
addDataSetReader(UA_Server *server) {
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 1");
    /* Parameters to filter which DataSetMessage has to be processed
     * by the DataSetReader */
    /* The following parameters are used to show that the data published by
     * tutorial_pubsub_publish.c is being subscribed and is being updated in
     * the information model */
    UA_UInt16 publisherIdentifier = PUBLISHERID;

    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = publisherIdentifier;
    readerConfig.writerGroupId = WRITERGROUDID;
    readerConfig.dataSetWriterId = DATASETWRITERID;

    UA_UadpDataSetReaderMessageDataType readerMessage;
    UA_UadpDataSetReaderMessageDataType_init(&readerMessage);

    readerMessage.dataSetMessageContentMask =
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER;

    UA_ExtensionObject_setValue(&readerConfig.messageSettings, &readerMessage,
                                &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE]);

    /* Setting up Meta data configuration in DataSetReader */
    fillTestDataSetMetaData(&readerConfig.dataSetMetaData,server);

    UA_Server_addDataSetReader(server, readerGroupIdentifier, &readerConfig,
                               &readerIdentifier);
}

static void
addSubscribedVariables(UA_Server *server, UA_NodeId dataSetReaderId,
                       UA_NodeId *stateNodeId) {
    UA_NodeId folderId;
    UA_String folderName = readerConfig.dataSetMetaData.name;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    UA_QualifiedName folderBrowseName;
    if(folderName.length > 0) {
        oAttr.displayName.locale = UA_STRING("en-US");
        oAttr.displayName.text = folderName;
        folderBrowseName.namespaceIndex = 1;
        folderBrowseName.name = folderName;
    } else {
        oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Subscribed Variables");
        folderBrowseName = UA_QUALIFIEDNAME(1, "Subscribed Variables");
    }

    UA_Server_addObjectNode(server, UA_NODEID_NULL, UA_NS0ID(OBJECTSFOLDER),
                            UA_NS0ID(ORGANIZES), folderBrowseName,
                            UA_NS0ID(BASEOBJECTTYPE), oAttr, NULL, &folderId);

    /**
     * **TargetVariables**
     *
     * The SubscribedDataSet option TargetVariables defines a list of Variable
     * mappings between received DataSet fields and target Variables in the
     * Subscriber AddressSpace. The values subscribed from the Publisher are
     * updated in the value field of these variables */

    /* Create the TargetVariables with respect to DataSetMetaData fields */
    UA_FieldTargetDataType *targetVars = (UA_FieldTargetDataType *)UA_calloc(
        readerConfig.dataSetMetaData.fieldsSize, sizeof(UA_FieldTargetDataType));
    for(size_t i = 0; i < readerConfig.dataSetMetaData.fieldsSize; i++) {
        /* Variable to subscribe data */
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        UA_LocalizedText_copy(&readerConfig.dataSetMetaData.fields[i].description,
                              &vAttr.description);
        vAttr.displayName.locale = UA_STRING("en-US");
        vAttr.displayName.text = readerConfig.dataSetMetaData.fields[i].name;
        vAttr.dataType = readerConfig.dataSetMetaData.fields[i].dataType;

        UA_NodeId newNode;
        UA_NodeId targetNodeId = UA_NODEID_STRING(1, "state");
        // UA_NodeId targetNodeId = *stateNodeId;

        UA_Server_addVariableNode(
            server, targetNodeId, folderId, UA_NS0ID(HASCOMPONENT),
            UA_QUALIFIEDNAME(1, (char *)readerConfig.dataSetMetaData.fields[i].name.data),
            UA_NS0ID(BASEDATAVARIABLETYPE), vAttr, NULL, &newNode);

        /* For creating Targetvariables */
        targetVars[i].attributeId = UA_ATTRIBUTEID_VALUE;
        targetVars[i].targetNodeId = targetNodeId;
    }

    UA_Server_DataSetReader_createTargetVariables(
        server, dataSetReaderId, readerConfig.dataSetMetaData.fieldsSize, targetVars);

    UA_free(targetVars);
    UA_free(readerConfig.dataSetMetaData.fields);
}

static void
fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData, UA_Server *server) {
    UA_DataSetMetaDataType_init(pMetaData);
    pMetaData->name = UA_STRING("RedundancyStateDataSet");

    const UA_DataType *redundancyType = getRedundancyType(server);
    if(!redundancyType) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "RedundancyState type not found in server");
    }

    /* Now there is only 1 field (Int16) */
    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData *)UA_Array_new(
        pMetaData->fieldsSize, &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init(&pMetaData->fields[0]);
    UA_NodeId_copy(&redundancyType->typeId, &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_STRUCTURE;
    pMetaData->fields[0].name = UA_STRING("state");
    pMetaData->fields[0].valueRank = -1; /* scalar */
}

/********************************************************************************
 * Common functions
 ********************************************************************************
 */

static void
readSyncState(UA_Server *server, RedundancyState_s *stateStruct) {
    UA_Variant value;
    UA_Variant_init(&value);

    UA_StatusCode retval =
        UA_Server_readValue(server, UA_NODEID_STRING(1, "state"), &value);

    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Failed to read state: 0x%08x",
                     retval);
        UA_Variant_clear(&value);
        return;
    }

    if(value.data == NULL) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "state has no data");
        UA_Variant_clear(&value);
        return;
    }

    const UA_DataType *redundancyType = getRedundancyType(server);
    if(!redundancyType) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "RedundancyState type not found in server");
    }
    if(redundancyType) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Subscriber type registered: %s ns=%u id=%u memSize=%u",
                    redundancyType->typeName,
                    redundancyType->typeId.namespaceIndex,
                    redundancyType->typeId.identifier.numeric,
                    redundancyType->memSize);
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Subscriber type NOT registered");
    }

    if(value.type != NULL &&
       value.arrayLength == 0 &&
       UA_NodeId_equal(&value.type->typeId, &redundancyType->typeId)) {

        RedundancyState_s *remoteState = (RedundancyState_s *)value.data;
        stateStruct->nmSequenceNr  = remoteState->nmSequenceNr;
        stateStruct->dswSequenceNr = remoteState->dswSequenceNr;

        /* Free old array if present */
        if(stateStruct->applicationStates && stateStruct->applicationStatesSize > 0) {
            UA_Array_delete(stateStruct->applicationStates,
                            stateStruct->applicationStatesSize,
                            &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);
            stateStruct->applicationStates = NULL;
            stateStruct->applicationStatesSize = 0;
        }

        if(remoteState->applicationStatesSize > 0) {
            /* Deep copy the UA_String array */
            UA_StatusCode rc = UA_Array_copy(
                remoteState->applicationStates,
                remoteState->applicationStatesSize,
                (void**)&stateStruct->applicationStates,
                &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);
            if(rc == UA_STATUSCODE_GOOD)
                stateStruct->applicationStatesSize = remoteState->applicationStatesSize;
            else
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                             "Failed to copy applicationStates: 0x%08x", rc);
        }

        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                    "Sync state read: nmSeq=%d dswSeq=%d arraySize=%lu",
                    stateStruct->nmSequenceNr, stateStruct->dswSequenceNr,
                    (unsigned long)stateStruct->applicationStatesSize);
        
        UA_StatusCode retval = UA_Server_setWriterGroupSequenceNumber(appServer, appWriterGroupId, stateStruct->dswSequenceNr + 1);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to set WriterGroup seq nr, "
                           "StatusCode: 0x%08x",
                           retval);
        }
        
        retval = UA_Server_setDataSetWriterSequenceNumber(appServer, appDataSetReaderGroupId, stateStruct->nmSequenceNr + 1);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to set dataSetWriter seq nr, "
                           "StatusCode: 0x%08x",
                           retval);
        }
        
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "Variant type mismatch — typeId ns=%u id=%u kind=%u",
                     value.type ? value.type->typeId.namespaceIndex : 0,
                     value.type ? value.type->typeId.identifier.numeric : 0,
                     value.type ? value.type->typeKind : 999);
    }

    UA_Variant_clear(&value);
}

static void
setSyncState(UA_Server *server, RedundancyState_s *stateStruct) {
    UA_Variant value;
    UA_Variant_init(&value);

    const UA_DataType *redundancyType = getRedundancyType(server);
    if(!redundancyType) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "RedundancyState type not found in server");
    }

    UA_Variant_setScalarCopy(&value, stateStruct, redundancyType);
    UA_StatusCode retval =
        UA_Server_writeValue(server, UA_NODEID_STRING(1, "state"), value);

    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                     "Failed to write RedundancyState: 0x%08x", retval);
    }
    // else {
    //     UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
    //                 "RedundancyState written: nmSeq=%d dsmSeq=%d arraySize=%lu",
    //                 stateStruct->nmSequenceNr, stateStruct->dswSequenceNr,
    //                 (unsigned long)stateStruct->applicationStatesSize);
    // }
}

static void
onDemandSync(UA_Server *server, void *data) {
    cbstruct_s *stateStruct = (cbstruct_s *)data;
    UA_Boolean currentIsPrimary = *stateStruct->isPrimary;

    /* Check for role change: Backup -> Primary failover */
    if(currentIsPrimary && !lastIsPrimary) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                    "FAILOVER TRIGGERED: Taking over as PRIMARY");

        /* Disable reader group (stop listening) */
        UA_StatusCode retval =
            UA_Server_disableReaderGroup(server, readerGroupIdentifier);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                         "Failed to disable ReaderGroup: 0x%08x", retval);
        }

        /* Enable writer group (start publishing) */
        retval = UA_Server_enableWriterGroup(server, writerGroupIdent);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                         "Failed to enable WriterGroup: 0x%08x", retval);
        }

        lastIsPrimary = currentIsPrimary;
    }

    if(currentIsPrimary) {
        /* PRIMARY: Publish state */
        setSyncState(server, stateStruct->data);
        UA_Server_WriterGroup_publish(server, writerGroupIdent);
    } else {
        /* BACKUP: Read state */
        readSyncState(server, stateStruct->data);
    }
}

static void
setupPubSub(UA_Boolean *isPrimary, RedundancyState_s *state, connectionConfig_s *config) {
    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl = {
        UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};

    runPubSub(&transportProfile, &networkAddressUrl, isPrimary, state);
}

void *
initSync(void *data) {
    cbstruct_s *stateStruct = (cbstruct_s *)data;
    UA_Boolean *isPrimary = stateStruct->isPrimary;
    appDataSetReaderGroupId = stateStruct->DatasetReaderGroupId;
    appWriterGroupId = stateStruct->writerGroupId;
    appServer = stateStruct->server;
    RedundancyState_s *state = stateStruct->data;
    setupPubSub(isPrimary, state, NULL);

    return NULL;
}

void
runPubSub(UA_String *transportProfile, UA_NetworkAddressUrlDataType *networkAddressUrl,
          UA_Boolean *isPrimary, RedundancyState_s *state) {

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_NodeId stateNodeId;

    UA_ServerConfig_setDefault(config);

    registerRedundancyStateType(server);

    const UA_DataType *t = getRedundancyType(server);
    if(t) {
        UA_DataTypeArray *customTypes = (UA_DataTypeArray*)UA_malloc(sizeof(UA_DataTypeArray));
        customTypes->next  = config->customDataTypes;
        customTypes->types = t;
        customTypes->typesSize = 1;
        config->customDataTypes = customTypes;
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "Custom type pushed to PubSub config: %s", t->typeName);
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Type not found, PubSub will not decode correctly");
    }


    // common
    addPubSubConnection(server, transportProfile, networkAddressUrl);

    // publisher
    addPublishedDataSet(server);
    addStateDataField(server, state, &stateNodeId);
    addWriterGroup(server);
    addDataSetWriter(server);

    // subscriber
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdentifier, &stateNodeId);

    UA_Server_enableAllPubSubComponents(server);

    cbstruct_s stateStruct = {state, isPrimary};
    lastIsPrimary = *isPrimary;

    if(*isPrimary) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                    "Starting as primary: Enabling WriterGroup, disabling ReaderGroup");
        UA_Server_enableWriterGroup(server, writerGroupIdent);
        UA_Server_disableReaderGroup(server, readerGroupIdentifier);
    } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER,
                    "Starting as backup: Disabling WriterGroup, enabling ReaderGroup");
        UA_Server_disableWriterGroup(server, writerGroupIdent);
        UA_Server_enableReaderGroup(server, readerGroupIdentifier);
    }

    // CB for publishing/reading state
    UA_Server_addRepeatedCallback(server, onDemandSync, &stateStruct, PUBLISHINGINTERVAL,
                                  NULL);

    UA_Server_runUntilInterrupt(server);

    UA_Server_delete(server);
}
