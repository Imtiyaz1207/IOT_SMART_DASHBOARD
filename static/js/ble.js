const scanBtn =
document.getElementById("scanBtn");

const sendBtn =
document.getElementById("sendBtn");

const deviceName =
document.getElementById("deviceName");

const status =
document.getElementById("status");

const ssidInput =
document.getElementById("ssid");

const passwordInput =
document.getElementById("password");

let bleDevice = null;
let characteristic = null;

const SERVICE_UUID =
"12345678-1234-1234-1234-123456789abc";

const CHARACTERISTIC_UUID =
"87654321-4321-4321-4321-cba987654321";

scanBtn.addEventListener(
"click",
async () => {

    try{

        status.innerText =
        "Scanning...";

        bleDevice =
        await navigator.bluetooth.requestDevice({

            acceptAllDevices: true,
            
            optionalServices:[
                SERVICE_UUID
            ]

        });

        const server =
        await bleDevice.gatt.connect();

        const service =
        await server.getPrimaryService(
            SERVICE_UUID
        );

        characteristic =
        await service.getCharacteristic(
            CHARACTERISTIC_UUID
        );

        deviceName.innerText =
        bleDevice.name;

        status.innerText =
        "Connected";

        console.log(
            "BLE Connected"
        );

    }

    catch(error){

        console.error(error);

        status.innerText =
        "Connection Failed";
    }

});

sendBtn.addEventListener(
"click",
async () => {

    try{

        if(!characteristic){

            alert(
                "Connect ESP32 first"
            );

            return;
        }

        const ssid =
        ssidInput.value.trim();

        const password =
        passwordInput.value.trim();

        if(!ssid || !password){

            alert(
                "Enter SSID and Password"
            );

            return;
        }

        const payload =
        `${ssid},${password}`;

        console.log(
            "Sending:",
            payload
        );

        const encoder =
        new TextEncoder();

        await characteristic.writeValue(
            encoder.encode(payload)
        );

        status.innerText =
        "Credentials Sent";

        alert(
            "WiFi Credentials Sent Successfully"
        );

    }

    catch(error){

        console.error(error);

        status.innerText =
        "Send Failed";
    }

});