#include "open62541/types.h"

#include <open62541/common.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/server_config_default.h>

#define PUBLISHERID 4333
#define WRITERGROUDID 433
#define DATASETWRITERID 43

#define PUBLISHINGINTERVAL 1000
#define KEYFRAMECOUNT 10

#define POLLINGINTERVAL 1
#define DEADLINE UA_DATETIME_SEC

#define N 10


UA_Variant prevValue;
UA_DateTime prevTimeValChange;


static UA_NodeId connectionIdentifier,
                 readerGroupIdentifier,
                 readerIdentifier;

static UA_DataSetReaderConfig readerConfig;


static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl){
    /* Details about the connection configuration and handling are located
        * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 2");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                            &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    /* Changed to static publisherId from random generation to identify
        * the publisher on Subscriber side */
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    connectionConfig.publisherId.id.uint16 = PUBLISHERID;
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdentifier);
}


static void
addReaderGroup(UA_Server *server) {
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup2");
    UA_Server_addReaderGroup(server, connectionIdentifier, &readerGroupConfig,
                             &readerGroupIdentifier);
}

static void
fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData) {
    UA_DataSetMetaDataType_init (pMetaData);
    pMetaData->name = UA_STRING ("DataSet 2");

    /* Now there is only 1 field (Int64) */
    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData*)UA_Array_new (pMetaData->fieldsSize,
                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    /* Int64 DataType */
    UA_FieldMetaData_init (&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_INT64].typeId,
                   &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_INT64;
    pMetaData->fields[0].name =  UA_STRING ("SensorValue");
    pMetaData->fields[0].valueRank = UA_VALUERANK_ONE_DIMENSION; 
}

static void
addDataSetReader(UA_Server *server) {
    memset (&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 2");
    /* Parameters to filter which DataSetMessage has to be processed
     * by the DataSetReader */
    /* The following parameters are used to show that the data published by
     * tutorial_pubsub_publish.c is being subscribed and is being updated in
     * the information model */
    UA_UInt16 publisherIdentifier = PUBLISHERID;

    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = publisherIdentifier;
    readerConfig.writerGroupId    = WRITERGROUDID;
    readerConfig.dataSetWriterId  = DATASETWRITERID;

    UA_UadpDataSetReaderMessageDataType readerMessage;
    UA_UadpDataSetReaderMessageDataType_init(&readerMessage);

    readerMessage.dataSetMessageContentMask =
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER | 
        UA_UADPDATASETMESSAGECONTENTMASK_STATUS;

    UA_ExtensionObject_setValue(&readerConfig.messageSettings,
                                &readerMessage,
                                &UA_TYPES[UA_TYPES_UADPDATASETREADERMESSAGEDATATYPE]);

    /* Setting up Meta data configuration in DataSetReader */
    fillTestDataSetMetaData(&readerConfig.dataSetMetaData);

    UA_Server_addDataSetReader(server, readerGroupIdentifier, &readerConfig,
                               &readerIdentifier);
}

static void
addSubscribedVariables (UA_Server *server, UA_NodeId dataSetReaderId) {
    UA_NodeId folderId;
    UA_String folderName = readerConfig.dataSetMetaData.name;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    UA_QualifiedName folderBrowseName;
    if(folderName.length > 0) {
        oAttr.displayName.locale = UA_STRING ("en-US");
        oAttr.displayName.text = folderName;
        folderBrowseName.namespaceIndex = 1;
        folderBrowseName.name = folderName;
    }
    else {
        oAttr.displayName = UA_LOCALIZEDTEXT ("en-US", "Subscribed Variables");
        folderBrowseName = UA_QUALIFIEDNAME (1, "Subscribed Variables");
    }

    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NS0ID(OBJECTSFOLDER), UA_NS0ID(ORGANIZES),
                            folderBrowseName, UA_NS0ID(BASEOBJECTTYPE), oAttr, NULL, &folderId);

    /**
     * **TargetVariables**
     *
     * The SubscribedDataSet option TargetVariables defines a list of Variable
     * mappings between received DataSet fields and target Variables in the
     * Subscriber AddressSpace. The values subscribed from the Publisher are
     * updated in the value field of these variables */


    /* Create the TargetVariables with respect to DataSetMetaData fields */
    UA_FieldTargetDataType *targetVars = (UA_FieldTargetDataType *)
            UA_calloc(readerConfig.dataSetMetaData.fieldsSize, sizeof(UA_FieldTargetDataType));
    for(size_t i = 0; i < readerConfig.dataSetMetaData.fieldsSize; i++) {
        /* Variable to subscribe data */
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        UA_LocalizedText_copy(&readerConfig.dataSetMetaData.fields[i].description,
                              &vAttr.description);
        vAttr.displayName.locale = UA_STRING("en-US");
        vAttr.displayName.text = readerConfig.dataSetMetaData.fields[i].name;
        vAttr.dataType = readerConfig.dataSetMetaData.fields[i].dataType;

        UA_NodeId newNode;
        /* With only one field (index 0), map to the.int64 node */
        UA_NodeId targetNodeId = UA_NODEID_STRING(1, "SensorValue");

        UA_Server_addVariableNode(server, targetNodeId,
                                  folderId, UA_NS0ID(HASCOMPONENT),
                                  UA_QUALIFIEDNAME(1, (char *)readerConfig.dataSetMetaData.fields[i].name.data),
                                  UA_NS0ID(BASEDATAVARIABLETYPE),
                                  vAttr, NULL, &newNode);

        /* For creating Targetvariables */
        targetVars[i].attributeId  = UA_ATTRIBUTEID_VALUE;
        targetVars[i].targetNodeId = targetNodeId;
    }

    UA_Server_DataSetReader_createTargetVariables(server, dataSetReaderId,
                                                  readerConfig.dataSetMetaData.fieldsSize,
                                                  targetVars);

    UA_free(targetVars);
    UA_free(readerConfig.dataSetMetaData.fields);
}


