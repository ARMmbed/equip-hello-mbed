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
#include "equip/Equip.h"

// todo: use explicit namespaces?
using namespace Equip;

/*****************************************************************************/
/* Configuration                                                             */
/*****************************************************************************/

// set device name
const char DEVICE_NAME[] = "mbed Provisioning";

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

#define VERBOSE_DEBUG_OUT 1

/*****************************************************************************/

/* short UUID */
const UUID uuid(0xFE8E);

/* states */
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

// variables for sending and receiving data
uint8_t readBuffer[1000];
SharedPointer<Block> readBlock(new BlockStatic(readBuffer, sizeof(readBuffer)));
SharedPointer<BlockStatic> writeBlock;

// wifi parameters
std::string ssid_string;
std::string key_string;

// Compatibility function
void signalReady();

void onResponseFinished(const Response& res)
{
    DEBUGOUT("main: output buffer usage: %lu of %lu\r\n", readBlock->getLength(), readBlock->getMaxLength());
    (void) res;

    // signal response is ready to be read
    signalReady();

    // free reference counted block
    writeBlock = SharedPointer<BlockStatic>();
}

// mbed Provisioing App client library
Router router(DEVICE_NAME, onResponseFinished);


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

    // change state inside router
    router.setStateMask(state);
}

void whenDisconnected(const Gap::DisconnectionCallbackParams_t*)
{
    DEBUGOUT("main: Disconnected!\r\n");
    DEBUGOUT("main: Restarting the advertising process\r\n");

    ble.gap().startAdvertising();

    // change state in main application
    state &= ~FLAG_CONNECTED;

    // change state inside router
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
    DEBUGOUT("main: block read: %lu\r\n", readBlock->getLength());
    (void) offset;

#if VERBOSE_DEBUG_OUT
    /* print generated output */
    if (readBlock->getLength() > 0)
    {
        for (size_t idx = 0; idx < readBlock->getLength(); idx++)
        {
            DEBUGOUT("%02X", readBlock->at(idx));
        }
        DEBUGOUT("\r\n\r\n");
    }
#endif

    return readBlock;
}

/*
    Function called when data has been written over BLE.
*/
void blockServerWriteHandler(SharedPointer<BlockStatic> block)
{
    DEBUGOUT("main: block write\r\n");

#if VERBOSE_DEBUG_OUT
    DEBUGOUT("main write:\r\n");
    for (size_t idx = 0; idx < block->getLength(); idx++)
    {
        DEBUGOUT("%02X", block->at(idx));
    }
    DEBUGOUT("\r\n\r\n");
#endif

    /*
        Process received data, assuming it is CBOR encoded.
        Any output generated will be written to the readBlock.
    */

    DEBUGOUT("main: input buffer usage: %lu of %lu\r\n", block->getLength(), block->getMaxLength());

    router.processCBOR((BlockStatic*) block.get(), (BlockStatic*) readBlock.get());

    // save block until we are done processing this request
    writeBlock = block;
}

/*****************************************************************************/
/* Wifi example                                                              */
/*****************************************************************************/

/*
    Callback function for constructing wifi intent.
*/
void wifiIntentConstruction(Request& req, Response& res)
{
    DEBUGOUT("main: wifi intent construction\r\n");
    (void) req;

    /* create intent using generated endpoint and constraint set */
    Intent intent("com.arm.connectivity.wifi");
    intent.knownParameters("/networks")
        .endpoint("/wifi");

    res.write(intent);
}


/*****************************************************************************/
/* Reset device example                                                      */
/*****************************************************************************/


void resetIntentConstruction(Request& req, Response& res)
{
    DEBUGOUT("main: reset intent construction\r\n");
    (void) req;

    /* create intent using generated endpoint and constraint set */
    Intent intent("com.arm.reset");
    intent.endpoint("/reset");
    res.write(intent);
}


/*****************************************************************************/
/* complex example                                                           */
/*****************************************************************************/

/*
    Callback functions for the example intent.
*/
void exampleIntentConstruction(Request& req, Response& res)
{
    DEBUGOUT("main: complex example intent construction\r\n");
    (void) req;

    /* create intent */
    Intent intent("com.arm.examples.complex");
    intent.endpoint("/examples/complex");

    res.write(intent);
}

/*****************************************************************************/
/* custom example                                                            */
/*****************************************************************************/

