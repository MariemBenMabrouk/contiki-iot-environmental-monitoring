
import socket
import json
import requests
import time

# =====================================
# UBIDOTS DEVICE TOKENS
# Replace these placeholders with your
# NEWLY GENERATED Device Tokens.
# =====================================

DEVICE_TOKENS = {
    2: "BBUS-F71pAF9Icn9f0G6SWpaiI7gSTixy4j",
    3: "BBUS-fdUz4SRbPgV4MzOIWDM4E0JBbKlHnk",
    4: "BBUS-eRi1Mkk8KAA7xyqKpyMIWpV8lnjYK0"
}

BASE_URL = "https://industrial.api.ubidots.com/api/v1.6/devices/"

# =====================================
# COOJA SERIAL SOCKET
# =====================================

HOST = "::1"
PORT = 60001

# =====================================
# CONNECT TO COOJA
# =====================================

print("Connecting to Cooja...")

sock = socket.create_connection((HOST, PORT))

print("Connected to Cooja gateway")
print("Forwarding data to Ubidots...")

buffer = ""

# Track last request time separately for each sensor
last_sent = {}

# =====================================
# RECEIVE SENSOR DATA
# =====================================

while True:

    try:
        data = sock.recv(1024)

        if not data:
            print("Cooja connection closed")
            break

        buffer += data.decode("utf-8", "replace")

        while "\n" in buffer:

            line, buffer = buffer.split("\n", 1)

            if "DATA recv" not in line:
                continue

            try:
                start = line.find("{")
                end = line.rfind("}")

                if start == -1 or end == -1:
                    continue

                json_text = line[start:end + 1]
                payload = json.loads(json_text)

                node_id = int(payload["node"])

                if node_id not in DEVICE_TOKENS:
                    print("Unknown sensor:", node_id)
                    continue

                device_label = "sensor-" + str(node_id)

                # Respect the rate limit per device token
                now = time.time()
                elapsed = now - last_sent.get(node_id, 0)

                if elapsed < 1.2:
                    time.sleep(1.2 - elapsed)

                headers = {
                    "X-Auth-Token": DEVICE_TOKENS[node_id],
                    "Content-Type": "application/json"
                }

                values = {
                    "temperature": float(payload["temperature"]),
                    "luminosity": float(payload["luminosity"])
                }

                url = BASE_URL + device_label + "/"

                response = requests.post(
                    url,
                    headers=headers,
                    data=json.dumps(values),
                    timeout=10
                )

                last_sent[node_id] = time.time()

                print("--------------------------------")
                print("Sensor ID:", node_id)
                print("Ubidots device:", device_label)
                print("Temperature:", values["temperature"])
                print("Luminosity:", values["luminosity"])
                print("HTTP status:", response.status_code)
                print("Response:", response.text)

            except (ValueError, KeyError, TypeError) as error:
                print("Invalid sensor packet:", error)

            except requests.exceptions.RequestException as error:
                print("Ubidots connection error:", error)

    except KeyboardInterrupt:
        print("Bridge stopped.")
        break

    except socket.error as error:
        print("Socket error:", error)
        break

sock.close()
print("Connection closed.")

