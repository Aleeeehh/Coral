#OTTIENI YOLO11N.PT DA ULTRALYTICS
from ultralytics import YOLO

# Load a model
model = YOLO("yolo11n.pt") #esporta in .pt

# CONTINUA IL FINETUNING
cartellaCheckpoint = "runs/detect/train/weights/last.pt"
dataset_path = "/Users/alessioprato/Desktop/Tesi Nuova/Notebooks/datasets/coco_subset_1000_200/coco_subset.yaml"
model = YOLO(cartellaCheckpoint)  # percorso del checkpoint dei pesi
results = model.train(
    data=dataset_path,
    device="mps",
    freeze=10,
    classes=[0],
    batch=8,  
    epochs=10,
    val=False, 
    imgsz=640,
    #resume=True,  # Argomento per continuare il training
    workers=0,
)