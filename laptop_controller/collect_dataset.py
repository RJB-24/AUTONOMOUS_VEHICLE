"""Collect labelled navigation images from the ESP32-CAM.
Keys: F=forward, L=left, R=right, B=blocked, SPACE=pause, Q=quit.
Only save a frame after deliberately assigning its safest action.
"""
import argparse, time, urllib.request
from pathlib import Path
import cv2
import numpy as np

KEYS = {ord("f"): "forward", ord("l"): "left", ord("r"): "right", ord("b"): "blocked"}

def fetch(url):
    with urllib.request.urlopen(url, timeout=1.5) as response:
        data = np.frombuffer(response.read(), np.uint8)
    image = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if image is None:
        raise RuntimeError("ESP32 returned an invalid image")
    return image

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--esp", required=True, help="ESP32 IP address")
    parser.add_argument("--output", default="ml/dataset")
    parser.add_argument("--interval", type=float, default=0.25,
                        help="Minimum seconds between saved images")
    args = parser.parse_args()
    root = Path(args.output)
    for label in KEYS.values():
        (root / label).mkdir(parents=True, exist_ok=True)
    url = f"http://{args.esp}/capture"
    selected = None
    last_saved = 0.0
    counts = {label: len(list((root / label).glob("*.jpg"))) for label in KEYS.values()}
    print("F/L/R/B selects and records a class; SPACE pauses; Q quits.")
    try:
        while True:
            try:
                frame = fetch(url)
            except Exception as exc:
                print(f"Camera error: {exc}")
                time.sleep(0.2)
                continue
            view = frame.copy()
            status = selected or "PAUSED"
            cv2.putText(view, f"Label: {status}", (8, 25), cv2.FONT_HERSHEY_SIMPLEX,
                        0.7, (0, 255, 0) if selected else (0, 0, 255), 2)
            cv2.putText(view, "F forward | L left | R right | B blocked | SPACE pause | Q quit",
                        (8, view.shape[0]-10), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (255,255,255), 1)
            cv2.imshow("Dataset collection", view)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27): break
            if key == ord(" "): selected = None
            elif key in KEYS: selected = KEYS[key]
            now = time.monotonic()
            if selected and now-last_saved >= args.interval:
                stamp = time.time_ns()
                cv2.imwrite(str(root/selected/f"{selected}_{stamp}.jpg"), frame)
                counts[selected] += 1; last_saved = now
                print("\r" + " | ".join(f"{k}:{v}" for k,v in counts.items()), end="", flush=True)
    finally:
        cv2.destroyAllWindows()
        print("\nFinal counts:", counts)

if __name__ == "__main__":
    main()
