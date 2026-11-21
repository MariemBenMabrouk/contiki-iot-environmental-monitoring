
from __future__ import print_function
import socket

HOST = "::1"
PORT = 60001

print("Connecting to Cooja gateway...")

try:
    sock = socket.create_connection((HOST, PORT))
    print("Connected successfully!")
    print("Waiting for environmental data...\n")

    while True:
        data = sock.recv(1024)

        if not data:
            print("Connection closed.")
            break

        print(data.decode("utf-8", "replace"), end="")

except KeyboardInterrupt:
    print("\nReceiver stopped.")

except Exception as e:
    print("Connection error:", e)

finally:
    try:
        sock.close()
    except:
        pass

