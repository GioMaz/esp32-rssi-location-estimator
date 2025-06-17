import numpy as np
import matplotlib.pyplot as plt
import random
import csv

# Dimensioni stanza
width, height = 18, 4

# Numero di AP
num_aps = 4

# Genera MAC address casuali per ogni AP
def random_mac():
    return [random.randint(0x00, 0xFF) for _ in range(6)]

macs = [random_mac() for _ in range(num_aps)]

# Posizioni AP (fisse)
ap_positions = [
    (10, 3),
    (15, 0),
    (0, 2),
    (4, 1),
]

colors = [
    (1, 0, 0),     # Rosso
    (0, 1, 0),     # Verde
    (0, 0, 1),     # Blu
    (1, 1, 0),     # Giallo
]

def simulate_rssi(x_ap, y_ap, x, y):
    dist = np.sqrt((x - x_ap)**2 + (y - y_ap)**2)
    if dist > 5:
        return None
    return -30 - dist * 5  # dBm: -30 vicino, -55 a 5 celle

# --- Generazione heatmap RGB (combinazione AP) ---
heatmap_rgb = np.zeros((height, width, 3))

for (pos_x, pos_y), color in zip(ap_positions, colors):
    for y in range(height):
        for x in range(width):
            rssi = simulate_rssi(pos_x, pos_y, x, y)
            if rssi is not None:
                intensity = max(0, min(1, (rssi + 80) / 50))
                heatmap_rgb[y, x] += np.array(color) * intensity

heatmap_rgb = np.clip(heatmap_rgb, 0, 1)

# --- Visualizzazione heatmap ---
plt.figure(figsize=(12, 3))
plt.imshow(heatmap_rgb, origin='upper')
plt.title('Mappa WiFi combinata (4 AP)')
plt.xlabel('X')
plt.ylabel('Y')
plt.grid(True, which='both', color='gray', linestyle='--', linewidth=0.5)

for (pos_x, pos_y), color in zip(ap_positions, colors):
    plt.scatter(pos_x, pos_y, c=[color], label=f'AP at ({pos_x},{pos_y})', edgecolors='black', s=100)

plt.legend()
plt.xticks(np.arange(width))
plt.yticks(np.arange(height))
plt.show()

# --- Salvataggio dati su CSV ---

filename = 'wifi_map.csv'
with open(filename, mode='w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    # Header: x,y,mac1,rssi1,mac2,rssi2,...
    header = ['x', 'y']
    for i, mac in enumerate(macs):
        mac_str = ''.join(f'{b:02X}' for b in mac)
        header.append(f'mac{i+1}')
        header.append(f'rssi{i+1}')
    writer.writerow(header)

    for y in range(height):
        for x in range(width):
            row = [x, y]
            for (mx, my), mac in zip(ap_positions, macs):
                rssi = simulate_rssi(mx, my, x, y)
                rssi_val = int(round(rssi)) if rssi is not None else -100  # <-- qui
                mac_str = ''.join(f'{b:02X}' for b in mac)
                row.append(mac_str)
                row.append(rssi_val)
            writer.writerow(row)


print(f"CSV generato: {filename}")
print("MAC address usati:")
for i, mac in enumerate(macs):
    print(f"AP{i+1}: {''.join(f'{b:02X}' for b in mac)}")
