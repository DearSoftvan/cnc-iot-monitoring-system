#!/usr/bin/env python3
"""
CNC IoT CSV Simulator
---------------------
Exp1.csv, Exp2.csv ve Prep.csv dosyalarını gerçek CNC/edge cihaz verisi gibi
satır satır okuyup MQTT topic'lerine JSON telemetry mesajı gönderir.

Varsayılan topic yapısı:
  factory/cnc/CNC-01/telemetry
  factory/cnc/CNC-02/telemetry
  factory/cnc/PREP-STATION/telemetry
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys
import time
from datetime import datetime
from typing import Any, Dict, Iterable, List, Optional

try:
    import paho.mqtt.client as mqtt
except ImportError:  # Kullanıcıya net hata verelim.
    mqtt = None


def clean_value(value: Any) -> Optional[Any]:
    """CSV'deki boş/na değerlerini None yapar, sayısal değerleri float'a çevirir."""
    if value is None:
        return None
    if isinstance(value, str):
        v = value.strip()
        if v == "" or v.lower() in {"na", "nan", "null", "none"}:
            return None
        value = v
    try:
        number = float(value)
        if math.isnan(number):
            return None
        if number.is_integer():
            return int(number)
        return number
    except (ValueError, TypeError):
        return value


def get(row: Dict[str, Any], key: str) -> Optional[Any]:
    return clean_value(row.get(key))


def quality_status(ra: Optional[float]) -> str:
    if ra is None:
        return "UNKNOWN"
    if ra < 0.8:
        return "GOOD"
    if ra < 1.2:
        return "WARNING"
    return "BAD"


def load_status(force_total: Optional[float]) -> str:
    if force_total is None:
        return "UNKNOWN"
    if force_total < 250:
        return "NORMAL_LOAD"
    if force_total < 350:
        return "HIGH_LOAD"
    return "FORCE_ALARM"


def alarm_code(q_status: str, l_status: str) -> Optional[str]:
    if l_status == "FORCE_ALARM":
        return "ALARM_FORCE_HIGH"
    if q_status == "BAD":
        return "ALARM_SURFACE_QUALITY_BAD"
    if l_status == "HIGH_LOAD":
        return "WARN_FORCE_HIGH"
    if q_status == "WARNING":
        return "WARN_SURFACE_QUALITY"
    return None


def make_payload(
    row: Dict[str, Any],
    *,
    machine_id: str,
    source_file: str,
    device_id: str,
    sequence_no: int,
) -> Dict[str, Any]:
    """CSV satırını projemizin MQTT JSON mesaj formatına dönüştürür."""
    ra = get(row, "Ra")
    total_force = get(row, "F")
    q_status = quality_status(ra)
    l_status = load_status(total_force)
    alarm = alarm_code(q_status, l_status)

    machine_status = "ALARM" if alarm and alarm.startswith("ALARM") else "RUNNING"

    experiment = get(row, "Experiment")
    if experiment is None:
        experiment = get(row, "Exp")

    return {
        "device_id": device_id,
        "machine_id": machine_id,
        "source_file": source_file,
        "sequence_no": sequence_no,
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "run_id": get(row, "Run_ID"),
        "experiment": experiment,
        "replica": get(row, "Replica"),
        "tool_id": get(row, "Tool_ID"),
        "group": get(row, "Group"),
        "subgroup": get(row, "Subgroup"),
        "position": get(row, "Position"),
        "condition": get(row, "Condition"),
        "tool_condition": get(row, "TCond"),
        "process": {
            "machined_length": get(row, "Machined_length"),
            "init_diameter": get(row, "Init_diameter"),
            "final_diameter": get(row, "Final_diameter"),
            "cycle_time": get(row, "CTime"),
            "depth_of_cut_ap": get(row, "ap"),
            "cutting_speed_vc": get(row, "vc"),
            "feed_rate_f": get(row, "f"),
        },
        "force": {
            "fx": get(row, "Fx"),
            "fy": get(row, "Fy"),
            "fz": get(row, "Fz"),
            "total_force": total_force,
        },
        "quality": {
            "ra": ra,
            "rz": get(row, "Rz"),
            "rsk": get(row, "Rsk"),
            "rku": get(row, "Rku"),
            "rsm": get(row, "RSm"),
            "rt": get(row, "Rt"),
        },
        "status": {
            "machine_status": machine_status,
            "load_status": l_status,
            "quality_status": q_status,
            "alarm_code": alarm,
        },
    }


def read_csv_rows(path: str) -> List[Dict[str, Any]]:
    with open(path, newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def create_mqtt_client(broker: str, port: int, client_id: str):
    if mqtt is None:
        print("paho-mqtt kurulu değil. Önce: pip install -r requirements.txt", file=sys.stderr)
        sys.exit(1)

    # Paho 2.x ile uyumlu; eski sürümlerde de çalışacak şekilde basit tutuldu.
    client = mqtt.Client(client_id=client_id)
    client.connect(broker, port, keepalive=60)
    client.loop_start()
    return client


def iter_round_robin(sources: List[Dict[str, Any]], loop: bool) -> Iterable[Dict[str, Any]]:
    index = 0
    while True:
        published_any = False
        for source in sources:
            rows = source["rows"]
            if not rows:
                continue
            if index < len(rows):
                row = rows[index]
            elif loop:
                row = rows[index % len(rows)]
            else:
                continue
            published_any = True
            yield {
                "machine_id": source["machine_id"],
                "device_id": source["device_id"],
                "source_file": source["source_file"],
                "row": row,
            }
        if not published_any:
            break
        index += 1


def main() -> None:
    parser = argparse.ArgumentParser(description="CNC CSV verilerini MQTT ile canlı veri gibi yayınlar.")
    parser.add_argument("--data-dir", default="data", help="CSV dosyalarının bulunduğu klasör")
    parser.add_argument("--broker", default="localhost", help="MQTT broker adresi")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker portu")
    parser.add_argument("--interval", type=float, default=1.0, help="Mesaj gönderme aralığı, saniye")
    parser.add_argument("--loop", action="store_true", help="CSV sonuna gelince başa dön")
    parser.add_argument("--dry-run", action="store_true", help="MQTT'ye göndermeden mesaj örneği yazdır")
    args = parser.parse_args()

    mapping = [
        ("Exp1.csv", "CNC-01", "EDGE-ESP12-01"),
        ("Exp2.csv", "CNC-02", "EDGE-ESP12-02"),
        ("Prep.csv", "PREP-STATION", "EDGE-ESP12-03"),
    ]

    sources: List[Dict[str, Any]] = []
    for filename, machine_id, device_id in mapping:
        path = os.path.join(args.data_dir, filename)
        if not os.path.exists(path):
            print(f"UYARI: {path} bulunamadı, atlanıyor.")
            continue
        rows = read_csv_rows(path)
        sources.append({
            "source_file": filename,
            "machine_id": machine_id,
            "device_id": device_id,
            "rows": rows,
        })
        print(f"Yüklendi: {filename} -> {machine_id}, satır sayısı: {len(rows)}")

    if not sources:
        print("Hiç CSV dosyası bulunamadı.", file=sys.stderr)
        sys.exit(1)

    client = None
    if not args.dry_run:
        client = create_mqtt_client(args.broker, args.port, client_id="cnc-csv-simulator")
        print(f"MQTT broker bağlantısı hazır: {args.broker}:{args.port}")

    sequence_no = 1
    try:
        for item in iter_round_robin(sources, loop=args.loop):
            payload = make_payload(
                item["row"],
                machine_id=item["machine_id"],
                source_file=item["source_file"],
                device_id=item["device_id"],
                sequence_no=sequence_no,
            )
            topic = f"factory/cnc/{item['machine_id']}/telemetry"
            message = json.dumps(payload, ensure_ascii=False)

            if args.dry_run:
                print(f"\nTOPIC: {topic}\n{json.dumps(payload, indent=2, ensure_ascii=False)}")
                if sequence_no >= 3:
                    print("\nDry-run için ilk 3 mesaj gösterildi.")
                    break
            else:
                result = client.publish(topic, message, qos=0, retain=False)
                result.wait_for_publish(timeout=3)
                print(
                    f"[{payload['timestamp']}] {topic} | "
                    f"Ra={payload['quality']['ra']} | "
                    f"F={payload['force']['total_force']} | "
                    f"Status={payload['status']['machine_status']}"
                )
                time.sleep(args.interval)
            sequence_no += 1
    except KeyboardInterrupt:
        print("\nSimülatör durduruldu.")
    finally:
        if client is not None:
            client.loop_stop()
            client.disconnect()


if __name__ == "__main__":
    main()
