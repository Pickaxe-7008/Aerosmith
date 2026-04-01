import cv2
from ultralytics import YOLO

model = YOLO("yolov8n.pt")

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Error: Could not open video source.")
    exit()

ret, frame = cap.read()

cv2.imshow("test", frame)

results = model(frame)
print(results)
