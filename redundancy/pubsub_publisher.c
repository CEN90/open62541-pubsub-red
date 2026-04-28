#include "pubsub_publisher.h"

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "include/pubsub_redundancy.h"

#define NUMFIELDS 10
#define FAKEVALUE 62541
#define LOOPSLEEPUS 25000

UA_Int16 runtime = 0;

void
runPublisher(HeartbeatConfig *hb_config, UA_Boolean *isPrimary, size_t numFields);

static UA_NodeId connectionIdentifier, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

typedef struct {
    UA_Boolean *isPrimary;
    UA_Boolean *firstMsgLogged;
    size_t numFields;
    UA_Int64 **values;
} cb_t;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl) {
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
    connectionConfig.publisherId.id.uint16 = PUBLISHER_PUBLISHERID;
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
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Create new PublishedDataSet based on the PublishedDataSetConfig. */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig,
                                  &publishedDataSetIdent);
}

static UA_NodeId
addStateVariable(UA_Server *server, UA_Int64 *values, size_t numFields) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "SensorValue");
    attr.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
    attr.valueRank = UA_VALUERANK_ONE_DIMENSION;
    
    UA_UInt32 dims[1] = {(UA_UInt32)numFields};
    attr.arrayDimensions = dims;
    attr.arrayDimensionsSize = UA_VALUERANK_ONE_DIMENSION;
    
    UA_Variant_init(&attr.value);
    UA_Variant_setArrayCopy(&attr.value, values, numFields, &UA_TYPES[UA_TYPES_INT64]);

    UA_NodeId stateNodeId = UA_NODEID_STRING(1, "SensorValue");
    UA_Server_addVariableNode(
        server, stateNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "SensorValue"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

    return stateNodeId;
}

static void
addDataField(UA_Server *server, UA_Int64 *values, size_t numFields) {
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("SensorValue");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        addStateVariable(server, values, numFields);
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
    writerGroupConfig.name = UA_STRING("Demo WriterGroup2");
    writerGroupConfig.publishingInterval = PUBLISHER_PUBLISHINGINTERVAL;
    writerGroupConfig.writerGroupId = PUBLISHER_WRITERGROUDID;
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
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter2");
    dataSetWriterConfig.dataSetWriterId = PUBLISHER_DATASETWRITERID;
    dataSetWriterConfig.keyFrameCount = PUBLISHER_KEYFRAMECOUNT;

    UA_UadpDataSetWriterMessageDataType dataSetWriterMessage;
    UA_UadpDataSetWriterMessageDataType_init(&dataSetWriterMessage);

    dataSetWriterMessage.dataSetMessageContentMask =
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER |
        UA_UADPDATASETMESSAGECONTENTMASK_STATUS;

    UA_ExtensionObject_setValue(&dataSetWriterConfig.messageSettings,
                                &dataSetWriterMessage,
                                &UA_TYPES[UA_TYPES_UADPDATASETWRITERMESSAGEDATATYPE]);

    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

static void
updateFakeValue(UA_Server *server, void *data) {
    cb_t *cbData = (cb_t *)data;
    
    if(!(*cbData->isPrimary)) {
        return;
    }

    /* Update all fields in-place */
    for(size_t i = 0; i < cbData->numFields; i++) {
        (*cbData->values)[i] += 1;
    }

    /* Write updated Int64[] to SensorValue */
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setArrayCopy(&value, *cbData->values, cbData->numFields,
                        &UA_TYPES[UA_TYPES_INT64]);

    UA_StatusCode ret =
        UA_Server_writeValue(server, UA_NODEID_STRING(1, "SensorValue"), value);

    if(ret != UA_STATUSCODE_GOOD) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to write SensorValue array. StatusCode: 0x%08x",
                       (unsigned int)ret);
        return;
    }

    if(!*(cbData->firstMsgLogged)) {
        *(cbData->firstMsgLogged) = true;
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "B3: SensorValue: %lld",
                    (long long)*cbData->values[0]);
    } else {
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "SensorValue: %lld",
                    (long long)*cbData->values[0]);
    }
}

static void
initKeyValuePairs(UA_Int64 *values, size_t numFields, const UA_Int64 baseValue) {
    for(size_t i = 0; i < numFields; i++) {
        values[i] = baseValue + (UA_Int64)i;
    }
}

static void
clearKeyValuePairs(UA_Int64 *values, size_t numFields) {
    for(size_t i = 0; i < numFields; i++) {
        values[i] = 0;
    }
}

void
runPublisher(HeartbeatConfig *hb_config, UA_Boolean *isPrimary, size_t numFields) {
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl = {
        UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4842/")};
    
    UA_Int64 *values = UA_Array_new(numFields, &UA_TYPES[UA_TYPES_INT64]);
    initKeyValuePairs(values, numFields, FAKEVALUE);

    // common
    addPubSubConnection(server, &transportProfile, &networkAddressUrl);

    // publisher
    addPublishedDataSet(server);
    addDataField(server, values, numFields);
    addWriterGroup(server);
    addDataSetWriter(server);

    UA_Boolean isFirstMsg = true;  // For regex parsing in testing
    UA_Boolean firstMsgLogged = false;
    cb_t callbackData = {isPrimary, &firstMsgLogged, numFields, &values};

    UA_Server_addRepeatedCallback(server, updateFakeValue, &callbackData,
                                  PUBLISHER_PUBLISHINGINTERVAL, NULL);

    UA_Server_enableAllPubSubComponents(server);

    initStateSync(isPrimary, server, hb_config, writerGroupIdent, dataSetWriterIdent, numFields,
                  values);

    if(!*isPrimary) {
        UA_Server_disableWriterGroup(server, writerGroupIdent);
    }

    sleep(2);

    while(true) {
        syncState(numFields, &values);  // use retval later

        if(*isPrimary && isFirstMsg) {
            UA_Server_enableWriterGroup(server, writerGroupIdent);
            UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                         "B2: Publisher is primary -> start publishing");
            isFirstMsg = false;
        }

        UA_Server_run_iterate(server, false);
        usleep(LOOPSLEEPUS);
    }

    clearKeyValuePairs(values, numFields);
    UA_Array_delete(values, numFields, &UA_TYPES[UA_TYPES_INT64]);

    UA_Server_delete(server);
}

int
main(int argc, char *argv[]) {
    const int port = 10001;
    UA_Boolean isPrimary = UA_FALSE;
    char redDcnIp[IPADDRLEN] = "127.0.0.1";
    size_t numFields = NUMFIELDS;

    if(argc >= 3) {
        strcpy(redDcnIp, argv[2]);

        if(strcmp(argv[1], "--primary") == 0)
            isPrimary = UA_TRUE;
        else if(strcmp(argv[1], "--backup") == 0)
            isPrimary = UA_FALSE;
        else {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid argument: %s",
                         argv[1]);
            return -1;
        }

        if(argc == 5) {
            if(strcmp(argv[3], "--numfields") == 0) {
                numFields = strtol(argv[4], NULL, 10);
            }
        }

    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Usage: %s [--primary | --backup] <redundancy Controller IP>",
                     argv[0]);
        return -1;
    }
    
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                 "numFields = %llu", (long long)numFields);

    HeartbeatConfig config = {
        .port = &port,
        .ipAddress = redDcnIp,
        .sockfd = NULL,
        .isPrimary = &isPrimary,
    };

    runPublisher(&config, &isPrimary, numFields);

    return 0;
}
