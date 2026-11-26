#!/usr/bin/env python3
"""Adds fake AV-CAN 0x62C GPS frames to a candump log (default: sample-can.log).

The car drives from Helsinki-Vantaa airport to Helsinki city centre (Senate
Square) along the main roads: Ilmailutie, Tuusulanväylä, Mäkelänkatu, Sörnäisten
rantatie and Pohjoisranta (19.5 km). A GPS fix is sent at the start of the log
(at the airport) and every time the SOC (0x55B) goes down; the route is split
evenly over the SOC drops, so the car arrives in the city centre when the
discharge ends and stays there while charging.

Existing 0x62C frames are removed first, so running the script again is safe.

Usage: ./add_gps_to_log.py [log-file]
"""
import math
import re
import sys

# Driving route (lat, lon) from the airport terminal to Senate Square. Taken from an
# OpenStreetMap (OSRM) driving route, simplified to stay within 12 m of the road.
ROUTE = [
    (60.31670, 24.96424), (60.31569, 24.96625), (60.31516, 24.96506), (60.31495, 24.96502),
    (60.31316, 24.96821), (60.31243, 24.96656), (60.31005, 24.97167), (60.30888, 24.97357),
    (60.30788, 24.97468), (60.30665, 24.97547), (60.30563, 24.97473), (60.30510, 24.97605),
    (60.30354, 24.97836), (60.30277, 24.98010), (60.30212, 24.98278), (60.30121, 24.98843),
    (60.30082, 24.98894), (60.29334, 24.98290), (60.28821, 24.98037), (60.28468, 24.97943),
    (60.28113, 24.97793), (60.27606, 24.97657), (60.27184, 24.97392), (60.26847, 24.97138),
    (60.26532, 24.96839), (60.26070, 24.96276), (60.25867, 24.95928), (60.25638, 24.95342),
    (60.25535, 24.95131), (60.25377, 24.94931), (60.25178, 24.94813), (60.24990, 24.94787),
    (60.24603, 24.94828), (60.24251, 24.94755), (60.23826, 24.94766), (60.23642, 24.94674),
    (60.23289, 24.94373), (60.23082, 24.94299), (60.22936, 24.94309), (60.22477, 24.94477),
    (60.22176, 24.94521), (60.21899, 24.94492), (60.21599, 24.94308), (60.21481, 24.94280),
    (60.21349, 24.94302), (60.20804, 24.94533), (60.20276, 24.94510), (60.20053, 24.94464),
    (60.19841, 24.94737), (60.19837, 24.94810), (60.19535, 24.95549), (60.19274, 24.96082),
    (60.19095, 24.96260), (60.19058, 24.96146), (60.18971, 24.96111), (60.18916, 24.96527),
    (60.18823, 24.97014), (60.18494, 24.96630), (60.17956, 24.95754), (60.17750, 24.95774),
    (60.17681, 24.95816), (60.17646, 24.95885), (60.17599, 24.96080), (60.17551, 24.96151),
    (60.17398, 24.96160), (60.16985, 24.95956), (60.16969, 24.95849), (60.16921, 24.95834),
    (60.16919, 24.95253),
]
FRAME_GAP_S = 0.001    # spacing between the four frames of one GPS message
AV_IFACE = "vcan1"     # GPS is on the AV-CAN, i.e. the second CAN (host --replay-can2 default)

# Template payload from the unit test (status 0x06C = GPS locked). Only the
# lat/lon fields are overwritten.
TEMPLATE = bytes.fromhex("10200D878145" "46030723F48000" "000807EAADC1B9" "40000008")

# Bit positions (bit 0 = MSB of payload byte 0), from canLeafZE1.h
FIELDS = {
    "lat_deg": (27, 8), "lat_min": (35, 8), "lat_ms": (43, 16),
    "lon_deg": (59, 8), "lon_min": (67, 8), "lon_ms": (75, 16),
}


