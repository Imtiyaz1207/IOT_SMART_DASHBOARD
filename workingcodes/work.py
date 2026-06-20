from flask import Flask, render_template, request, jsonify
from flask import send_from_directory
import os
import paho.mqtt.client as mqtt
import json
import uuid

app = Flask(__name__)

MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

ACTION_TOPIC = "iot/smarttank/actions"
STATUS_TOPIC = "iot/smarttank/status"
ACK_TOPIC = "iot/smarttank/system_ack"

device_status = {
    "system": "OFFLINE",
    "light": "OFF",
    "fan": "OFF",
    "pump": "OFF"
}

last_ack = {}

# ---------------- MQTT ----------------

def on_connect(client, userdata, flags, rc):
    print("MQTT Connected")
    client.subscribe(STATUS_TOPIC)
    client.subscribe(ACK_TOPIC)

def on_message(client, userdata, msg):
    global device_status
    global last_ack

    try:
        data = json.loads(msg.payload.decode())

        if msg.topic == STATUS_TOPIC:
            device_status = data

        elif msg.topic == ACK_TOPIC:
            last_ack = data

    except Exception as e:
        print(e)

mqtt_client = mqtt.Client()

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()

# ---------------- Routes ----------------

@app.route('/')
def home():
    return render_template('index.html')

@app.route('/status')
def status():
    return jsonify({
        "status": device_status,
        "ack": last_ack
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




if __name__ == '__main__':
    app.run(
    host="0.0.0.0",
    port=5000,
    debug=True
)