#!/usr/bin/env python3
"""
MediaPipe Pose → OSC Bridge for ProyectoDanza2026

Captures webcam (OpenCV) or Kinect v1 (libfreenect), runs MediaPipe Pose
(33 landmarks), maps to 18 joints, applies temporal smoothing and
last-valid-pose retention, sends via OSC to the C++ adapter.

Install: pip install mediapipe opencv-python python-osc freenect
Run:     python3 mediapipe_pose_bridge.py --source webcam
         python3 mediapipe_pose_bridge.py --source kinect

OSC Protocol:
  /pose/begin       i:bodyId i:numJoints
  /pose/joint       s:jointName f:x f:y f:z f:confidence
  /pose/end
  /mediapipe/presence  f:presence     (1.0 = fully tracked, 0.0 = lost)
"""

import cv2
import mediapipe as mp
from pythonosc import udp_client
import time
import sys
import argparse
import traceback

# ============================================
# CONFIG
# ============================================
OSC_HOST = "127.0.0.1"
OSC_PORT = 12345
DEFAULT_CAMERA_ID = 0
FLIP_HORIZONTAL = True  # Mirror for natural interaction
DEFAULT_MIN_DETECTION_CONFIDENCE = 0.5
DEFAULT_MIN_TRACKING_CONFIDENCE = 0.5
FPS_TARGET = 30

# Temporal smoothing & loss retention
MAX_MISSING_FRAMES = 10       # ~333 ms at 30 fps
DEFAULT_SMOOTHING_ALPHA = 0.25

# ============================================
# MediaPipe → Our 18-joint mapping
# ============================================
# MediaPipe landmarks (0-32) → our JointType names
# MediaPipe coords are 0-1: x left→right, y top→bottom
# We don't change coords here; the C++ adapter mirrors X
LANDMARK_MAP = {
    0:  "head",           # NOSE → HEAD
    11: "shoulder_left",  # LEFT_SHOULDER
    12: "shoulder_right", # RIGHT_SHOULDER
    13: "elbow_left",     # LEFT_ELBOW
    14: "elbow_right",    # RIGHT_ELBOW
    15: "wrist_left",     # LEFT_WRIST
    16: "wrist_right",    # RIGHT_WRIST
    19: "hand_left",      # LEFT_INDEX (closest to hand center)
    20: "hand_right",     # RIGHT_INDEX
    23: "hip_left",       # LEFT_HIP
    24: "hip_right",      # RIGHT_HIP
    25: "knee_left",      # LEFT_KNEE
    26: "knee_right",     # RIGHT_KNEE
    27: "ankle_left",     # LEFT_ANKLE
    28: "ankle_right",    # RIGHT_ANKLE
}


# ============================================
# JointSmoother — exponential moving average
# ============================================
class JointSmoother:
    """Per-joint exponential smoothing to reduce jitter.

    Formula: filtered = previous * (1 - alpha) + current * alpha

    alpha = 0.20  → more stability, more latency
    alpha = 0.25  → balanced (recommended default)
    alpha = 0.35  → more responsive, less smoothing
    """

    def __init__(self, alpha=0.25):
        self.alpha = alpha
        self.previous = {}  # joint_name → (x, y, z)

    def smooth_joint(self, joint_name, x, y, z, visibility):
        """Apply exponential smoothing to a single joint.

        Returns (sx, sy, sz, visibility) — visibility is passed through unchanged.
        """
        if joint_name not in self.previous:
            self.previous[joint_name] = (x, y, z)

        px, py, pz = self.previous[joint_name]

        sx = px * (1.0 - self.alpha) + x * self.alpha
        sy = py * (1.0 - self.alpha) + y * self.alpha
        sz = pz * (1.0 - self.alpha) + z * self.alpha

        self.previous[joint_name] = (sx, sy, sz)

        return sx, sy, sz, visibility

    def reset(self):
        """Clear smoothing history (e.g. after prolonged tracking loss)."""
        self.previous.clear()


