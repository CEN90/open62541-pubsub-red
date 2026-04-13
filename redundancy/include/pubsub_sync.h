#ifndef OPEN62541_PUBSUB_SYNC_H
#define OPEN62541_PUBSUB_SYNC_H
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#include "../include/redundancy_state.h"
#include "pubsub_redundancy.h"
#include "redundancy_state.h"

#define PUBLISHERID 2234
#define WRITERGROUDID 100
#define DATASETWRITERID 62541

#define PUBLISHINGINTERVAL 100
#define KEYFRAMECOUNT 10

// struct with Struct_s and isPrimary
typedef struct {
    RedundancyState_s *data;
    UA_Boolean *isPrimary;
    UA_Server *server;
    UA_NodeId writerGroupId;
    UA_NodeId DatasetReaderGroupId;
} cbstruct_s;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl);

static void
addPublishedDataSet(UA_Server *server);

static void
addStateDataField(UA_Server *server, RedundancyState_s *state, UA_NodeId *stateNodeId);

static void
addWriterGroup(UA_Server *server);

static void
addDataSetWriter(UA_Server *server);

static void
addReaderGroup(UA_Server *server);

static void
addDataSetReader(UA_Server *server);

static void
addSubscribedVariables(UA_Server *server, UA_NodeId dataSetReaderId,
                       UA_NodeId *stateNodeId);

static void
fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData, UA_Server *server);

// Test functions
static void
printServerVars(UA_Server *server);

static void
callbackPrintVars(UA_Server *server, void *data);

static void
onDemandSync(UA_Server *server, void *data);

static void
setupPubSub(UA_Boolean *isPrimary, RedundancyState_s *state, connectionConfig_s *config);

void *
initSync(void *data);

void
runPubSub(UA_String *transportProfile, UA_NetworkAddressUrlDataType *networkAddressUrl,
          UA_Boolean *isPrimary, RedundancyState_s *state);

#endif
