import pandas as pd
import matplotlib.pyplot as plt

# Carica il dataset
df = pd.read_csv("features_dataset.csv")

# Separa train e query
train = df[df["type"] == "train"]
query = df[df["type"] == "query"]

plt.figure(figsize=(8, 6))
# Punti di training
plt.scatter(train["fx"], train["fy"], alpha=0.6, label="Training")
# Punto query
plt.scatter(query["fx"], query["fy"], s=100, marker='X', edgecolors='black',
            label="Query rumorosa")

plt.title("Feature Space KNN Wi-Fi")
plt.xlabel("Feature x  (MAC normalizzato)")
plt.ylabel("Feature y  (RSSI normalizzato)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
