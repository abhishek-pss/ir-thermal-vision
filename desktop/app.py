from __future__ import annotations

import threading
import time
from typing import Optional

import cv2
import numpy as np
from flask import Flask, Response, jsonify, render_template, request

app = Flask(__name__)

WIDTH = 32
HEIGHT = 24
EXPECTED_VALUES = WIDTH * HEIGHT
EXPECTED_BYTES = EXPECTED_VALUES * 2  # int16 centi-degrees Celsius

_frame_lock = threading.Lock()
_latest_frame: Optional[np.ndarray] = None
_last_frame_time = 0.0
_frame_count = 0


def frame_status() -> dict:
    with _frame_lock:
        age = (time.time() - _last_frame_time) if _last_frame_time else None
        return {
            "connected": age is not None and age < 5.0,
            "frame_age_seconds": round(age, 2) if age is not None else None,
            "frames_received": _frame_count,
        }


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/health")
def health():
    return jsonify({"ok": True, **frame_status()})


@app.post("/api/frame")
def receive_frame():
    global _latest_frame, _last_frame_time, _frame_count

    payload = request.get_data(cache=False)
    if len(payload) != EXPECTED_BYTES:
        return jsonify({
            "ok": False,
            "error": f"Expected {EXPECTED_BYTES} bytes for a 32x24 int16 frame; received {len(payload)}.",
        }), 400

    # Firmware sends little-endian signed int16, scaled by 100.
    raw = np.frombuffer(payload, dtype="<i2")
    frame = raw.astype(np.float32).reshape((HEIGHT, WIDTH)) / 100.0
    frame[raw.reshape((HEIGHT, WIDTH)) == -32768] = np.nan

    if not np.isfinite(frame).any():
        return jsonify({"ok": False, "error": "Frame contains no valid temperature pixels."}), 400

    with _frame_lock:
        _latest_frame = frame.copy()
        _last_frame_time = time.time()
        _frame_count += 1

    return jsonify({"ok": True, "frame": _frame_count})


def make_thermal_image(frame: np.ndarray) -> np.ndarray:
    valid = np.isfinite(frame)
    values = frame[valid]
    low = float(np.percentile(values, 2))
    high = float(np.percentile(values, 98))
    if high - low < 0.5:
        high = low + 0.5

    safe = np.nan_to_num(frame, nan=low, posinf=high, neginf=low)
    normalized = np.clip((safe - low) / (high - low), 0.0, 1.0)
    gray = (normalized * 255).astype(np.uint8)

    # Upscale the low-resolution thermal array for a smoother video-like display.
    enlarged = cv2.resize(gray, (640, 480), interpolation=cv2.INTER_CUBIC)
    colored = cv2.applyColorMap(enlarged, cv2.COLORMAP_INFERNO)

    min_temp = float(np.nanmin(frame))
    max_temp = float(np.nanmax(frame))
    mean_temp = float(np.nanmean(frame))
    min_y, min_x = np.unravel_index(np.nanargmin(frame), frame.shape)
    max_y, max_x = np.unravel_index(np.nanargmax(frame), frame.shape)

    # Mark the hottest pixel. This is a hotspot marker, not human identification.
    hot_point = (int((max_x + 0.5) * 20), int((max_y + 0.5) * 20))
    cv2.drawMarker(colored, hot_point, (255, 255, 255), cv2.MARKER_CROSS, 22, 2)
    cv2.putText(colored, f"HOT {max_temp:.1f} C", (12, 28),
                cv2.FONT_HERSHEY_SIMPLEX, 0.65, (255, 255, 255), 2, cv2.LINE_AA)
    cv2.putText(colored, f"MIN {min_temp:.1f} C   AVG {mean_temp:.1f} C",
                (12, 458), cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                (255, 255, 255), 2, cv2.LINE_AA)
    return colored


def mjpeg_stream():
    placeholder = np.zeros((480, 640, 3), dtype=np.uint8)
    cv2.putText(placeholder, "WAITING FOR ESP THERMAL FRAMES", (35, 235),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (220, 220, 220), 2, cv2.LINE_AA)

    while True:
        with _frame_lock:
            frame = _latest_frame.copy() if _latest_frame is not None else None
            age = (time.time() - _last_frame_time) if _last_frame_time else None

        if frame is None:
            image = placeholder.copy()
        elif age is not None and age > 5.0:
            image = make_thermal_image(frame)
            cv2.putText(image, "SIGNAL STALE - CHECK ESP32 / WIFI", (12, 55),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 2, cv2.LINE_AA)
        else:
            image = make_thermal_image(frame)

        ok, encoded = cv2.imencode(".jpg", image, [int(cv2.IMWRITE_JPEG_QUALITY), 85])
        if ok:
            yield (b"--frame\r\n"
                   b"Content-Type: image/jpeg\r\n\r\n" + encoded.tobytes() + b"\r\n")
        time.sleep(0.08)


@app.get("/video_feed")
def video_feed():
    return Response(mjpeg_stream(), mimetype="multipart/x-mixed-replace; boundary=frame")


@app.get("/api/stats")
def stats():
    with _frame_lock:
        frame = _latest_frame.copy() if _latest_frame is not None else None
    result = frame_status()
    if frame is not None:
        result.update({
            "minimum_c": round(float(np.nanmin(frame)), 2),
            "maximum_c": round(float(np.nanmax(frame)), 2),
            "average_c": round(float(np.nanmean(frame)), 2),
        })
    return jsonify(result)


if __name__ == "__main__":
    # Listen on the LAN so the ESP can send frames from the same Wi-Fi network.
    app.run(host="0.0.0.0", port=5000, threaded=True, debug=False)
