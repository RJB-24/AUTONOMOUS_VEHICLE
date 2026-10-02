"""Laptop vision controller: fetch ESP32 JPEGs, infer, and send safe commands by UDP."""
import argparse, socket, time, urllib.request
import cv2
import numpy as np

LABELS = ["blocked", "forward", "left", "right"]
CODES = {"blocked":"B", "forward":"F", "left":"L", "right":"R"}

def load_interpreter(path):
    try:
        from tflite_runtime.interpreter import Interpreter
    except ImportError:
        from tensorflow.lite.python.interpreter import Interpreter
    it = Interpreter(model_path=path, num_threads=2)
    it.allocate_tensors()
    return it

def fetch_frame(url, timeout):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        data = np.frombuffer(r.read(), np.uint8)
    frame = cv2.imdecode(data, cv2.IMREAD_COLOR)
    if frame is None: raise RuntimeError("Invalid camera JPEG")
    return frame

def infer(it, frame):
    inp, out = it.get_input_details()[0], it.get_output_details()[0]
    h,w = inp["shape"][1:3]
    # Match training RGB input and centre crop.
    sh,sw=frame.shape[:2]; side=min(sh,sw); y=(sh-side)//2; x=(sw-side)//2
    rgb=cv2.cvtColor(frame[y:y+side,x:x+side],cv2.COLOR_BGR2RGB)
    image=cv2.resize(rgb,(w,h),interpolation=cv2.INTER_AREA)[None]
    if inp["dtype"] == np.uint8: image=image.astype(np.uint8)
    else: image=image.astype(np.float32)
    it.set_tensor(inp["index"],image); it.invoke()
    raw=it.get_tensor(out["index"])[0]
    if out["dtype"] == np.uint8:
        scale,zero=out["quantization"]; scores=(raw.astype(float)-zero)*scale
    else: scores=raw.astype(float)
    idx=int(np.argmax(scores)); return LABELS[idx],float(scores[idx])

def edge_fallback(frame):
    """Optional lightweight heuristic, not a substitute for trained free-space segmentation."""
    small=cv2.resize(frame,(160,120)); gray=cv2.cvtColor(small,cv2.COLOR_BGR2GRAY)
    edges=cv2.Canny(gray,60,140); roi=edges[55:115]
    thirds=np.array_split(roi,3,axis=1); density=[np.mean(x>0) for x in thirds]
    best=int(np.argmin(density))
    return ("left","forward","right")[best], max(0.0, min(0.65, 1.0-density[best]*3))

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--esp",required=True,help="ESP32 IP, e.g. 192.168.1.90")
    p.add_argument("--model",default="ml/models/navigation_int8.tflite")
    p.add_argument("--confidence",type=float,default=.70)
    p.add_argument("--edge-fallback",action="store_true")
    p.add_argument("--show",action="store_true")
    a=p.parse_args(); it=load_interpreter(a.model); sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    target=(a.esp,4210); url=f"http://{a.esp}/capture"
    last_ok=0
    try:
        while True:
            try:
                frame=fetch_frame(url,1.0); label,conf=infer(it,frame); last_ok=time.monotonic()
                if conf<a.confidence and a.edge_fallback: label,conf=edge_fallback(frame)
                if conf<a.confidence: label="blocked"
                msg=f"V,{CODES[label]},{round(conf*100)}\n".encode(); sock.sendto(msg,target)
                print(f"\r{label:8s} {conf:5.1%}",end="",flush=True)
                if a.show:
                    cv2.putText(frame,f"{label} {conf:.0%}",(10,30),cv2.FONT_HERSHEY_SIMPLEX,.8,(0,255,0),2)
                    cv2.imshow("Robot camera",frame)
                    if cv2.waitKey(1)&0xff==ord('q'): break
            except Exception as e:
                sock.sendto(b"V,B,0\n",target); print(f"\nFail-safe stop: {e}")
                time.sleep(.15)
            if time.monotonic()-last_ok>1.0: sock.sendto(b"V,B,0\n",target)
    finally:
        for _ in range(3): sock.sendto(b"V,B,0\n",target)
        cv2.destroyAllWindows()
if __name__=="__main__": main()
