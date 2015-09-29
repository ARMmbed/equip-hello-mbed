/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "mbed.h"
#include "ble/BLE.h"
#include "ble-blocktransfer/BlockTransferService.h"
#include "voytalk/VoytalkHub.h"

#if 1
#define DEBUGOUT(...) { printf(__VA_ARGS__); }
#else
#define DEBUGOUT(...) /* nothing */
#endif // DEBUGOUT

#define DEBUG


const char DEVICE_NAME[] = "Nesvoy";
const UUID uuid(0xFE8E);

typedef enum {
    STATE_UNPROVISIONED_DISCONNECTED = 0x00,
    STATE_PROVISIONED_DISCONNECTED   = 0x01,

    STATE_UNPROVISIONED_CONNECTED    = 0x02 + 0x00,
    STATE_PROVISIONED_CONNECTED      = 0x02 + 0x01
} state_t;

volatile static uint8_t state;

/*****************************************************************************/
/* Global variables used by the test app                                     */
/*****************************************************************************/

// BLE_API ble device
BLE ble;

// Transfer large blocks of data on platforms without Fragmentation-And-Recombination
BlockTransferService bts;

// Voytalk handling
VoytalkHub vthub(DEVICE_NAME);

// buffer for sending and receiving data
SharedPointer<Block> writeBlock;
uint8_t readBuffer[200];
BlockStatic readBlock(readBuffer, sizeof(readBuffer));

// wifi parameters
std::string ssid_string;
std::string key_string;

/*****************************************************************************/
/* Functions for handling debug output                                       */
/*****************************************************************************/


/*
    Functions called when BLE device connects and disconnects.
*/
void whenConnected(const Gap::ConnectionCallbackParams_t* params)
{
    DEBUGOUT("main: Connected: %d %d %d\r\n", params->connectionParams->minConnectionInterval,
                                              params->connectionParams->maxConnectionInterval,
                                              params->connectionParams->slaveLatency);

    state |= 0x02;
}

void whenDisconnected(const Gap::DisconnectionCallbackParams_t*)
{
    DEBUGOUT("main: Disconnected!\r\n");
    DEBUGOUT("main: Restarting the advertising process\r\n");

    ble.gap().startAdvertising();

    state &= ~0x02;
}


/*****************************************************************************/
/* BlockTransfer callbacks for sending and receiving data                    */
/*****************************************************************************/

/*
    Function called when signaling client that new data is ready to be read.
*/
void blockServerSendNotification()
{
    DEBUGOUT("main: notify read updated\r\n");
    bts.updateCharacteristicValue((uint8_t*)"", 0);
}

/*
    Function called when device receives a read request over BLE.
*/
SharedPointer<Block> blockServerReadHandler(uint32_t offset)
{
    DEBUGOUT("main: block read\r\n");
    (void) offset;

    return SharedPointer<Block>(new BlockStatic(readBlock));
}

/*
    Function called when data has been written over BLE.
*/
void blockServerWriteHandler(SharedPointer<Block> block)
{
    DEBUGOUT("main: block write\r\n");

    /*
        Process received data, assuming it is CBOR encoded.
        Any output generated will be written to the readBlock.
    */
    vthub.processCBOR(block, &readBlock);

    /*
        If the readBlock length is non-zero it means a reply has been generated.
    */
    if (readBlock.getLength() > 0)
    {
        minar::Scheduler::postCallback(blockServerSendNotification);
    }
}

/*****************************************************************************/
/* Voytalk Wifi example                                                      */
/*****************************************************************************/

/*
    Callback function for constructing wifi intent.
*/
void wifiIntentConstruction(VoytalkHub& hub)
{
    DEBUGOUT("main: wifi intent construction\r\n");
    /* create intent using generated endpoint and constraint set */
    VoytalkIntent intent("com.arm.connectivity.wifi");

    /* serialize object tree to CBOR */
    hub.processIntent(intent);
}

void wifiIntentInvocation(VoytalkHub& hub, VoytalkIntentInvocation& object)
{
    DEBUGOUT("main: wifi invocation\r\n");

    // print object tree
    object.print();

    /////////////////////////////////////////
    // write to flash
    SharedPointer<CborBase> parameters = object.getParameters();

    CborMap* map = static_cast<CborMap*>(parameters.get());

    SharedPointer<CborBase> ssid = map->find("ssid");
    SharedPointer<CborBase> key = map->find("key");

    CborString* ssid_cbor = static_cast<CborString*>(ssid.get());
    CborString* key_cbor = static_cast<CborString*>(key.get());

    // copy strings from CBOR object to global strings
    ssid_string = ssid_cbor->getString();
    key_string = key_cbor->getString();

    /////////////////////////////////////////
    // create coda

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // notify client about new data in read characteristic
    minar::Scheduler::postCallback(blockServerSendNotification);

    // optional: change state inside Voytalk hub
    hub.setStateMask(0x02);

    state = STATE_PROVISIONED_CONNECTED;
}



