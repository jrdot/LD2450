"""웹UI 레이아웃 확인용 서버임. 실제 센서 데이터와 분리된 고정 예제 제공함."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json
import re

html = re.search(r'R"HTML\((.*)\)HTML"', Path("include/web_ui.h").read_text(encoding="utf-8"), re.S).group(1)
targets = [
    dict(present=True, x=-782, y=1713, fx=-782, fy=1713, speed=-16, resolution=320),
    dict(present=True, x=1200, y=3400, fx=1200, fy=3400, speed=12, resolution=320),
    dict(present=False, x=0, y=0, fx=0, fy=0, speed=0, resolution=0),
]


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            content, kind = html.encode(), "text/html; charset=utf-8"
        else:
            data = dict(name="ESP32 Sensor · 예제", version="1.0.0", network="AP", ip="192.168.4.1",
                        rssi=None, attempts=5, uptime=1000, heap=170000, bytes=30000, frames=1000,
                        fresh=True, age=45, range=6000, update="자동업데이트 비활성화됨", targets=targets)
            if self.path == "/api/settings":
                data = dict(ssid="", device=dict(name="ESP32 Sensor", alpha=.35, range=6000, stale=2000),
                            auto=dict(enabled=False, versionUrl="", binaryUrl="", interval=60, ca=""))
            content, kind = json.dumps(data, ensure_ascii=False).encode(), "application/json; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print("웹UI 예제 미리보기: http://127.0.0.1:8765", flush=True)
    HTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
