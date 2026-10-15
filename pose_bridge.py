#!/usr/bin/env python3
"""
MoveNet Pose → OSC Bridge for ProyectoDanza2026

Uses ONNX Runtime + imageio (lightweight, no OpenCV needed).
MoveNet Lightning ONNX model (~13MB, auto-downloaded).

Install: pip install onnxruntime imageio imageio-ffmpeg Pillow python-osc requests
Run:     python3 pose_bridge.py --camera 0

OSC Protocol:
  /pose/begin  i:bodyId i:numJoints
  /pose/joint  s:jointName f:x f:y f:z f:confidence
  /pose/end
"""

import numpy as np
import os
import sys
import time
import urllib.request
import argparse

try:
    import onnxruntime as ort
except ImportError:
    print("ERROR: onnxruntime not installed.")
    sys.exit(1)

try:
    import imageio
except ImportError:
    print("ERROR: imageio not installed. Run: pip install imageio imageio-ffmpeg")
    sys.exit(1)

try:
    from pythonosc import udp_client
except ImportError:
    print("ERROR: python-osc not installed.")
    sys.exit(1)

# ============================================
# CONFIG
# ============================================
OSC_HOST = "127.0.0.1"
OSC_PORT = 12345
MIN_CONFIDENCE = 0.3
INPUT_SIZE = 192
MODEL_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pose_model.onnx")

MODEL_URLS = [
    "https://github.com/ibaiGorordo/ONNX-MoveNet-Single-Pose-Lightning/raw/main/movenet_lightning.onnx",
    "https://huggingface.co/qualcomm/MoveNet-SinglePose-Lightning/resolve/main/movenet_lightning.onnx",
]

KEYPOINT_MAP = {
    0:  "head",           5:  "shoulder_left",
    6:  "shoulder_right", 7:  "elbow_left",
    8:  "elbow_right",    9:  "wrist_left",
    10: "wrist_right",    11: "hip_left",
    12: "hip_right",      13: "knee_left",
    14: "knee_right",     15: "ankle_left",
    16: "ankle_right",
}

def download_model():
    if os.path.exists(MODEL_PATH) and os.path.getsize(MODEL_PATH) > 100000:
        return True
    print("Downloading MoveNet ONNX (~13MB)...")
    for url in MODEL_URLS:
        try:
            urllib.request.urlretrieve(url, MODEL_PATH)
            mb = os.path.getsize(MODEL_PATH) / (1024 * 1024)
            if mb < 1.0: continue
            print(f"  ✅ {mb:.1f} MB")
            return True
        except Exception as e:
            print(f"  ❌ {e}")
    print(f"\nManual: save ONNX model as {MODEL_PATH}")
    return False

def compute_joints(keypoints, confidences):
    joints = {}
    for mp_idx, name in KEYPOINT_MAP.items():
        conf = confidences[mp_idx]
        if conf >= MIN_CONFIDENCE:
            x, y = keypoints[mp_idx]
            joints[name] = (float(x), float(y), 0.0, float(conf))
    
    # NECK
    if all(confidences[i] >= MIN_CONFIDENCE for i in [0, 5, 6]):
        sx = (keypoints[5][0] + keypoints[6][0]) / 2.0
        sy = (keypoints[5][1] + keypoints[6][1]) / 2.0
        nx = (keypoints[0][0] + sx) / 2.0
        ny = (keypoints[0][1] + sy) / 2.0
        nc = min(confidences[i] for i in [0, 5, 6])
        joints["neck"] = (float(nx), float(ny), 0.0, float(nc))
    
    # SPINE_MID
    if confidences[5] >= MIN_CONFIDENCE and confidences[6] >= MIN_CONFIDENCE:
        mx = (keypoints[5][0] + keypoints[6][0]) / 2.0
        my = (keypoints[5][1] + keypoints[6][1]) / 2.0
        mc = min(confidences[5], confidences[6])
        joints["spine_mid"] = (float(mx), float(my), 0.0, float(mc))
    
    # SPINE_BASE
    if confidences[11] >= MIN_CONFIDENCE and confidences[12] >= MIN_CONFIDENCE:
        hx = (keypoints[11][0] + keypoints[12][0]) / 2.0
        hy = (keypoints[11][1] + keypoints[12][1]) / 2.0
        hc = min(confidences[11], confidences[12])
        joints["spine_base"] = (float(hx), float(hy), 0.0, float(hc))
    
    if "wrist_left" in joints:
        joints["hand_left"] = joints["wrist_left"]
    if "wrist_right" in joints:
        joints["hand_right"] = joints["wrist_right"]
    
    return joints