# ============================================
# Computed joints (neck, spine_mid, spine_base)
# ============================================
def compute_computed_joints(landmarks):
    """Compute joints from midpoints of existing landmarks.

    Returns dict: joint_name → (x, y, z, confidence)
    z is always 0.0 for computed joints (no depth from midpoint).
    """
    computed = {}

    # Neck: midpoint of NOSE (0) and midpoint of shoulders (11+12)
    nose_x, nose_y = landmarks[0].x, landmarks[0].y
    l_sho_x, l_sho_y = landmarks[11].x, landmarks[11].y
    r_sho_x, r_sho_y = landmarks[12].x, landmarks[12].y
    sho_mid_x = (l_sho_x + r_sho_x) / 2.0
    sho_mid_y = (l_sho_y + r_sho_y) / 2.0
    computed["neck"] = (
        (nose_x + sho_mid_x) / 2.0,
        (nose_y + sho_mid_y) / 2.0,
        0.0,
        max(landmarks[0].visibility, landmarks[11].visibility, landmarks[12].visibility),
    )

    # Spine mid: midpoint of shoulders
    computed["spine_mid"] = (
        sho_mid_x,
        sho_mid_y,
        0.0,
        max(landmarks[11].visibility, landmarks[12].visibility),
    )

    # Spine base: midpoint of hips
    l_hip_x, l_hip_y = landmarks[23].x, landmarks[23].y
    r_hip_x, r_hip_y = landmarks[24].x, landmarks[24].y
    hip_mid_x = (l_hip_x + r_hip_x) / 2.0
    hip_mid_y = (l_hip_y + r_hip_y) / 2.0
    computed["spine_base"] = (
        hip_mid_x,
        hip_mid_y,
        0.0,
        max(landmarks[23].visibility, landmarks[24].visibility),
    )

    return computed


# ============================================
# Collect joints (raw, before smoothing)
# ============================================
def collect_raw_joints(landmarks):
    """Extract raw joints from MediaPipe landmarks + computed midpoints.

    Returns dict: joint_name → (x, y, z, visibility)
    """
    joints = {}

    # Direct landmark mapping
    for mp_idx, our_name in LANDMARK_MAP.items():
        lm = landmarks[mp_idx]
        joints[our_name] = (lm.x, lm.y, lm.z, lm.visibility)

    # Computed midpoints
    computed = compute_computed_joints(landmarks)
    joints.update(computed)

    return joints


# ============================================
# OSC Sending
# ============================================
def send_frame(client, joints, body_id=1):
    """Send a complete frame of (already smoothed) joint data via OSC."""
    client.send_message("/pose/begin", [body_id, len(joints)])

    for name, (x, y, z, conf) in joints.items():
        client.send_message("/pose/joint", [name, float(x), float(y), float(z), float(conf)])

    client.send_message("/pose/end", [])


def send_presence(client, presence):
    """Send global skeleton tracking presence via OSC.

    presence: 1.0 = fully tracked, 0.0 = lost.
    Values between 0-1 are used by the C++ adapter for smooth fade.
    """
    client.send_message("/mediapipe/presence", [float(presence)])


# ============================================
# Frame Processing (with smoothing + retention)
# ============================================
def process_frame(rgb_frame, pose, client, smoother, state):
    """Run MediaPipe Pose, apply smoothing, handle tracking loss.

    Args:
        rgb_frame: RGB image (numpy array, writable).
        pose: MediaPipe Pose object.
        client: OSC UDP client.
        smoother: JointSmoother instance.
        state: dict with keys 'missing_frames' and 'last_valid_pose'.

    Returns:
        MediaPipe results object (for drawing overlay).
    """
    rgb_frame.flags.writeable = False
    results = pose.process(rgb_frame)
    rgb_frame.flags.writeable = True

    if results.pose_landmarks:
        # --- Pose detected ---
        state["missing_frames"] = 0

        # Collect raw joints
        raw_joints = collect_raw_joints(results.pose_landmarks.landmark)

        # Apply temporal smoothing
        smoothed = {}
        for name, (x, y, z, vis) in raw_joints.items():
            sx, sy, sz, svis = smoother.smooth_joint(name, x, y, z, vis)
            smoothed[name] = (sx, sy, sz, svis)

        # Save as last valid pose for retention during brief loss
        state["last_valid_pose"] = smoothed

        # Send smoothed joints + full presence
        send_frame(client, smoothed)
        send_presence(client, 1.0)
    else:
        # --- Pose lost (this frame) ---
        state["missing_frames"] += 1

        if state["last_valid_pose"] is not None and state["missing_frames"] <= MAX_MISSING_FRAMES:
            # Hold last valid pose with decaying presence
            presence = max(0.0, 1.0 - state["missing_frames"] / MAX_MISSING_FRAMES)
            send_frame(client, state["last_valid_pose"])
            send_presence(client, presence)
        else:
            # Fully lost — notify C++ side
            if state["missing_frames"] == MAX_MISSING_FRAMES + 1:
                # Only send once when threshold is crossed
                send_presence(client, 0.0)
                smoother.reset()
                state["last_valid_pose"] = None
            elif state["missing_frames"] > MAX_MISSING_FRAMES + 1:
                # Keep sending 0.0 to ensure C++ eventually gets it
                # (UDP is lossy — repeat for reliability)
                send_presence(client, 0.0)

    return results