void customIntentConstruction(Request& req, Response& res)
{
    DEBUGOUT("main: custom intent construction\r\n");
    (void) req;

    /* create intent using generated endpoint and constraint set */
    Intent intent("com.arm.examples.custom");
    intent.endpoint("/custom")
        .constraints()
            .title("Hello!")
            .description("This is the description")
            .addProperty("test",
                Constraint(Constraint::TypeString)
                    .title("Test")
                    .defaultValue("default goes here")
            )
            .addProperty("test2",
                Constraint(Constraint::TypeString)
                    .title("Other test")
                    .defaultValue("default goes here")
            );
    res.write(intent);
}




/*****************************************************************************/
/* Middleware for actually doing stuff                                       */
/*****************************************************************************/

Request* demoCallbackRequest = NULL;
Response* demoCallbackResponse = NULL;
Router::next_t demoCallbackHandle;

void demoCallbackTask()
{
    demoCallbackHandle();
}

void printInvocation(Request& req, Response& res, Router::next_t& next)
{
    (void) req;
    (void) res;

    IntentInvocation invocation(req.getBody());
    invocation.getParameters().print();

    // save parameters so callback can be called asynchronously
    demoCallbackRequest = &req;
    demoCallbackResponse = &res;
    demoCallbackHandle = next;

    // post call to function to demo asynchronous callback
    minar::Scheduler::postCallback(demoCallbackTask);
}

void saveWifi(Request& req, Response& res, Router::next_t& next)
{
    DEBUGOUT("main: saving wifi details\r\n");
    (void) req;
    (void) res;

    IntentInvocation invocation(req.getBody());

    invocation.getParameters().find("ssid").getString(ssid_string);
    invocation.getParameters().find("key").getString(key_string);

    // change state in main application
    state |= FLAG_PROVISIONED;

    // change state inside router
    router.setStateMask(state);

    next();
}

void resetDevice(Request& req, Response& res, Router::next_t& next)
{
    DEBUGOUT("main: reset device\r\n");
    (void) req;
    (void) res;

    ssid_string = "";
    key_string = "";

    // change state in main application
    state &= ~FLAG_PROVISIONED;

    // change state inside router
    router.setStateMask(state);

    next();
}

void sendSuccess(Request& req, Response& res, Router::next_t& next)
{
    DEBUGOUT("main: sending success coda\r\n");
    (void) req;

    IntentInvocation invocation(req.getBody());
    Coda coda(invocation.getID());
    coda.success(true);
    res.write(coda);
    next(200);
}

void networkList(Request& req, Response& res, Router::next_t& next)
{
    DEBUGOUT("main: listing network resources");
    (void) req;
    (void) res;

    KnownParameters parameters(res);

    parameters.begin();

    parameters.parameter("com.arm.connectivity.wifi", 255)
        .map()
            .key("ssid").value("miWifi")
        .end();

    parameters.parameter("com.arm.connectivity.wifi", 125)
        .map()
            .key("ssid").value("yoWifi")
        .end();

    parameters.end();

    next(200);
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/

void app_start(int, char *[])
{
    /*
        Register intents in the router.

        First parameter is the callback function for intent generation.
        Second parameter is a bitmap for grouping intents together.
    */

    // Wifi provisioning intent
    router.registerIntent(wifiIntentConstruction,
                          FLAG_CONNECTED | FLAG_PROVISIONED);

    // reset intent
    router.registerIntent(resetIntentConstruction,
                          FLAG_PROVISIONED);

    // custom intent
    //router.registerIntent(customIntentConstruction,
    //                     FLAG_CONNECTED | FLAG_PROVISIONED);

    // example intent
    router.registerIntent(exampleIntentConstruction,
                         FLAG_CONNECTED | FLAG_PROVISIONED);

    /*
        Set the current state mask.

        Mask is AND'ed with each intent's bitmap and only intents with non-zero
        results are displayed and can be invoked.
    */
    router.setStateMask(0);

    /*
        Define some resource callbacks
    */

    router.get("/networks",          networkList,                               NULL);
    router.post("/wifi",             printInvocation, saveWifi,    sendSuccess, NULL);
    router.post("/reset",            printInvocation, resetDevice, sendSuccess, NULL);
    router.post("/custom",           printInvocation,              sendSuccess, NULL);
    router.post("/examples/complex", printInvocation,              sendSuccess, NULL);


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

    printf("Test: %s %s\r\n", __DATE__, __TIME__);
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

volatile bool sendNotification = false;

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

