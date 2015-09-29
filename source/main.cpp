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
#include "BLE.h"
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
BlockTransferService* bts;

// Voytalk handling
VoytalkHub vthub(DEVICE_NAME);

// buffer for sending and receiving data
block_t writeBlock;
uint8_t writeBuffer[200];

block_t readBlock;
uint8_t readBuffer[200];

// flag for signaling inside interrupt context
bool sendNotify = false;

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

void whenDisconnected(Gap::Handle_t handle, Gap::DisconnectionReason_t reasons)
{
    DEBUGOUT("main: Disconnected!\r\n");
    DEBUGOUT("main: Restarting the advertising process\r\n");

    ble.startAdvertising();
    
    state &= ~0x02;
}


/*****************************************************************************/
/* BlockTransfer callbacks for sending and receiving data                    */
/*****************************************************************************/

/*
    Function called when device receives a read request over BLE.
*/
void blockServerReadHandler(block_t* block)
{
    DEBUGOUT("main: block read\r\n");

    // assign buffer to be read
    block->data = readBlock.data;
    block->length = readBlock.length;

    // re-enable writes
    writeBlock.maxLength = sizeof(writeBuffer);
}

/*
    Function called when data has been written over BLE.
*/
block_t* blockServerWriteHandler(block_t* block)
{
    DEBUGOUT("main: block write\r\n");

    /*
        Process received data, assuming it is CBOR encoded.
        Any output generated will be written to the readBlock.
    */
    vthub.processCBOR(block, &readBlock);

    /*
        If the readBlock length is non-zero it means a reply has been generated.
        Set sendNotify flag. This sends a reply outside interrupt context.
    */
    if (readBlock.length > 0)
    {
        // disable write until value has been read
        writeBlock.maxLength = 0;

        sendNotify = true;
    }

    /*
        Return block to BlockTransfer Service.
        This can be a different block than the one just received.
    */
    return block;
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

    // queue write to flash 
    writeFlash = true;
    
    /////////////////////////////////////////
    // create coda

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // disable write until value has been read
    writeBlock.maxLength = 0;

    // notify client about new data in read characteristic
    sendNotify = true;

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
    // erase flash
    flash->internalErase();

    /////////////////////////////////////////
    // create coda

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // disable write until value has been read
    writeBlock.maxLength = 0;

    // notify client about new data in read characteristic
    sendNotify = true;

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

    // disable write until value has been read
    writeBlock.maxLength = 0;

    // notify client about new data in read characteristic
    sendNotify = true;
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/

int main(void)
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
    ble.onConnection(whenConnected);
    ble.onDisconnection(whenDisconnected);

    // set fastest connection interval based on Apple recommendations
    Gap::ConnectionParams_t fast;
    ble.getPreferredConnectionParams(&fast);
    fast.minConnectionInterval = 16; // 20 ms
    fast.maxConnectionInterval = 32; // 40 ms
    fast.slaveLatency = 0;
    ble.setPreferredConnectionParams(&fast);

    /* construct advertising beacon */
    ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED|GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME,
                                     (const uint8_t *) DEVICE_NAME, sizeof(DEVICE_NAME) - 1);

    ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, uuid.getBaseUUID(), uuid.getLen());

    ble.setAdvertisingPayload();

    ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.setAdvertisingInterval(1600); /* 1s; in multiples of 0.625ms. */

    // Apple uses device name instead of beacon name
    ble.setDeviceName((const uint8_t*) DEVICE_NAME);

    /*************************************************************************/
    /*************************************************************************/
    // setup block transfer service

    // block for receiving data
    writeBlock.data = writeBuffer;
    writeBlock.length = 0;
    writeBlock.maxLength = sizeof(writeBuffer);
    writeBlock.offset = 0;

    // block for sending data
    readBlock.data = readBuffer;
    readBlock.length = 0;
    readBlock.maxLength = sizeof(readBuffer);
    readBlock.offset = 0;

    // add service using ble device, responding to uuid, and without encryption
    bts = new BlockTransferService(ble, uuid, SecurityManager::SECURITY_MODE_ENCRYPTION_OPEN_LINK);

    // set callback functions for the BlockTransfer service
    bts->setWriteAuthorizationCallback(blockServerWriteHandler, &writeBlock);
    bts->setReadAuthorizationCallback(blockServerReadHandler);

    // ble setup complete - start advertising
    ble.startAdvertising();


    for(;;)
    {
        /* signal central that data is ready to be read. */
        if (sendNotify)
        {
            sendNotify = false;

            DEBUGOUT("main: notify read updated\r\n");
            bts->updateCharacteristicValue(readBuffer, 0);
        }

        // go to sleep
        ble.waitForEvent();
    }
}