def draw_pose_on_frame(frame, results, mp_pose, mp_drawing, frame_count, start_time):
    """Draw MediaPipe pose landmarks and FPS counter on frame (BGR)."""
    if results.pose_landmarks:
        mp_drawing.draw_landmarks(
            frame,
            results.pose_landmarks,
            mp_pose.POSE_CONNECTIONS,
            mp_drawing.DrawingSpec(color=(0, 255, 0), thickness=2, circle_radius=2),
            mp_drawing.DrawingSpec(color=(0, 0, 255), thickness=2),
        )

    elapsed = time.time() - start_time
    fps_str = "0"
    if elapsed > 0:
        fps_str = str(int(frame_count / elapsed))
    cv2.putText(
        frame, "FPS: " + fps_str,
        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2,
    )


# ============================================================
# Webcam source (OpenCV)
# ============================================================
def run_webcam(camera_id, detection_confidence, tracking_confidence, osc_port):
    """Capture from webcam via OpenCV, process with MediaPipe, send OSC."""
    client = udp_client.SimpleUDPClient(OSC_HOST, osc_port)
    mp_pose = mp.solutions.pose
    mp_drawing = mp.solutions.drawing_utils
    pose = mp_pose.Pose(
        static_image_mode=False,
        model_complexity=1,
        smooth_landmarks=True,
        enable_segmentation=False,
        min_detection_confidence=detection_confidence,
        min_tracking_confidence=tracking_confidence,
    )

    cap = cv2.VideoCapture(camera_id)
    if not cap.isOpened():
        print("ERROR: Cannot open camera {}".format(camera_id))
        print("Try changing --camera (0, 1, 2...)")
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, FPS_TARGET)

    frame_time = 1.0 / FPS_TARGET
    start_time = time.time()
    frame_count = 0

    # Per-source smoothing state
    smoother = JointSmoother(alpha=DEFAULT_SMOOTHING_ALPHA)
    state = {"missing_frames": 0, "last_valid_pose": None}

    print("\nRunning (webcam)... Press 'q' in the camera window to quit.\n")

    try:
        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                print("Camera disconnected. Waiting...")
                time.sleep(1)
                continue

            if FLIP_HORIZONTAL:
                frame = cv2.flip(frame, 1)

            # Webcam gives BGR → convert to RGB for MediaPipe
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            results = process_frame(rgb, pose, client, smoother, state)
            draw_pose_on_frame(frame, results, mp_pose, mp_drawing, frame_count, start_time)

            frame_count += 1
            cv2.imshow("MediaPipe Pose Bridge (Webcam)", frame)

            elapsed = time.time() - start_time
            target_elapsed = frame_count / FPS_TARGET
            if elapsed < target_elapsed:
                time.sleep(target_elapsed - elapsed)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    except KeyboardInterrupt:
        pass
    finally:
        cap.release()
        cv2.destroyAllWindows()
        pose.close()
        # Notify C++ that tracking has stopped
        send_presence(client, 0.0)


