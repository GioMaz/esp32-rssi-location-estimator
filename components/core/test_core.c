#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "core.h" // contiene PreprocData, Features, FeaturesLabel, Pos, aps_to_features_set, knn
#include "../network/network.h" // contiene AccessPoint, MAX_APS

#define MAX_POS 1000
#define MAX_TOKENS 128 // limite superiore ragionevole

// Funzione per convertire stringa MAC in array di byte
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

// Struttura per i dati di posizione
typedef struct {
    int x, y;
    AccessPoint aps[MAX_APS];
} PositionData;

// Funzione per calcolare la distanza euclidea tra due posizioni
double position_distance(int x1, int y1, int x2, int y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

int main() {
    FILE *fp = fopen("wifi_map.csv", "r");
    if (!fp) {
        perror("Errore apertura file wifi_map.csv");
        return EXIT_FAILURE;
    }

    char line[1024];
    // Leggi e salta l'header
    if (!fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "Errore lettura header\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    PositionData data[MAX_POS];
    int count = 0;

    // PARSING DINAMICO DEL CSV
    while (fgets(line, sizeof(line), fp) && count < MAX_POS) {
        char *tokens[MAX_TOKENS];
        int ntok = 0;
        char *p = strtok(line, ",\n");
        while (p && ntok < MAX_TOKENS) {
            tokens[ntok++] = p;
            p = strtok(NULL, ",\n");
        }

        // Ci aspettiamo almeno 2 colonne (x,y) + 2*n AP
        int num_aps = (ntok - 2) / 2;
        if (ntok < 4 || num_aps <= 0) {
            continue; // riga vuota o malformata
        }
        if (num_aps > MAX_APS) num_aps = MAX_APS;

        // Leggi coordinate
        data[count].x = atoi(tokens[0]);
        data[count].y = atoi(tokens[1]);

        // Leggi fino a num_aps AP, inizializza gli altri a RSSI bassi
        for (int i = 0; i < MAX_APS; i++) {
            if (i < num_aps) {
                mac_from_string(tokens[2 + 2*i], data[count].aps[i].mac);
                data[count].aps[i].rssi = atoi(tokens[2 + 2*i + 1]);
            } else {
                memset(data[count].aps[i].mac, 0, 6);
                data[count].aps[i].rssi = -100;
            }
        }
        count++;
    }
    
    fclose(fp);
    
    if (count == 0) {
        fprintf(stderr, "Nessun dato caricato!\n");
        return EXIT_FAILURE;
    }
    
    printf("Caricati %d punti dal dataset\n", count);

    // Scegli punto di test casuale
    srand((unsigned)time(NULL));
    int test_idx = rand() % count;
    PositionData *test_point = &data[test_idx];
    
    printf("=== TEST ALGORITMO KNN ===\nPosizione reale selezionata: (%d,%d)\n",
           test_point->x, test_point->y);

    // Genera AP rumorosi per la query
    AccessPoint noisy_aps[MAX_APS];
    for (int i = 0; i < MAX_APS; i++) {
        memcpy(noisy_aps[i].mac, test_point->aps[i].mac, 6);
        noisy_aps[i].rssi = add_noise(test_point->aps[i].rssi);
    }

    // Costruzione dataset_features
    FeaturesLabel *dataset_features = malloc(count * sizeof(FeaturesLabel));
    if (!dataset_features) {
        perror("Memoria dataset_features");
        return EXIT_FAILURE;
    }

    // Calcola parametri preprocess su tutti gli AP
    PreprocData global_preproc;
    AccessPoint *all_aps = malloc(count * MAX_APS * sizeof(AccessPoint));
    if (!all_aps) {
        perror("Memoria all_aps");
        free(dataset_features);
        return EXIT_FAILURE;
    }

    int tot = 0;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < MAX_APS; j++) {
            all_aps[tot++] = data[i].aps[j];
        }
    }

    Features *dummy = malloc(tot * sizeof(Features));
    if (!dummy) {
        perror("Memoria dummy features");
        free(all_aps);
        free(dataset_features);
        return EXIT_FAILURE;
    }
    
    aps_to_features_set(all_aps, dummy, tot, &global_preproc);
    free(all_aps);
    free(dummy);

    printf("Parametri preprocess: MAC %llu - %llu, RSSI %d - %d\n",
           global_preproc.min_mac, global_preproc.max_mac,
           global_preproc.min_rssi, global_preproc.max_rssi);

    // Calcola signature per ogni punto mappa
    for (int i = 0; i < count; i++) {
        Features feats[MAX_APS];
        aps_to_features_set(data[i].aps, feats, MAX_APS, &global_preproc);
        
        double sx = 0, sy = 0, wsum = 0;
        for (int j = 0; j < MAX_APS; j++) {
            double w = 1.0 + j;
            sx += feats[j].x * w;
            sy += feats[j].y * w;
            wsum += w;
        }
        
        if (wsum > 0) { 
            sx /= wsum; 
            sy /= wsum; 
        }
        
        dataset_features[i].features.x = sx;
        dataset_features[i].features.y = sy;
        dataset_features[i].label.x = data[i].x;
        dataset_features[i].label.y = data[i].y;
    }

    // Calcola signature query
    Features qfeats[MAX_APS];
    aps_to_features_set(noisy_aps, qfeats, MAX_APS, &global_preproc);
    
    double qx = 0, qy = 0, qw = 0;
    for (int j = 0; j < MAX_APS; j++) {
        double w = 1.0 + j;
        qx += qfeats[j].x * w;
        qy += qfeats[j].y * w;
        qw += w;
    }
    
    if (qw > 0) { 
        qx /= qw; 
        qy /= qw; 
    }
    
    Features query_feature = { qx, qy };
    printf("Query features: (%.4f,%.4f)\n", qx, qy);

    // Esegui KNN per diversi k
    int k_values[] = {1, 3, 5, 7};
    int num_k_values = sizeof(k_values) / sizeof(k_values[0]);
    
    for (int ki = 0; ki < num_k_values; ki++) {
        int k = k_values[ki];
        FeaturesLabel *copy = malloc(count * sizeof(FeaturesLabel));
        if (!copy) {
            fprintf(stderr, "Errore allocazione memoria per k=%d\n", k);
            continue;
        }
        
        memcpy(copy, dataset_features, count * sizeof(FeaturesLabel));
        Pos est = knn(copy, count, k, &query_feature);
        
        double err = position_distance(test_point->x, test_point->y,
                                     (int)est.x, (int)est.y);
        printf("k=%d -> stimata=(%d,%d), errore=%.2f\n",
               k, (int)est.x, (int)est.y, err);
        free(copy);
    }

    // Esporta CSV delle feature
    FILE *fout = fopen("features_dataset.csv", "w");
    if (!fout) { 
        perror("Errore apertura features_dataset.csv"); 
    } else {
        fprintf(fout, "fx,fy,label_x,label_y,type\n");
        
        for (int i = 0; i < count; i++) {
            fprintf(fout, "%.6f,%.6f,%d,%d,train\n",
                    dataset_features[i].features.x,
                    dataset_features[i].features.y,
                    dataset_features[i].label.x,
                    dataset_features[i].label.y);
        }
        
        fprintf(fout, "%.6f,%.6f,%d,%d,query\n",
                query_feature.x,
                query_feature.y,
                test_point->x,
                test_point->y);
        
        fclose(fout);
        printf("Dataset delle feature scritto in features_dataset.csv\n");
    }

    free(dataset_features);
    return EXIT_SUCCESS;
}