def set_bits(buf, start, length, value):
    for i in range(length):
        bit = (value >> (length - 1 - i)) & 1
        pos = start + i
        mask = 0x80 >> (pos % 8)
        if bit:
            buf[pos // 8] |= mask
        else:
            buf[pos // 8] &= ~mask


def dms(value):
    deg = int(value)
    rest = (value - deg) * 60.0
    minutes = int(rest)
    ms = int(round((rest - minutes) * 60000.0))
    if ms >= 60000:
        ms -= 60000
        minutes += 1
    return deg, minutes, ms


def encode(lat, lon):
    payload = bytearray(TEMPLATE)
    for prefix, value in (("lat", lat), ("lon", lon)):
        for name, part in zip(("deg", "min", "ms"), dms(value)):
            set_bits(payload, *FIELDS[f"{prefix}_{name}"], part)
    p = payload.hex().upper()
    return [
        "1018" + p[0:12],            # first frame: 6 payload bytes
        "21" + p[12:26],             # consecutive frames: 7 bytes each
        "22" + p[26:40],
        "23" + p[40:48] + "FFFFFF",  # last 4 bytes, padded
    ]


def segment_km(a, b):
    north = (b[0] - a[0]) * 111.32
    east = (b[1] - a[1]) * 111.32 * math.cos(math.radians((a[0] + b[0]) / 2))
    return math.hypot(north, east)


ROUTE_KM = sum(segment_km(a, b) for a, b in zip(ROUTE, ROUTE[1:]))


def position(distance_km):
    """Point on the route after driving distance_km from the airport."""
    remaining = max(0.0, distance_km)
    for a, b in zip(ROUTE, ROUTE[1:]):
        seg = segment_km(a, b)
        if remaining <= seg:
            t = remaining / seg if seg else 0.0
            return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        remaining -= seg
    return ROUTE[-1]


def soc_drops(lines, line_re):
    drops, last = 0, None
    for line in lines:
        m = line_re.match(line)
        if m and m.group(3).upper() == "55B" and len(m.group(4)) >= 4:
            soc = int(m.group(4)[0:4], 16) >> 6
            if last is not None and soc < last:
                drops += 1
            last = soc
    return drops


def gps_lines(ts, distance_km):
    lat, lon = position(distance_km)
    return [f"({ts + (i + 1) * FRAME_GAP_S:.6f}) {AV_IFACE} 62C#{data}\n"
            for i, data in enumerate(encode(lat, lon))]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "sample-can.log"
    with open(path) as f:
        lines = [l for l in f if " 62C#" not in l]

    line_re = re.compile(r"\((\d+\.\d+)\) (\S+) ([0-9A-Fa-f]+)#([0-9A-Fa-f]*)")
    step_km = ROUTE_KM / max(1, soc_drops(lines, line_re))
    out = []
    distance = 0.0
    last_soc = None
    initial_done = False
    # Pending GPS message, written after the last frame with the same timestamp
    pending = None

    for line in lines:
        m = line_re.match(line)
        if pending and (not m or float(m.group(1)) != pending[0]):
            out.extend(gps_lines(*pending))
            pending = None
        out.append(line)
        if not m:
            continue
        ts, can_id, data = float(m.group(1)), m.group(3).upper(), m.group(4)
        if not initial_done:
            pending = (ts, distance)
            initial_done = True
        if can_id == "55B" and len(data) >= 4:
            soc_raw = int(data[0:4], 16) >> 6
            if last_soc is not None and soc_raw < last_soc:
                distance += step_km
                pending = (ts, distance)
            last_soc = soc_raw
    if pending:
        out.extend(gps_lines(*pending))

    with open(path, "w") as f:
        f.writelines(out)
    end_lat, end_lon = position(distance)
    print(f"{path}: drove {distance:.1f} of {ROUTE_KM:.1f} km in {step_km * 1000:.0f} m steps, "
          f"end position {end_lat:.6f}, {end_lon:.6f}")


if __name__ == "__main__":
    main()
