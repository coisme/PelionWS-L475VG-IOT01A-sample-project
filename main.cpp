// ----------------------------------------------------------------------------
// Copyright 2016-2018 ARM Ltd.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------
#ifndef MBED_TEST_MODE
#include "mbed.h"
#include "simple-mbed-cloud-client.h"
#include "FATFileSystem.h"

#ifdef ENABLE_SENSORS
#include "VL53L0X.h"
#endif /* ENABLE_SENSORS */

// An event queue is a very useful structure to debounce information between contexts (e.g. ISR and normal threads)
// This is great because things such as network operations are illegal in ISR, so updating a resource in a button's fall() function is not allowed
EventQueue eventQueue;

// Default network interface object
NetworkInterface *net;

// Default block device
BlockDevice* bd = BlockDevice::get_default_instance();
FATFileSystem fs("sd", bd);

// Declaring pointers for access to Pelion Client resources outside of main()
MbedCloudClientResource *button_res;
MbedCloudClientResource *pattern_res;
#ifdef ENABLE_SENSORS
MbedCloudClientResource *distance_res;
#endif /* ENABLE_SENSORS */

// This function gets triggered by the timer. It's easy to replace it by an InterruptIn and fall() mode on a real button
void button_press() {
    int v = button_res->get_value_int() + 1;

    button_res->set_value(v);

    printf("Button clicked %d times\n", v);
}

#ifdef ENABLE_SENSORS
static DevI2C devI2c(PB_11,PB_10);
static DigitalOut shutdown_pin(PC_6);
static VL53L0X range(&devI2c, &shutdown_pin, PC_7);

void update_sensors() {
    uint32_t distance;
    int status = range.get_distance(&distance);
    if (status == VL53L0X_ERROR_NONE) {
        distance_res->set_value((int)distance);
        printf("VL53L0X [mm]:            %6ld\r", distance);
    } else {
        printf("VL53L0X [mm]:                --\r");
    }    
}
#endif /* ENABLE_SENSORS */

/**
 * PUT handler
 * @param resource The resource that triggered the callback
 * @param newValue Updated value for the resource
 */
void pattern_updated(MbedCloudClientResource *resource, m2m::String newValue) {
    printf("PUT received, new value: %s\n", newValue.c_str());
}

/**
 * POST handler
 * @param resource The resource that triggered the callback
 * @param buffer If a body was passed to the POST function, this contains the data.
 *               Note that the buffer is deallocated after leaving this function, so copy it if you need it longer.
 * @param size Size of the body
 */
void blink_callback(MbedCloudClientResource *resource, const uint8_t *buffer, uint16_t size) {
    printf("POST received. Going to blink LED pattern: %s\n", pattern_res->get_value().c_str());

    static DigitalOut augmentedLed(LED1); // LED that is used for blinking the pattern

    // Parse the pattern string, and toggle the LED in that pattern
    string s = std::string(pattern_res->get_value().c_str());
    size_t i = 0;
    size_t pos = s.find(':');
    while (pos != string::npos) {
        wait_ms(atoi(s.substr(i, pos - i).c_str()));
        augmentedLed = !augmentedLed;

        i = ++pos;
        pos = s.find(':', pos);

        if (pos == string::npos) {
            wait_ms(atoi(s.substr(i, s.length()).c_str()));
            augmentedLed = !augmentedLed;
        }
    }
}

/**
 * Notification callback handler
 * @param resource The resource that triggered the callback
 * @param status The delivery status of the notification
 */
void button_callback(MbedCloudClientResource *resource, const NoticationDeliveryStatus status) {
    printf("Button notification, status %s (%d)\n", MbedCloudClientResource::delivery_status_to_string(status), status);
}

/**
 * Registration callback handler
 * @param endpoint Information about the registered endpoint such as the name (so you can find it back in portal)
 */
void registered(const ConnectorClientEndpointInfo *endpoint) {
    printf("Connected to Pelion Device Management. Endpoint Name: %s\n", endpoint->internal_endpoint_name.c_str());
}

int main(void) {
    printf("Starting Simple Pelion Device Management Client example\n");

    // If the USER button is pushed at launch, format the SD card.
    const int PUSHED = 0;
    DigitalIn *user_button = new DigitalIn(USER_BUTTON);
    if(user_button->read() == PUSHED) {
        printf("USER button is pushed. Formatting the storage...\n");
        if(fs.reformat(bd) == 0){
            printf("The storage reformatted successfully.\n");
        } else {
            printf("Failed to reformat the storage.\n");
        }
    }

#ifdef ENABLE_SENSORS
    range.init_sensor(VL53L0X_DEFAULT_ADDRESS);
#endif /* ENABLE_SENSORS */

    printf("Connecting to the network using Wifi...\n");

    // Connect to the internet (DHCP is expected to be on)
    net = NetworkInterface::get_default_instance();

    nsapi_error_t status = net->connect();

    if (status != NSAPI_ERROR_OK) {
        printf("Connecting to the network failed %d!\n", status);
        return -1;
    }

    printf("Connected to the network successfully. IP address: %s\n", net->get_ip_address());

    // SimpleMbedCloudClient handles registering over LwM2M to Pelion DM
    SimpleMbedCloudClient client(net, bd, &fs);
    int client_status = client.init();
    if (client_status != 0) {
        printf("Pelion Client initialization failed (%d)\n", client_status);
        return -1;
    }

    // Creating resources, which can be written or read from the cloud
    button_res = client.create_resource("3200/0/5501", "button_count");
    button_res->set_value(0);
    button_res->methods(M2MMethod::GET);
    button_res->observable(true);
    button_res->attach_notification_callback(button_callback);

#ifdef ENABLE_SENSORS
    distance_res = client.create_resource("3330/0/5700", "distance");
    distance_res->set_value(0);
    distance_res->methods(M2MMethod::GET);
    distance_res->observable(true);
#endif /* ENABLE_SENSORS */

    pattern_res = client.create_resource("3201/0/5853", "blink_pattern");
    pattern_res->set_value("500:500:500:500:500:500:500:500");
    pattern_res->methods(M2MMethod::GET | M2MMethod::PUT);
    pattern_res->attach_put_callback(pattern_updated);

    MbedCloudClientResource *blink_res = client.create_resource("3201/0/5850", "blink_action");
    blink_res->methods(M2MMethod::POST);
    blink_res->attach_post_callback(blink_callback);

    printf("Initialized Pelion Client. Registering...\n");

    // Callback that fires when registering is complete
    client.on_registered(&registered);

    // Register with Pelion DM
    client.register_and_connect();

    // Placeholder for callback to update local resource when GET comes.
    // The timer fires on an interrupt context, but debounces it to the eventqueue, so it's safe to do network operations
    InterruptIn userButton(USER_BUTTON);
    userButton.fall(eventQueue.event(button_press));

#ifdef ENABLE_SENSORS
    Ticker timer;
    timer.attach(eventQueue.event(update_sensors), 1.0);
#endif /* ENABLE_SENSORS */

    // You can easily run the eventQueue in a separate thread if required
    eventQueue.dispatch_forever();
}
#endif