# ============================================================
# Kinect source (libfreenect)
# ============================================================
def run_kinect(detection_confidence, tracking_confidence, osc_port):
    """Capture from Kinect v1 via libfreenect, process with MediaPipe, send OSC."""
    try:
        import freenect
    except ImportError:
        print("ERROR: freenect not installed. Run: pip install freenect")
        print("Also ensure libfreenect is installed: brew install libfreenect")
        sys.exit(1)

    print("Initializing Kinect v1 via libfreenect...")

    # Quick test: grab one frame to verify the Kinect is connected
    try:
        test_rgb, _ = freenect.sync_get_video()
        if test_rgb is None:
            print("ERROR: No RGB data from Kinect. Check USB connection and power adapter.")
            sys.exit(1)
        print("Kinect RGB: {}x{} — OK".format(test_rgb.shape[1], test_rgb.shape[0]))
    except Exception as e:
        print("ERROR: Cannot access Kinect: {}".format(e))
        print("Make sure the Kinect is connected via USB + power adapter.")
        sys.exit(1)

    client = udp_client.SimpleUDPClient(OSC_HOST, osc_port)
    mp_pose = mp.solutions.pose
    mp_drawing = mp.solutions.drawing_utils
    pose = mp_pose.Pose(
        static_image_mode=False,
        model_complexity=1,
        smooth_landmarks=True,
        enable_segmentation=False,
        min_detection_confidence=detection_confidence,
        min_tracking_confidence=tracking_confidence,
    )

    start_time = time.time()
    frame_count = 0

    # Per-source smoothing state
    smoother = JointSmoother(alpha=DEFAULT_SMOOTHING_ALPHA)
    state = {"missing_frames": 0, "last_valid_pose": None}

    print("\nRunning (Kinect v1)... Press 'q' in the camera window to quit.\n")

    try:
        while True:
            # freenect.sync_get_video() returns RGB directly, blocks until frame ready
            rgb, _ = freenect.sync_get_video()
            if rgb is None:
                print("Kinect frame lost. Waiting...")
                time.sleep(0.1)
                continue

            # freenect gives RGB — flip if needed, then process
            if FLIP_HORIZONTAL:
                rgb = cv2.flip(rgb, 1)

            # MediaPipe expects RGB (freenect gives RGB, no conversion needed)
            # Make a writable copy for MediaPipe to set flags on
            rgb_writable = rgb.copy()
            results = process_frame(rgb_writable, pose, client, smoother, state)

            # Convert RGB → BGR for OpenCV display
            bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
            draw_pose_on_frame(bgr, results, mp_pose, mp_drawing, frame_count, start_time)

            frame_count += 1
            cv2.imshow("MediaPipe Pose Bridge (Kinect v1)", bgr)

            if cv2.waitKey(1) & 0xFF == ord("q"):
                break

    except KeyboardInterrupt:
        pass
    except Exception as e:
        print("\nERROR: {}".format(e))
        traceback.print_exc()
    finally:
        freenect.sync_stop()
        cv2.destroyAllWindows()
        pose.close()
        # Notify C++ that tracking has stopped
        send_presence(client, 0.0)


# ============================================================
# Main
# ============================================================
def main():
    parser = argparse.ArgumentParser(description='MediaPipe Pose → OSC Bridge')
    parser.add_argument('--source', choices=['webcam', 'kinect'], default='webcam',
                        help='Video source: webcam (OpenCV) or kinect (libfreenect)')
    parser.add_argument('--camera', type=int, default=DEFAULT_CAMERA_ID,
                        help='Camera device ID for webcam (default: %(default)s)')
    parser.add_argument('--detection-confidence', type=float,
                        default=DEFAULT_MIN_DETECTION_CONFIDENCE,
                        help='Minimum detection confidence 0.0–1.0 (default: %(default)s)')
    parser.add_argument('--tracking-confidence', type=float,
                        default=DEFAULT_MIN_TRACKING_CONFIDENCE,
                        help='Minimum tracking confidence 0.0–1.0 (default: %(default)s)')
    parser.add_argument('--port', type=int, default=OSC_PORT,
                        help='OSC port to send to (default: %(default)s)')
    args = parser.parse_args()

    print("=" * 50)
    print("MediaPipe Pose → OSC Bridge")
    print("Source: {}".format(args.source))
    if args.source == 'webcam':
        print("Camera ID: {}".format(args.camera))
    print("OSC target: {}:{}".format(OSC_HOST, args.port))
    print("Detection confidence: {:.2f}".format(args.detection_confidence))
    print("Tracking confidence:  {:.2f}".format(args.tracking_confidence))
    print("Smoothing alpha:      {:.2f}".format(DEFAULT_SMOOTHING_ALPHA))
    print("Max missing frames:   {}".format(MAX_MISSING_FRAMES))
    print("=" * 50)

    if args.source == 'kinect':
        run_kinect(args.detection_confidence, args.tracking_confidence, args.port)
    else:
        run_webcam(args.camera, args.detection_confidence,
                   args.tracking_confidence, args.port)

    print("Done.")


if __name__ == "__main__":
    main()
