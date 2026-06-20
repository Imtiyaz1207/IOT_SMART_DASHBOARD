from flask import Flask, render_template, request, jsonify
from flask import send_from_directory
import os
import paho.mqtt.client as mqtt
import json
import uuid
import time

app = Flask(__name__)

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

ACTION_TOPIC = "shaik/home/actions"
STATUS_TOPIC = "shaik/home/status"
ACK_TOPIC = "shaik/home/system_ack"
OTA_TOPIC = "shaik/home/ota"

ota_progress = 0

device_status = {
    "system": "OFFLINE",
    "light": "OFF",
    "fan": "OFF",
    "pump": "OFF"
}

last_ack = {}
last_seen = time.time()  #new 20/6
# ---------------- MQTT ----------------

def on_connect(client, userdata, flags, rc):
    print("MQTT Connected")
    client.subscribe(STATUS_TOPIC)
    client.subscribe(ACK_TOPIC)
    client.subscribe(OTA_TOPIC) 
def on_message(client, userdata, msg):
    global device_status
    global last_ack
    global ota_progress
    try:
        data = json.loads(msg.payload.decode())

        if msg.topic == STATUS_TOPIC:
            print(
                "STATUS RECEIVED",
                data
            )
            device_status = data
            global last_seen # new 20/6
            last_seen = time.time()

        elif msg.topic == ACK_TOPIC:
            last_ack = data
        
        elif msg.topic == OTA_TOPIC:
            ota_data = json.loads(
                msg.payload.decode()
            )
            ota_progress = ota_data.get(
                "progress",
                0
            )
    
    except Exception as e:
        print(e)

#mqtt_client = mqtt.Client()

mqtt_client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION1
)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

'''mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()'''

def start_mqtt():

    try:

        mqtt_client.connect(
            MQTT_BROKER,
            MQTT_PORT,
            60
        )

        mqtt_client.loop_start()

        print("MQTT STARTED")

    except Exception as e:

        print(
            "MQTT ERROR:",
            e
        )
# ---------------- Routes ----------------

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/status')
def status():
    global device_status
    global last_seen
    
    if time.time() - last_seen > 10:

        device_status["system"] = "OFFLINE"
        device_status["light"] = "OFF"
        device_status["fan"] = "OFF"
        device_status["pump"] = "OFF"

    return jsonify({
        "status": device_status,
        "ack": last_ack,
        "ota_progress": ota_progress
    })

@app.route('/command', methods=['POST'])
def command():

    command = request.json["command"]

    payload = {
        "command_id": str(uuid.uuid4()),
        "command": command
    }

    mqtt_client.publish(
        ACTION_TOPIC,
        json.dumps(payload)
    )

    return jsonify({
        "message": "Command Sent"
    })

UPLOAD_FOLDER = "firmware"

@app.route('/upload_firmware', methods=['POST'])
def upload_firmware():

    file = request.files['file']

    os.makedirs(
        UPLOAD_FOLDER,
        exist_ok=True
    )

    file.save(
        os.path.join(
            UPLOAD_FOLDER,
            "firmware.bin"
        )
    )

    return jsonify({
        "message":"Firmware Uploaded"
    })

@app.route('/firmware.bin')
def firmware_file():

    return send_from_directory(
        "firmware",
        "firmware.bin"
    )



@app.route("/ble")
def ble():
    return render_template("ble_dashboard.html")


if __name__ == '__main__':

    start_mqtt()

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False
    )