/*****************************************************************************/
/* Reset device example                                                      */
/*****************************************************************************/


void resetIntentConstruction(VoytalkHub& hub)
{
    DEBUGOUT("main: reset intent construction\r\n");

    /* create intent using generated endpoint and constraint set */
    VoytalkIntent intent("com.arm.reset");

    /* serialize object tree to CBOR */
    hub.processIntent(intent);
}

void resetIntentInvocation(VoytalkHub& hub, VoytalkIntentInvocation& object)
{
    DEBUGOUT("main: reset invocation\r\n");

    // print object tree
    object.print();

    /////////////////////////////////////////
    // create coda

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // notify client about new data in read characteristic
    minar::Scheduler::postCallback(blockServerSendNotification);

    // optional: change state inside Voytalk hub
    hub.setStateMask(0x01);

    // change state in main application
    state = STATE_UNPROVISIONED_CONNECTED;
}

/*****************************************************************************/
/* Voytalk complex example                                                   */
/*****************************************************************************/

/*
    Callback functions for the example intent.
*/
void exampleIntentConstruction(VoytalkHub& hub)
{
    DEBUGOUT("main: complex example intent construction\r\n");

    /* create intent */
    VoytalkIntent intent("com.arm.examples.complex");

    /* serialize object tree to CBOR */
    hub.processIntent(intent);
}

void exampleIntentInvocation(VoytalkHub& hub, VoytalkIntentInvocation& object)
{
    DEBUGOUT("main: complex example invocation\r\n");

    // print object tree
    object.print();

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // notify client about new data in read characteristic
    minar::Scheduler::postCallback(blockServerSendNotification);
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/

void app_start(int, char *[])
{
    /*
        Register Voytalk intents in the hub.

        First parameter is the callback function for intent generation.
        Second is the callback function for when the intent is invoked.
        Third parameter is a bitmap for grouping intents together.
    */

    // Wifi provisioning intent
    vthub.registerIntent(wifiIntentConstruction,
                         wifiIntentInvocation,
                         0x01 | 0x02);

    // reset intent
    vthub.registerIntent(resetIntentConstruction,
                         resetIntentInvocation,
                         0x02);

    /*
        Set the current state mask.

        Mask is AND'ed with each intent's bitmap and only intents with non-zero
        results are displayed and can be invoked.
    */
    vthub.setStateMask(0x01);


    /*************************************************************************/
    /*************************************************************************/
    /* bluetooth le */
    ble.init();

    // status callback functions
    ble.gap().onConnection(whenConnected);
    ble.gap().onDisconnection(whenDisconnected);

    // set fastest connection interval based on Apple recommendations
    Gap::ConnectionParams_t fast;
    ble.getPreferredConnectionParams(&fast);
    fast.minConnectionInterval = 16; // 20 ms
    fast.maxConnectionInterval = 32; // 40 ms
    fast.slaveLatency = 0;
    ble.gap().setPreferredConnectionParams(&fast);

    /* construct advertising beacon */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED|GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                     (const uint8_t *) DEVICE_NAME, sizeof(DEVICE_NAME) - 1);

    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, uuid.getBaseUUID(), uuid.getLen());

    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1600); /* 1s; in multiples of 0.625ms. */

    // Apple uses device name instead of beacon name
    ble.gap().setDeviceName((const uint8_t*) DEVICE_NAME);

    /*************************************************************************/
    /*************************************************************************/
    // setup block transfer service

    // add service using ble device, responding to uuid, and without encryption
    bts.init(uuid, SecurityManager::SECURITY_MODE_ENCRYPTION_OPEN_LINK);

    // set callback functions for the BlockTransfer service
    bts.setWriteAuthorizationCallback(blockServerWriteHandler);
    bts.setReadAuthorizationCallback(blockServerReadHandler);

    // ble setup complete - start advertising
    ble.gap().startAdvertising();

    printf("Voytalk Test: %s %s\r\n", __DATE__, __TIME__);
}