static void
onPollLogValue(UA_Server *server, void *data) {
    UA_DateTimeStruct test = UA_DateTime_toStruct(prevTimeValChange);
    
    UA_LOG_INFO(
        UA_Log_Stdout,
        UA_LOGCATEGORY_SERVER, 
        "Value: %lld -> %lld ms, %lld us", 
        *(UA_Int64*)prevValue.data,
        test.milliSec, test.microSec
    );
}

static void
onPollingEventSimple(UA_Server *server, void *data) {
    UA_Variant value;
    UA_Variant_init(&value);

    UA_StatusCode retval = UA_Server_readArrayDimensions(server, UA_NODEID_STRING(1, "SensorValue"), &value);

    if(retval == UA_STATUSCODE_GOOD) {
        UA_DateTime now = UA_DateTime_nowMonotonic();
        
        // No previous value, initialize only
        if(prevTimeValChange == 0 || prevValue.type == NULL) {
            prevTimeValChange = now;
            prevValue = value;
            return;
        }
        
        UA_DateTime delta = now - prevTimeValChange;
        
        // Old value, check if time exceeded
        if(*(UA_Int64*) value.data == *(UA_Int64*) prevValue.data) {
            if(delta > DEADLINE) {
                prevTimeValChange = now;
                UA_DateTimeStruct test = UA_DateTime_toStruct(delta);
                UA_LOG_ERROR(
                    UA_Log_Stdout, 
                    UA_LOGCATEGORY_SERVER, 
                    "S1: Deadline not held -> %lld ms, %lld us", 
                    test.milliSec, test.microSec
                );
            }
        } else {
            // New value, update timestamp and value
            prevValue = value;
            prevTimeValChange = now;
        }        
    }
}


int
main(int argc, char *argv[]) {

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    UA_Variant_init(&prevValue);
    UA_DateTime_init(&prevTimeValChange);
    
    // common
    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl =
        {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4842/")};
    addPubSubConnection(server, &transportProfile, &networkAddressUrl);

    // subscriber
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdentifier);

    // CB for publishing/reading state
    UA_Server_addRepeatedCallback(server,
                                onPollingEventSimple,
                                NULL,
                                POLLINGINTERVAL,
                                NULL);
    UA_Server_addRepeatedCallback(server,
                                onPollLogValue,
                                NULL,
                                PUBLISHINGINTERVAL,
                                NULL);

    UA_Server_enableAllPubSubComponents(server);
    UA_Server_runUntilInterrupt(server);

    UA_Server_delete(server);
    return 0;
}
