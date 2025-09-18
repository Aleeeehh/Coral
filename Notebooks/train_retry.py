#OTTIENI YOLO11N.PT DA ULTRALYTICS
from ultralytics import YOLO
import sys
import os
import pandas as pd
import shutil

# Load a model
model = YOLO("yolo11n.pt") #esporta in .pt

# Variabili usate in tutte le casistiche
cartellaCheckpoint = "runs/detect/train/weights/last.pt"
dataset_path = "coco.yaml"
device = "mps"
freeze = 11 #congela l'intera backbone(vanno da 0 a 10), pure l'ultimo blocco C2PSA di attention
classes = [0]
batch = 16 # 16 e -1 sono equivalenti con mps
epochs = 50
val = False
imgsz = 640
workers = 0 # sembra il migliore in velocità e per memory leaks
close_mosaic = 10 #default ma specificato perchè interessante
mosaic = 1.0 #default ma specificato perchè interessante

# Lista per salvare le metriche della classe person
person_metrics_history = []


# Definiamo il callback custom
def val_person_callback(trainer):
    # viene chiamato a fine epoca
    model_checkpoint = YOLO(cartellaCheckpoint)
    metrics = model_checkpoint.val(
        data="/Users/alessioprato/Desktop/Tesi Nuova/Notebooks/datasets/coco_subset_3000_600/coco_subset.yaml",
        split="val",
        verbose=False,
        conf=0.5,
    )
            # Estrai le metriche per la classe person
    epoch_metrics = {
            'epoch': trainer.epoch + 1,
            'precision': round(metrics.box.p[0], 4),
            'recall': round(metrics.box.r[0], 4),
            'mAP50': round(metrics.box.ap50[0], 4),
            'mAP50-95': round(metrics.box.ap[0], 4),
        }
    person_metrics_history.append(epoch_metrics)



    #crea la directory
    epoch_num = trainer.epoch + 1
    checkpoint_dir = f"runs/detect/train/checkpoints"
    os.makedirs(checkpoint_dir, exist_ok=True)

   # Salva in CSV (aggiorna il file ad ogni epoca)
    csv_filename = "runs/detect/train/checkpoints/person_results.csv"

  # Se il file CSV esiste già, carica i dati esistenti
    if os.path.exists(csv_filename):
      existing_df = pd.read_csv(csv_filename)
    # Converte in lista di dizionari per mantenere la compatibilità
      existing_data = existing_df.to_dict('records')
    # Combina i dati esistenti con SOLO l'epoca corrente
      all_metrics = existing_data + [epoch_metrics]  # Solo l'epoca corrente  
    else:
      all_metrics = [epoch_metrics]  # Solo l'epoca corrente

    # Crea il DataFrame con tutti i dati
    df = pd.DataFrame(all_metrics)
    df.to_csv(csv_filename, index=False)
    
    print(f"\n📊 Validation solo 'person' (epoca {trainer.epoch+1}): Precisione={metrics.box.p[0]:.4f}, Recall={metrics.box.r[0]:.4f}, mAP50={metrics.box.ap50[0]:.4f}, mAP50-95={metrics.box.ap[0]:.4f}")
        
    
    # Salva il modello checkpoint
    source_file = cartellaCheckpoint
    destination_file = os.path.join(checkpoint_dir, f"epoch{epoch_num}.pt")

    try:
            # Copia il file last.pt
            if os.path.exists(source_file):
                shutil.copy2(source_file, destination_file)
            
                # Verifica che il file sia stato copiato
                if os.path.exists(destination_file):
                    file_size = os.path.getsize(destination_file)
                    print(f"✅ Checkpoint copiato con successo!")
                else:
                    print(f"❌ Errore: Il file {destination_file} non è stato copiato!")
            else:
                print(f"❌ Errore: Il file sorgente {source_file} non esiste!")
                
    except Exception as copy_error:
            print(f"❌ Errore nella copia del checkpoint: {copy_error}")


# Funzione per eseguire il training con gestione degli errori
def train_with_retry():
    try:
        # Controlla se esiste il checkpoint per decidere se fare resume o training da zero
        if os.path.exists(cartellaCheckpoint):
            print("Checkpoint trovato! Avvio training con resume=True...")
            model_checkpoint = YOLO(cartellaCheckpoint)
            # Registra il callback nel modello
            model_checkpoint.add_callback("on_fit_epoch_end", val_person_callback)
            results = model_checkpoint.train(
                data=dataset_path,
                device=device,
                freeze=freeze,
                classes=classes,
                batch=batch,  
                epochs=epochs,
                val=val, 
                imgsz=imgsz,
                resume=True,  # Continua dal checkpoint
                workers=workers,
                close_mosaic=close_mosaic,
                mosaic=mosaic,
            )
        else:
            print("Nessun checkpoint trovato. Avvio training da zero...")
            # Registra il callback nel modello
            model.add_callback("on_fit_epoch_end", val_person_callback)
            results = model.train(
                data=dataset_path,
                device=device,
                freeze=freeze,
                classes=classes,
                batch=batch,  
                epochs=epochs,
                val=val, 
                imgsz=imgsz,
                workers=workers,
                close_mosaic=close_mosaic,
                mosaic=mosaic,
            )
        return results
        
    except Exception as e:
        # Gestisce TUTTE le eccezioni e prova sempre a fare resume se esiste il checkpoint
        print(f"Errore rilevato: {e}")
        
        if os.path.exists(cartellaCheckpoint):
            print("Riavvio training dal checkpoint last.pt con resume=True...")
            
            try:
                # Carica il modello dal checkpoint
                model_checkpoint = YOLO(cartellaCheckpoint)
                # Registra il callback nel modello
                model_checkpoint.add_callback("on_fit_epoch_end", val_person_callback)
                # Riavvia il training con resume=True
                results = model_checkpoint.train(
                    data=dataset_path,
                    device=device,
                    freeze=freeze,
                    classes=classes,
                    batch=batch,  
                    epochs=epochs,
                    val=val, 
                    imgsz=imgsz,
                    resume=True,  # Argomento per continuare il training
                    workers=workers,
                    close_mosaic=close_mosaic,
                    mosaic=mosaic,
                )
                return results
            except Exception as e2:
                # Se anche il resume fallisce, riprova ricorsivamente
                print(f"Errore rilevato: {e}")
                print("riprovo...")
                return train_with_retry()  # Chiamata ricorsiva
        else:
            # Se non esiste checkpoint, rilancia l'eccezione originale
            print("Nessun checkpoint disponibile per il resume. Training fallito.")
            raise e

# Esegui il training
try:
    results = train_with_retry()
    print("Training completato con successo!")
except Exception as e:
    print(f"Training fallito definitivamente: {e}")
    sys.exit(1)