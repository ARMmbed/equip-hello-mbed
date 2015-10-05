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
#include "voytalk/Voytalk.h"

/*****************************************************************************/
/* Configuration                                                             */
/*****************************************************************************/

// set device name
const char DEVICE_NAME[] = "Testvoy";

// set TX power
#ifndef CFG_BLE_TX_POWER_LEVEL
#define CFG_BLE_TX_POWER_LEVEL 0
#endif

// control debug output
#if 1
#define DEBUGOUT(...) { printf(__VA_ARGS__); }
#else
#define DEBUGOUT(...) /* nothing */
#endif // DEBUGOUT

/*****************************************************************************/

/* Voytalk short UUID */
const UUID uuid(0xFE8E);

/* Voytalk states */
typedef enum {
    FLAG_CONNECTED      = 0x01,
    FLAG_PROVISIONED    = 0x02,
} flags_t;

static volatile uint8_t state;

/*****************************************************************************/
/* Global variables used by the test app                                     */
/*****************************************************************************/

// BLE_API ble device
BLE ble;

// Transfer large blocks of data on platforms without Fragmentation-And-Recombination
BlockTransferService bts;

// Voytalk handling
VoytalkRouter router(DEVICE_NAME);

// buffer for sending and receiving data
SharedPointer<Block> writeBlock;
uint8_t readBuffer[1000];
BlockStatic readBlock(readBuffer, sizeof(readBuffer));

// wifi parameters
std::string ssid_string;
std::string key_string;

// Compatibility function
void signalReady();

/*****************************************************************************/
/* Functions for handling debug output                                       */
/*****************************************************************************/

/*
    Functions called when BLE device connects and disconnects.
*/
void whenConnected(const Gap::ConnectionCallbackParams_t* params)
{
    (void) params;
    DEBUGOUT("main: Connected: %d %d %d\r\n", params->connectionParams->minConnectionInterval,
                                              params->connectionParams->maxConnectionInterval,
                                              params->connectionParams->slaveLatency);

    // change state in main application
    state |= FLAG_CONNECTED;

    // change state inside Voytalk hub
    router.setStateMask(state);
}

void whenDisconnected(const Gap::DisconnectionCallbackParams_t*)
{
    DEBUGOUT("main: Disconnected!\r\n");
    DEBUGOUT("main: Restarting the advertising process\r\n");

    ble.gap().startAdvertising();

    // change state in main application
    state &= ~FLAG_CONNECTED;

    // change state inside Voytalk hub
    router.setStateMask(state);
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
    router.processCBOR((BlockStatic*) block.get(), &readBlock);

    /*
        If the readBlock length is non-zero it means a reply has been generated.
    */
    if (readBlock.getLength() > 0)
    {
        signalReady();
    }
}

/*****************************************************************************/
/* Voytalk Wifi example                                                      */
/*****************************************************************************/

/*
    Callback function for constructing wifi intent.
*/
void wifiIntentConstruction(VTRequest& req, VTResponse& res)
{
    DEBUGOUT("main: wifi intent construction\r\n");
    /* create intent using generated endpoint and constraint set */
    VTIntent intent("com.arm.connectivity.wifi");
    intent.knownParameters("/networks");

    res.write(intent);
}
/*
void wifiIntentInvocation(VoytalkHub& hub, VoytalkIntentInvocation& object)
{
    DEBUGOUT("main: wifi invocation\r\n");

    // debug: print object tree
    object.print();

    /////////////////////////////////////////
    // retrieve parameters
    ssid_string = object.getParameters()->find("ssid")->getString();
    key_string = object.getParameters()->find("key")->getString();

    /////////////////////////////////////////
    // create coda

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);

    // change state in main application
    state |= FLAG_PROVISIONED;

    // change state inside Voytalk hub
    hub.setStateMask(state);
}*/



/*****************************************************************************/
/* Reset device example                                                      */
/*****************************************************************************/


void resetIntentConstruction(VTRequest& req, VTResponse& res)
{
    DEBUGOUT("main: reset intent construction\r\n");

    /* create intent using generated endpoint and constraint set */
    VTIntent intent("com.arm.reset");

    res.write(intent);
}
/*
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

    // change state in main application
    state &= ~FLAG_PROVISIONED;

    // change state inside Voytalk hub
    hub.setStateMask(state);
}
*/

/*****************************************************************************/
/* Voytalk complex example                                                   */
/*****************************************************************************/

/*
    Callback functions for the example intent.
*/
void exampleIntentConstruction(VTRequest& req, VTResponse& res)
{
    DEBUGOUT("main: complex example intent construction\r\n");

    /* create intent */
    VTIntent intent("com.arm.examples.complex");

    res.write(intent);
}
/*
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
}
*/

