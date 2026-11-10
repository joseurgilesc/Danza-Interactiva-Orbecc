#!/usr/bin/env python3
"""
Pose-only MediaPipe detector for the OrbbecAdapter (Danza Interactiva Orbecc).

Servidor TCP local: recibe frames RGB crudos del cliente oF (OrbbecAdapter),
corre PoseLandmarker (33 landmarks) y devuelve los landmarks normalizados [0,1]
como JSON. Solo pose — más ligero y rápido que el detector completo de ofx-mascara.

Protocolo (longitud prefijada, little-endian):
  cliente -> servidor : [uint32 w][uint32 h][RGB w*h*3 bytes]
  servidor -> cliente : [uint32 len][JSON bytes]

Uso:
  python pose_detector.py [--port 5005] [--host 127.0.0.1]
"""

import argparse
import json
import os
import socket
import struct
import time

import numpy as np

import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision

HERE = os.path.dirname(os.path.abspath(__file__))

PoseLandmarker = vision.PoseLandmarker
PoseLandmarkerOptions = vision.PoseLandmarkerOptions
VisionRunningMode = vision.RunningMode
BaseOptions = mp_python.BaseOptions

MODEL = os.path.join(HERE, "models", "pose_landmarker_lite.task")


def build_pose():
    opts = PoseLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=MODEL),
        running_mode=VisionRunningMode.VIDEO,
        num_poses=8,
        min_pose_detection_confidence=0.4,
        min_pose_presence_confidence=0.4,
        min_tracking_confidence=0.4,
    )
    return PoseLandmarker.create_from_options(opts)


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def send_json(conn, obj):
    data = json.dumps(obj).encode("utf-8")
    conn.sendall(struct.pack("<I", len(data)) + data)


def pose_to_list(result):
    # Devuelve una LISTA de poses; cada pose es una lista de 33 landmarks.
    out = []
    if result.pose_landmarks:
        for pose_lm in result.pose_landmarks:
            pts = [{"x": lm.x, "y": lm.y, "z": lm.z, "visibility": lm.visibility} for lm in pose_lm]
            out.append(pts)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=5005)
    ap.add_argument("--host", default="127.0.0.1")
    args = ap.parse_args()

    print("cargando modelo MediaPipe Pose...", flush=True)
    pose = build_pose()
    print("modelo listo", flush=True)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(1)
    print(f"escuchando en {args.host}:{args.port}", flush=True)

    ts = 0
    frame_no = 0

    while True:
        conn, addr = srv.accept()
        print(f"cliente conectado: {addr}", flush=True)
        try:
            while True:
                head = recv_exact(conn, 8)
                if head is None:
                    break
                w, h = struct.unpack("<II", head)
                if w == 0 or h == 0 or w * h > 4 * 1024 * 1024:
                    break
                rgb_bytes = recv_exact(conn, w * h * 3)
                if rgb_bytes is None:
                    break

                rgb = np.frombuffer(rgb_bytes, dtype=np.uint8).reshape(h, w, 3)
                mp_image = mp.Image(
                    image_format=mp.ImageFormat.SRGB,
                    data=np.ascontiguousarray(rgb),
                )

                ts += 33
                t0 = time.perf_counter()
                pose_res = pose.detect_for_video(mp_image, ts)
                t1 = time.perf_counter()
                frame_no += 1
                if frame_no % 60 == 0:
                    print(f"pose {(t1 - t0) * 1000:.1f}ms", flush=True)

                reply = {
                    "pose": pose_to_list(pose_res),
                    "width": w,
                    "height": h,
                }
                send_json(conn, reply)
        except (ConnectionError, OSError) as e:
            print(f"desconectado: {e}", flush=True)
        finally:
            conn.close()


if __name__ == "__main__":
    main()
