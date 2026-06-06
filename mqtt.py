import streamlit as st
import paho.mqtt.client as mqtt
import json
import threading
import time

# --- CONFIG ---
MQTT_BROKER = "0cc63655558d460093c7a1a3185b0af2.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "farmer_1"
MQTT_PASS = "Abcd1234"
MQTT_TOPIC = "farm/sensors"

# --- GLOBAL DATA ---
if 'GLOBAL_SENSOR_DATA' not in globals():
    GLOBAL_SENSOR_DATA = {
        "n": 0, "p": 0, "k": 0,
        "temp": 0.0, "hum": 0.0,
        "air": 0, "soil": 0
    }

lock = threading.Lock()

# --- MQTT CALLBACKS ---
def on_connect(client, userdata, flags, rc):
    print("Connected:", rc)
    if rc == 0:
        client.subscribe(MQTT_TOPIC)
        print("Subscribed to", MQTT_TOPIC)

def on_message(client, userdata, msg):
    global GLOBAL_SENSOR_DATA
    try:
        payload = json.loads(msg.payload.decode())
        print("Received:", payload)

        with lock:
            GLOBAL_SENSOR_DATA.update(payload)

    except Exception as e:
        print("Error:", e)

# --- START MQTT ---
@st.cache_resource
def start_mqtt():
    client = mqtt.Client()
    client.username_pw_set(MQTT_USER, MQTT_PASS)

    client.tls_set()
    client.tls_insecure_set(True)

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(MQTT_BROKER, MQTT_PORT)
    client.loop_start()

    return client

mqtt_client = start_mqtt()

# --- GIVE MQTT TIME TO RECEIVE ---
time.sleep(1)

# --- STREAMLIT PAGE ---
st.set_page_config(page_title="SmartFarm", layout="wide")

st.markdown("""
<style>
.status-healthy {
    background-color: #4CAF50;
    color: white;
    padding: 10px;
    border-radius: 5px;
    text-align: center;
    font-weight: bold;
}
</style>
""", unsafe_allow_html=True)

# --- FETCH DATA SAFELY ---
time.sleep(0.5)
with lock:
    s = GLOBAL_SENSOR_DATA.copy()

# --- DEBUG ---
st.write("DEBUG:", s)

# --- UI ---
col_title, col_nav = st.columns([2, 3])

with col_title:
    st.subheader("🍃 SmartFarm Prototype")

with col_nav:
    st.write("<p style='text-align: right;'>Home | Alerts | Reports | Settings</p>", unsafe_allow_html=True)

st.divider()

left, center, right = st.columns([1, 2, 1.5])

# LEFT
with left:
    st.markdown("### Farm Overview")
    st.write("Air Quality:", s['air'])

    if s['soil'] < 30:
        st.warning("Low Soil Moisture!")

# CENTER
with center:
    c1, c2, c3 = st.columns(3)
    c1.metric("Temp", f"{s['temp']} °C")
    c2.metric("Humidity", f"{s['hum']} %")
    c3.metric("Soil", f"{s['soil']} %")

# RIGHT
with right:
    st.markdown("### NPK")
    st.write("N:", s['n'])
    st.write("P:", s['p'])
    st.write("K:", s['k'])

# CONTROL
st.markdown("---")
if st.button("💧 Start Pump"):
    mqtt_client.publish("farm/control", "PUMP_ON")
    st.success("Pump triggered")

# --- REFRESH LOOP ---
time.sleep(2)
st.rerun()
