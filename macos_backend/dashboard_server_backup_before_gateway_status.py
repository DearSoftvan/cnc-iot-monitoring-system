#!/usr/bin/env python3
"""
CNC IoT Local Dashboard Server
------------------------------
MacOS merkez bilgisayar üzerinde çalışır.
- MQTT broker'dan CNC telemetry mesajlarını dinler.
- Verileri SQLite'a kaydeder.
- HTTP ile dashboard sayfasını sunar.
- WebSocket ile dashboard'a canlı veri aktarır.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set

import paho.mqtt.client as mqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
import uvicorn

BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"
DB_PATH = BASE_DIR / "cnc_messages.db"

app = FastAPI(title="CNC IoT Dashboard", version="0.1.0")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

websockets: Set[WebSocket] = set()
event_loop: Optional[asyncio.AbstractEventLoop] = None
mqtt_client: Optional[mqtt.Client] = None
config: Dict[str, str | int] = {
    "broker": "localhost",
    "port": 1883,
    "topic": "factory/cnc/+/telemetry",
}


def init_db() -> None:
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                received_at TEXT NOT NULL,
                topic TEXT NOT NULL,
                machine_id TEXT,
                payload_json TEXT NOT NULL
            )
            """
        )
        conn.commit()


def insert_message(topic: str, payload: dict) -> None:
    machine_id = payload.get("machine_id")
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            "INSERT INTO messages (received_at, topic, machine_id, payload_json) VALUES (?, ?, ?, ?)",
            (datetime.now().isoformat(timespec="seconds"), topic, machine_id, json.dumps(payload, ensure_ascii=False)),
        )
        conn.commit()


def read_latest(limit: int = 30) -> List[dict]:
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        rows = conn.execute(
            "SELECT id, received_at, topic, machine_id, payload_json FROM messages ORDER BY id DESC LIMIT ?",
            (limit,),
        ).fetchall()
    result: List[dict] = []
    for row in rows:
        payload = json.loads(row["payload_json"])
        payload["_db_id"] = row["id"]
        payload["_received_at"] = row["received_at"]
        payload["_topic"] = row["topic"]
        result.append(payload)
    return result


async def broadcast(payload: dict) -> None:
    disconnected: List[WebSocket] = []
    for ws in list(websockets):
        try:
            await ws.send_json(payload)
        except Exception:
            disconnected.append(ws)
    for ws in disconnected:
        websockets.discard(ws)


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        topic = str(config["topic"])
        print(f"MQTT bağlantısı başarılı. Dinlenen topic: {topic}")
        client.subscribe(topic)
    else:
        print(f"MQTT bağlantı hatası. rc={rc}")


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode("utf-8"))
    except Exception as exc:
        print(f"Geçersiz MQTT mesajı: {exc}")
        return

    insert_message(msg.topic, payload)
    if event_loop is not None:
        asyncio.run_coroutine_threadsafe(broadcast(payload), event_loop)


@app.on_event("startup")
async def startup_event() -> None:
    global event_loop, mqtt_client
    event_loop = asyncio.get_running_loop()
    init_db()

    mqtt_client = mqtt.Client(client_id="cnc-dashboard-subscriber")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(str(config["broker"]), int(config["port"]), keepalive=60)
    mqtt_client.loop_start()


@app.on_event("shutdown")
async def shutdown_event() -> None:
    if mqtt_client is not None:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()


@app.get("/")
async def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.get("/api/health")
async def health() -> JSONResponse:
    return JSONResponse({
        "status": "ok",
        "mqtt_broker": config["broker"],
        "mqtt_port": config["port"],
        "mqtt_topic": config["topic"],
        "database": str(DB_PATH),
    })


@app.get("/api/latest")
async def latest(limit: int = 30) -> JSONResponse:
    limit = max(1, min(limit, 200))
    return JSONResponse(read_latest(limit=limit))


@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    websockets.add(ws)
    try:
        await ws.send_json({"type": "connected", "message": "Dashboard WebSocket connected"})
        while True:
            # İstemciden gelen mesajlara ihtiyacımız yok; bağlantıyı canlı tutuyoruz.
            await ws.receive_text()
    except WebSocketDisconnect:
        websockets.discard(ws)


def main() -> None:
    parser = argparse.ArgumentParser(description="CNC IoT Dashboard Server")
    parser.add_argument("--broker", default="localhost", help="MQTT broker adresi")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT broker portu")
    parser.add_argument("--topic", default="factory/cnc/+/telemetry", help="Dinlenecek MQTT topic")
    parser.add_argument("--host", default="0.0.0.0", help="HTTP dashboard host")
    parser.add_argument("--port", type=int, default=8000, help="HTTP dashboard port")
    args = parser.parse_args()

    config["broker"] = args.broker
    config["port"] = args.mqtt_port
    config["topic"] = args.topic

    uvicorn.run(app, host=args.host, port=args.port)


if __name__ == "__main__":
    main()
