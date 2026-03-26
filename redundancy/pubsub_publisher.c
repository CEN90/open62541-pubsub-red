#include "pubsub_publisher.h"

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include "include/pubsub_redundancy.h"

#define N 1
#define FAKEVALUE 62541

void
runPublisher(HeartbeatConfig *hb_config, UA_Boolean *isPrimary);

static UA_NodeId connectionIdentifier, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

typedef struct {
    UA_Boolean *isPrimary;
    UA_Int64 *val;
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
addStateVariable(UA_Server *server, UA_Int64 *fakeValue) {
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "SensorValue");
    attr.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
    attr.valueRank = -1;
    UA_Variant value;

    UA_Variant_setScalar(&value, fakeValue, &UA_TYPES[UA_TYPES_INT64]);
    attr.value = value;

    UA_NodeId stateNodeId = UA_NODEID_STRING(1, "SensorValue");
    UA_Server_addVariableNode(
        server, stateNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), UA_QUALIFIEDNAME(1, "SensorValue"),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Value %d",
                 *(int64_t *)value.data);
    return stateNodeId;
}

static void
addDataField(UA_Server *server, UA_Int64 *fakeValue) {
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("SensorValue");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        addStateVariable(server, fakeValue);
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
        UA_UADPDATASETMESSAGECONTENTMASK_SEQUENCENUMBER;

    UA_ExtensionObject_setValue(&dataSetWriterConfig.messageSettings,
                                &dataSetWriterMessage,
                                &UA_TYPES[UA_TYPES_UADPDATASETWRITERMESSAGEDATATYPE]);

    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

static void
updateFakeValue(UA_Server *server, void *data) {
    cb_t *cbData = (cb_t *)data;

    if(*cbData->isPrimary) {
        *(cbData->val) += 1;

        UA_Variant value;
        UA_Variant_init(&value);

        UA_Variant_setScalar(&value, cbData->val, &UA_TYPES[UA_TYPES_INT64]);
        UA_Server_writeValue(server, UA_NODEID_STRING(1, "SensorValue"), value);
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "SensorValue: %d",
                     *(cbData->val));
    }
}

void
runPublisher(HeartbeatConfig *hb_config, UA_Boolean *isPrimary) {
    UA_Int64 fakeValue = FAKEVALUE;
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl = {
        UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4842/")};

    UA_KeyValuePair *kps = UA_Array_new(N, &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);

    kps[0].key = UA_QUALIFIEDNAME(1, "FakeNews");
    UA_Variant_setScalar(&kps[0].value, &fakeValue, &UA_TYPES[UA_TYPES_INT64]);

    // common
    addPubSubConnection(server, &transportProfile, &networkAddressUrl);

    // publisher
    addPublishedDataSet(server);
    addDataField(server, &fakeValue);
    addWriterGroup(server);
    addDataSetWriter(server);

    cb_t callbackData = {isPrimary, &fakeValue};

    UA_Server_addRepeatedCallback(server, updateFakeValue, &callbackData,
                                  PUBLISHER_PUBLISHINGINTERVAL, NULL);

    UA_Server_enableAllPubSubComponents(server);

    initStateSync(isPrimary, server, hb_config, writerGroupIdent, dataSetWriterIdent, N,
                  kps);

    // Busy wait, refactor to cb?
    while(true) {
        syncState(N, kps);  // use retval later

        UA_Server_run_iterate(server, true);
    }

    UA_Server_delete(server);
}

int
main(int argc, char *argv[]) {
    const int port = 10001;
    UA_Boolean isPrimary = UA_FALSE;
    char redDcnIp[IPADDRLEN] = "127.0.0.1";

    if(argc == 3) {
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
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Usage: %s [--primary | --backup] <redundancy Controller IP>",
                     argv[0]);
        return -1;
    }

    HeartbeatConfig config = {
        .port = &port,
        .ipAddress = redDcnIp,
        .sockfd = NULL,
        .isPrimary = &isPrimary,
    };

    runPublisher(&config, &isPrimary);

    return 0;
}
