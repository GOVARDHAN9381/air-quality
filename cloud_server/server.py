"""
ESP32 Air Quality Monitor - Python Telemetry Server & Web Visualizer
Zero external dependencies required (Uses Python 3 standard library: http.server, json, urllib)
Run command: python server.py
"""

import http.server
import socketserver
import json
import os
import urllib.parse
from datetime import datetime

PORT = int(os.environ.get("PORT", 3000))
PUBLIC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../web_dashboard"))

# In-memory telemetry buffer
telemetry_state = {
    "adc": 1220,
    "ppm": 412,
    "voltage": 0.98,
    "temp": 26.4,
    "hum": 55.0,
    "status": "NORMAL",
    "green": 1,
    "red": 0,
    "buzz": 0,
    "warnThresh": 1200,
    "alertThresh": 2000,
    "updatedAt": datetime.now().isoformat()
}

class TelemetryRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=PUBLIC_DIR, **kwargs)

    def end_headers(self):
        # Enable CORS for all incoming client requests
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/data":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(telemetry_state).encode("utf-8"))
            return

        # Serve static dashboard files from web_dashboard
        return super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/telemetry":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length)

            try:
                data = json.loads(post_data.decode("utf-8"))
                adc = int(data.get("adc", 1000))
                
                # State logic
                status = "NORMAL"
                if adc >= telemetry_state["alertThresh"]:
                    status = "ALERT"
                elif adc >= telemetry_state["warnThresh"]:
                    status = "WARNING"

                telemetry_state["adc"] = adc
                telemetry_state["ppm"] = data.get("ppm", int(400 + ((adc / 600.0) ** 2.5) * 85))
                telemetry_state["voltage"] = round((adc / 4095.0) * 3.3, 2)
                telemetry_state["temp"] = float(data.get("temp", telemetry_state["temp"]))
                telemetry_state["hum"] = float(data.get("hum", telemetry_state["hum"]))
                telemetry_state["status"] = status
                telemetry_state["green"] = 1 if status == "NORMAL" else 0
                telemetry_state["red"] = 0 if status == "NORMAL" else 1
                telemetry_state["buzz"] = 1 if status == "ALERT" else 0
                telemetry_state["updatedAt"] = datetime.now().isoformat()

                print(f"[ESP32 -> Web] ADC: {adc} | Temp: {telemetry_state['temp']}C | Hum: {telemetry_state['hum']}% | Status: {status}")

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": True, "state": telemetry_state}).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode("utf-8"))
            return

        self.send_response(404)
        self.end_headers()

if __name__ == "__main__":
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), TelemetryRequestHandler) as httpd:
        print("=" * 65)
        print(f" AirSense Python Telemetry Hub running on http://localhost:{PORT}")
        print(f" Web Dashboard available at:   http://localhost:{PORT}")
        print(f" ESP32 POST Endpoint:          http://localhost:{PORT}/api/telemetry")
        print("=" * 65)
        httpd.serve_forever()
