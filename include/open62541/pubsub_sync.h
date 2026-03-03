#ifndef OPEN62541_PUBSUB_SYNC_H
#define OPEN62541_PUBSUB_SYNC_H
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/server_config_default.h>
#include "open62541/pubsub_redundancy.h"

#define PUBLISHERID 2234
#define WRITERGROUDID 100
#define DATASETWRITERID 62541

#define PUBLISHINGINTERVAL 100
#define KEYFRAMECOUNT 10

//struct with Struct_s and isPrimary
typedef struct {
    State_s *dataType;
    UA_Boolean *isPrimary;
} cbstruct_s;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl);

static void
addPublishedDataSet(UA_Server *server);

static void
addStateDataField(UA_Server *server, State_s *state);

static void
addWriterGroup(UA_Server *server);

static void
addDataSetWriter(UA_Server *server);

static void
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *networkAddressUrl,
    UA_Boolean *isPrimary, State_s *state);

#endif
