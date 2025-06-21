#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "core.h"         // include core
#include "../network/network.h"  // include per AccessPoint, MAX_APS

#define MAX_POS 1000


// Rimuovi la definizione di AccessPoint e MAX_APS da qui
// usa quella definita in network.h

void mac_from_string(const char *str, uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        sscanf(str + 2*i, "%2hhX", &mac[i]);
    }
}

// Funzione per aggiungere rumore +/-5 dBm agli RSSI
int add_noise(int rssi) {
    int noise = (rand() % 11) - 5; // -5..+5
    return rssi + noise;
}

typedef struct {
    int x, y;
    AccessPoint aps[MAX_APS];
} PositionData;

int main() {
    FILE *fp = fopen("wifi_map.csv", "r");
    if (!fp) {
        perror("Errore apertura file");
        return 1;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "Errore lettura header\n");
        return 1;
    }

    PositionData data[MAX_POS];
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        int x, y;
        char mac_str[MAX_APS][13];
        int rssis[MAX_APS];

        int n = sscanf(line, "%d,%d,%12[^,],%d,%12[^,],%d,%12[^,],%d,%12[^,],%d",
            &x, &y,
            mac_str[0], &rssis[0],
            mac_str[1], &rssis[1],
            mac_str[2], &rssis[2],
            mac_str[3], &rssis[3]);

        if (n < 10) {
            fprintf(stderr, "Errore parsing riga: %s\n", line);
            continue;
        }

        data[count].x = x;
        data[count].y = y;
        for (int i = 0; i < MAX_APS; i++) {
            mac_from_string(mac_str[i], data[count].aps[i].mac);
            data[count].aps[i].rssi = rssis[i];
        }
        count++;
    }
    fclose(fp);

    srand(time(NULL));

    // Scegli posizione reale casuale
    int idx = rand() % count;

    // Genera RSSI rumorosi
    AccessPoint noisy_aps[MAX_APS];
    for (int i = 0; i < MAX_APS; i++) {
        memcpy(noisy_aps[i].mac, data[idx].aps[i].mac, 6);
        noisy_aps[i].rssi = add_noise(data[idx].aps[i].rssi);
    }

    printf("Posizione reale: (%d,%d)\n", data[idx].x, data[idx].y);
    
    printf("RSSI rumorosi simulati:\n");
    for (int i = 0; i < MAX_APS; i++) {
        printf("  AP %d MAC: %02X:%02X:%02X:%02X:%02X:%02X RSSI: %d\n",
            i+1,
            noisy_aps[i].mac[0], noisy_aps[i].mac[1], noisy_aps[i].mac[2],
            noisy_aps[i].mac[3], noisy_aps[i].mac[4], noisy_aps[i].mac[5],
            noisy_aps[i].rssi);
    } 

    // --- Preparazione per knn ---
    PreprocData preproc;
    Features noisy_features[MAX_APS];
    aps_to_features_set(noisy_aps, noisy_features, MAX_APS, &preproc);

    FeaturesLabel dataset_features[MAX_POS * MAX_APS];
    int dataset_features_count = 0;

    for (int i = 0; i < count; i++) {
        Features f_set[MAX_APS];
        PreprocData tmp_preproc;
        aps_to_features_set(data[i].aps, f_set, MAX_APS, &tmp_preproc);

        // Per ogni posizione nel dataset, calcola la media dei features degli AP
        double x_sum = 0, y_sum = 0;
        for (int j = 0; j < MAX_APS; j++) {
            x_sum += f_set[j].x;
            y_sum += f_set[j].y;
        }
        Features avg_feature = { x_sum / MAX_APS, y_sum / MAX_APS };

        dataset_features[dataset_features_count].features = avg_feature;
        dataset_features[dataset_features_count].label.x = data[i].x;
        dataset_features[dataset_features_count].label.y = data[i].y;
        dataset_features_count++;
    }

    // Calcola feature media dei noisy_aps (query)
    double x_sum = 0, y_sum = 0;
    for (int i = 0; i < MAX_APS; i++) {
        x_sum += noisy_features[i].x;
        y_sum += noisy_features[i].y;
    }
    Features query_feature = { x_sum / MAX_APS, y_sum / MAX_APS };

    // Chiama knn con k=3
    Pos estimated_pos = knn(dataset_features, dataset_features_count, 3, &query_feature);

    printf("Posizione stimata: (%d,%d)\n", estimated_pos.x, estimated_pos.y);

    return 0;
}
