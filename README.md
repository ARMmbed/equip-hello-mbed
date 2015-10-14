
This is simple example that shows you how to use mbed OS with the mbed Provisioning App to configure a device. This example covers configuring a device with WiFi credentials, setting up a more complex example for exploring controls offered by the mbed Provisioning App, and defining custom configuration required by the device. 

First it's important to understand the [concepts](https://github.com/ARMmbed/envoy-docs/blob/master/docs/concepts.md) that are used by the mbed Provisioning App.

**Example 1: WiFi Credentials**

This example configures a `com.arm.connectivity.wifi` intent to allow the App user to select a WiFi network to connect their device to. It uses the 'known parameters' system to provide a list of suggested WiFi networks the user might want to use. 

The steps involved in this example are:

1. Configure a resource that describes the currently available networks at `/networks`.
2. Register the WiFi intent construction callback with the router.
3. Configure the intent:
	1. with an endpoint (i.e. the path the credentials should be POSTed to).
	2. with the URL of the resource that describes the available WiFi networks.
4. Handle the intent invocation at the endpoint configured in step 3.
5. Send a success Coda back to the mbed Provisioning App.

**Example 2: Complex Example**

This example sets up an example intent that exists in the global intent registry. The purpose if this intent is to show the options available in the mbed Provisioning App.

1. Register the intent construction callback with the router.
2. Configure the intent with an endpoint (i.e. the path the credentials should be POSTed to).
3. Handle the intent invocation at the endpoint configured in step 2.
4. Send a success Coda back to the mbed Provisioning App.

**Example 3: Custom Example**

This example shows how an intent can be entirely constructed on the device, so it does not rely at all on a global registry entry. This is useful for developing new intents before they are submitted to the global registry.

1. Register the intent construction callback with the router.
2. Configure the intent with an endpoint (i.e. the path the credentials should be POSTed to).
3. Add constraints to the intent that describe the parameters the device is expecting.
4. Handle the intent invocation at the endpoint configured in step 2.
5. Send a success Coda back to the mbed Provisioning App.


### Pre-Requisites

Please install the following:

* [yotta](https://github.com/ARMmbed/yotta). Please note that yotta has its own set of dependencies, listed in the [installation instructions](http://yottadocs.mbed.com/#installing).


### How to build

First, navigate to the directory containing your source files:

```bash
cd voytalk-hello-mbed
```

yotta must know which platform (target) it is building to. So we declare the target, then build.

```bash
yotta target frdm-k64f-gcc
yotta build
```

The resulting binary file will be located in
`build/frdm-k64f-gcc/source/`.

