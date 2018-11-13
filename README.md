# Create firmware

## Import

```
$ mbed import https://github.com/coisme/PelionWS-L475VG-IOT01A-sample-project.git
```

## Setup

### Change Wi-Fi parameters

Replace SSID and Password in `mbed_app.json` file to yours.

```JSON:mbed_app.json
            "nsapi.default-wifi-ssid"           : "\"SSID\"",
            "nsapi.default-wifi-password"       : "\"Password\""
```

### Create certificates and manifests

You need an API key of your Pelion Device Management account. Replace *<YOUR_API_KEY>* with your API key.

```
$ mbed dm init -d "example.com" --model-name "Workshop" -a "<YOUR_API_KEY>" -q -f
```

## Build firmware

```
$ mbed compile -t GCC_ARM -m DISCO_L475VG_IOT01A
```

Write the firmware to your board via USB.

# Firmware Update by Over-The-Air (OTA)

## Add sensors

Define `ENABLE_SENSORS` macro. A way to do that is adding a line `ENABLE_SENSORS=1` to the `macros` section in `mbed_app.json` file.

```mbed_app.json
    "macros": [
        "MBEDTLS_USER_CONFIG_FILE=\"mbedTLSConfig_mbedOS.h\"",
        "PAL_USER_DEFINED_CONFIGURATION=\"sotp_fs_config_MbedOS.h\"",
        "MBED_CLIENT_USER_CONFIG_FILE=\"mbed_cloud_client_user_config.h\"",
        "MBED_CLOUD_CLIENT_USER_CONFIG_FILE=\"mbed_cloud_client_user_config.h\"",
        "PAL_DTLS_PEER_MIN_TIMEOUT=5000",
        "MBED_CONF_APP_MAIN_STACK_SIZE=5000",
        "ARM_UC_USE_PAL_BLOCKDEVICE=1",
        "MBED_CLOUD_CLIENT_UPDATE_STORAGE=ARM_UCP_FLASHIAP_BLOCKDEVICE",
        "ENABLE_SENSORS=1"
    ],
```
## Build update firmware

```
$ mbed compile -t GCC_ARM -m DISCO_L475VG_IOT01A
```

## Perform update campaign

```
$ mbed dm update device -D <device ID> -m DISCO_L475VG_IOT01A -a <API_KEY>
```

