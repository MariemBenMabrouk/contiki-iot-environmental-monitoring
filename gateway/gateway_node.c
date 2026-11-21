
import socket
import json
import time
import os
import requests

# =====================================
# UBIDOTS CONFIGURATION
# =====================================

TOKEN = os.environ.get("UBIDOTS_TOKEN")

if not TOKEN:
    raise RuntimeError("Please set your UBIDOTS_TOKEN first.")

BASE_URL = "https://industrial.api.ubidots.com/api/v1.6/devices/"

HEADERS = {
    "X-Auth-Token": TOKEN,
    "Content-Type": "application/json"
}

# =====================================
# COOJA CONFIGURATION
# =====================================

HOST = "::1"
PORT = 60001

# Minimum interval between HTTP requests
MIN_INTERVAL = 1.2
last_request = 0


# =====================================
# SEND SENSOR DATA TO UBIDOTS
# =====================================

def send_to_ubidots(payload):
    global last_request

    node_id = int(payload["node"])

    # Each sensor gets its own Ubidots device
    device = "sensor-" + str(node_id)

    url = BASE_URL + device

    values = {
        "temperature": float(payload["temperature"]),
        "luminosity": float(payload["luminosity"])
    }

    # Avoid sending requests too quickly
    elapsed = time.time() - last_request

    if elapsed < MIN_INTERVAL:
        time.sleep(MIN_INTERVAL - elapsed)

    for attempt in range(5):
        try:
            response = requests.post(
                url,
                headers=HEADERS,
                data=json.dumps(values),
                timeout=10
            )

            last_request = time.time()

            if response.status_code in (200, 201):
                print("SUCCESS:", device, values)
                return

            if response.status_code == 429:
                delay = 2 ** (attempt + 1)
                print("Rate limit reached. Waiting", delay, "seconds...")
                time.sleep(delay)
                continue

            print("UBIDOTS ERROR:", response.status_code)
            print(response.text)
            return

        except requests.exceptions.RequestException as error:
            delay = 2 ** (attempt + 1)
            print("CONNECTION ERROR:", error)
            time.sleep(delay)

    print("FAILED:", device)


# =====================================
# CONNECT TO COOJA
# =====================================

print("Connecting to Cooja Serial Socket...")

sock = socket.create_connection((HOST, PORT))

print("Connected successfully.")
print("Waiting for sensor packets...")


# =====================================
# READ GATEWAY OUTPUT
# =====================================

buffer = ""

try:
    while True:

        data = sock.recv(1024)

        if not data:
            print("Cooja connection closed.")
            break

        buffer += data.decode("utf-8", "replace")

        # Process complete lines
        while "\n" in buffer:

            line, buffer = buffer.split("\n", 1)

            if "DATA recv" not in line:
                continue

            try:
                # Extract JSON from the gateway output
                start = line.find("{")
                end = line.rfind("}")

                if start == -1 or end == -1:
                    continue

                json_text = line[start:end + 1]

                payload = json.loads(json_text)

                # Check required fields
                if not all(
                    key in payload
                    for key in ("node", "temperature", "luminosity")
                ):
                    print("Invalid sensor packet:", payload)
                    continue

                print("RECEIVED:", payload)

                send_to_ubidots(payload)

            except (ValueError, KeyError, TypeError) as error:
                print("PACKET ERROR:", error)
                print("Line:", line)

except KeyboardInterrupt:
    print("\nBridge stopped by user.")

finally:
    sock.close()
    print("Connection closed.")

