import paho.mqtt.client as mqtt
import json
import math
import time

# --- Configuration ---
MQTT_BROKER = "10.164.64.13"  
MQTT_PORT = 1883
REQUEST_TOPIC = "cps/compute/request"
RESULT_TOPIC = "cps/compute/result"

def on_connect(client, userdata, flags, rc, *args):
    if rc == 0:
        print("[+] Connected to Mosquitto Broker successfully!")
        client.subscribe(REQUEST_TOPIC)
        print(f"[*] Listening for tasks on: '{REQUEST_TOPIC}'...")
    else:
        print(f"[!] Connection failed with code {rc}")

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode('utf-8')
        print(f"\n[↓] Task Received: {payload_str}")
        
        data = json.loads(payload_str)
        iterations = data.get("value", 0)
        
        start_time = time.time()
        
        dummy = 0.5
        for _ in range(iterations):
            dummy = math.sin(dummy) * math.cos(dummy) + 1.5
            
        compute_time_ms = (time.time() - start_time) * 1000
        
        response = {
            "status": "success",
            "computed_result": dummy,
            "fog_compute_time_ms": round(compute_time_ms, 4)
        }
        
        response_json = json.dumps(response)
        client.publish(RESULT_TOPIC, response_json)
        
        print(f"[↑] Task Finished ({compute_time_ms:.2f} ms). Result sent back.")
        
    except Exception as e:
        print(f"[!] Error processing task: {e}")

# --- VERSION COMPATIBILITY FIX ---
try:
    # For newer Paho MQTT v2.0+
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
except AttributeError:
    # For older Paho MQTT v1.x
    client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

print("Starting Fog Node Engine...")
client.connect(MQTT_BROKER, MQTT_PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\nFog Node shut down.")