def send_frame(client, joints, body_id=1):
    if not joints:
        return
    client.send_message("/pose/begin", [body_id, len(joints)])
    for name, (x, y, z, conf) in joints.items():
        client.send_message("/pose/joint", [name, float(x), float(y), float(z), float(conf)])
    client.send_message("/pose/end", [])

def main():
    parser = argparse.ArgumentParser(description='MoveNet Pose → OSC Bridge')
    parser.add_argument('--camera', type=int, default=0, help='Camera device ID')
    args = parser.parse_args()
    
    print("=" * 50)
    print("MoveNet Pose → OSC Bridge (ONNX + imageio)")
    print(f"Camera: {args.camera} | OSC: {OSC_HOST}:{OSC_PORT}")
    print("=" * 50)
    
    if not download_model():
        sys.exit(1)
    
    print("Loading ONNX model...")
    session = ort.InferenceSession(MODEL_PATH, providers=['CPUExecutionProvider'])
    input_name = session.get_inputs()[0].name
    print(f"✅ Model loaded")
    
    from PIL import Image
    
    client = udp_client.SimpleUDPClient(OSC_HOST, OSC_PORT)
    
    # Open camera via imageio (uses ffmpeg backend)
    camera_uri = f"<video{args.camera}>"
    print(f"Opening camera: {camera_uri}")
    
    try:
        reader = imageio.get_reader(camera_uri, 'ffmpeg')
        fps = reader.get_meta_data().get('fps', 30)
        print(f"✅ Camera open. FPS: {fps}")
    except Exception as e:
        print(f"❌ Camera error: {e}")
        print("  Try: pip install imageio-ffmpeg")
        print("  Or check camera permissions in System Preferences > Privacy > Camera")
        sys.exit(1)
    
    print(f"\n🎥 Streaming... OSC → {OSC_HOST}:{OSC_PORT}\n")
    
    frame_count = 0
    fps_time = time.time()
    
    try:
        for frame in reader:
            if frame is None:
                continue
            
            h, w = frame.shape[:2]
            
            # imageio returns RGB, no conversion needed
            if frame.shape[2] == 3:
                rgb = frame
            else:
                rgb = frame[:, :, :3]
            
            # Flip horizontal for mirror view
            rgb = np.fliplr(rgb)
            
            # Resize to model input
            img = np.array(Image.fromarray(rgb).resize((INPUT_SIZE, INPUT_SIZE)))
            input_data = np.expand_dims(img.astype(np.float32), axis=0)
            
            # Run inference
            outputs = session.run(None, {input_name: input_data})
            output = outputs[0]  # (1, 17, 3)
            
            keypoints = np.zeros((17, 2), dtype=np.float32)
            confidences = np.zeros(17, dtype=np.float32)
            
            for i in range(17):
                y_val = float(output[0][i][0])
                x_val = float(output[0][i][1])
                conf = float(output[0][i][2])
                keypoints[i] = [x_val, y_val]
                confidences[i] = conf
            
            joints = compute_joints(keypoints, confidences)
            
            if joints:
                send_frame(client, joints)
            
            frame_count += 1
            if frame_count % 30 == 0:
                elapsed = time.time() - fps_time
                fps_str = f"{(30.0 / elapsed):.1f}" if elapsed > 0 else "?"
                print(f"\r  FPS: {fps_str} | Joints: {len(joints)}", end="")
                fps_time = time.time()
    
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"\n⚠️ Error: {e}")
    finally:
        reader.close()
        print("\nDone.")

if __name__ == "__main__":
    main()