/*****************************************************************************/
/* Voytalk custom example                                                    */
/*****************************************************************************/

void customIntentConstruction(VTRequest& req, VTResponse& res)
{
    DEBUGOUT("main: custom intent construction\r\n");

    // /*  Define custom constraints.
    //     Note: constraints can be nested.
    // */

    // /* ssid, key are level 0 variables */
    // VoytalkConstraint L0_ssid("Network Name",
    //                           VoytalkConstraint::TypeString,
    //                           "ssid");

    // VoytalkConstraint L0_key("Password",
    //                          VoytalkConstraint::TypeString,
    //                          "key");

    // /* combine level 0 variables */
    // VoytalkConstraint* properties[] = { &L0_ssid,
    //                                     &L0_key };

    // // define level 0
    // VoytalkConstraint constraints("Custom Access",
    //                               properties);

    // /* optional: default values */
    // L0_ssid.setDefaultValue("homehub");

    // /* optional: description fields */
    // L0_ssid.setDescription("The name of the network you want to connect to.");
    // L0_key.setDescription("The password for the network.");

    // constraints.setDescription("The device wants to access your Wifi network.");

    // /* create intent using generated endpoint and constraint set */
    // VTIntent intent("com.arm.examples.custom", constraints);

    // callback(200, intent);
}
/*
void customIntentInvocation(VoytalkHub& hub, VoytalkIntentInvocation& object)
{
    DEBUGOUT("main: custom example invocation\r\n");

    // print object tree
    object.print();

    // Read ID from invocation. ID is returned in coda response.
    uint32_t invocationID = object.getID();

    // create coda and pass to Voytalk hub
    VoytalkCoda coda(invocationID, 1);
    hub.processCoda(coda);
}*/


void networkListResource(VTRequest& req, VTResponse& res, VoytalkRouter::done_t done)
{
    DEBUGOUT("listing network resources");

    VoytalkKnownParameters parameters(res, 2);

    parameters.parameter("com.arm.connectivity.wifi", 50)
        .map(2)
            .item("ssid", "miWifi")
            .item("key", "supersecurepassword");

    parameters.parameter("com.arm.connectivity.wifi", 20)
        .map(2)
            .item("ssid", "yoWifi")
            .item("key", "securepasswordinit");

    done(200);
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
    router.registerIntent(wifiIntentConstruction,
                          FLAG_CONNECTED | FLAG_PROVISIONED);

    // reset intent
    router.registerIntent(resetIntentConstruction,
                          FLAG_PROVISIONED);

    // custom intent
#if 0
    router.registerIntent(customIntentConstruction,
                         FLAG_CONNECTED | FLAG_PROVISIONED);
#endif

    /*
        Set the current state mask.

        Mask is AND'ed with each intent's bitmap and only intents with non-zero
        results are displayed and can be invoked.
    */
    router.setStateMask(0);

    /*
        Define some resource callbacks
    */

    router.get("/networks", networkListResource);
    router.post("/otherthing", networkListResource);

    /*************************************************************************/
    /*************************************************************************/
    /* bluetooth le */
    ble.init();

    // status callback functions
    ble.gap().onConnection(whenConnected);
    ble.gap().onDisconnection(whenDisconnected);

    /* construct advertising beacon */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED|GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::SHORTENED_LOCAL_NAME, (const uint8_t *) DEVICE_NAME, sizeof(DEVICE_NAME) - 1);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, uuid.getBaseUUID(), uuid.getLen());
    ble.gap().accumulateAdvertisingPayloadTxPower(CFG_BLE_TX_POWER_LEVEL);

    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(1000); /* 1s; in multiples of 0.625ms. */

    // set TX power
    ble.gap().setTxPower(CFG_BLE_TX_POWER_LEVEL);

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


/*****************************************************************************/
/* Compatibility                                                             */
/*****************************************************************************/

#if defined(YOTTA_MINAR_VERSION_STRING)
/*********************************************************/
/* Build for mbed OS                                     */
/*********************************************************/

void signalReady()
{
    minar::Scheduler::postCallback(blockServerSendNotification);
}

#else
/*********************************************************/
/* Build for mbed Classic                                */
/*********************************************************/

bool sendNotification = false;

void signalReady()
{
    sendNotification = true;
}

int main(void)
{
    app_start(0, NULL);

    for(;;)
    {
        // send notification outside of interrupt context
        if (sendNotification)
        {
            sendNotification = false;
            blockServerSendNotification();
        }

        ble.waitForEvent();
    }
}
#endif

