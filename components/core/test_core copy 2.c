#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "core.h"         // include core
#include "../network/network.h"  // include per AccessPoint, MAX_APS

#define MAX_POS 1000

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

// Funzione per calcolare la distanza euclidea tra due posizioni
double position_distance(int x1, int y1, int x2, int y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

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

    // Carica tutti i dati dal CSV
    while (fgets(line, sizeof(line), fp) && count < MAX_POS) {
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

    printf("Caricati %d punti dal dataset\n", count);
    
    if (count == 0) {
        fprintf(stderr, "Nessun dato caricato!\n");
        return 1;
    }
    
    srand(time(NULL));

    // Scegli posizione reale casuale dal dataset per il test
    int test_idx = rand() % count;
    PositionData *test_point = &data[test_idx];

    printf("=== TEST ALGORITMO KNN ===\n");
    printf("Posizione reale selezionata: (%d,%d)\n", test_point->x, test_point->y);
    
    // Genera RSSI rumorosi basati sui dati reali della posizione selezionata
    AccessPoint noisy_aps[MAX_APS];
    printf("RSSI originali vs rumorosi:\n");
    for (int i = 0; i < MAX_APS; i++) {
        memcpy(noisy_aps[i].mac, test_point->aps[i].mac, 6);
        int original_rssi = test_point->aps[i].rssi;
        noisy_aps[i].rssi = add_noise(original_rssi);
        printf("  AP %d: RSSI %d -> %d (diff: %d)\n", 
               i+1, original_rssi, noisy_aps[i].rssi, 
               noisy_aps[i].rssi - original_rssi);
    }

    // --- Preparazione dataset per KNN ---
    // Alloca memoria per il dataset KNN
    FeaturesLabel *dataset_features = malloc(count * sizeof(FeaturesLabel));
    if (!dataset_features) {
        fprintf(stderr, "Errore allocazione memoria per dataset\n");
        return 1;
    }

    // Calcola i parametri di preprocessing globali su tutto il dataset
    PreprocData global_preproc;
    AccessPoint *all_aps = malloc(count * MAX_APS * sizeof(AccessPoint));
    if (!all_aps) {
        fprintf(stderr, "Errore allocazione memoria per preprocessing\n");
        free(dataset_features);
        return 1;
    }
    
    int total_aps = 0;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < MAX_APS; j++) {
            all_aps[total_aps++] = data[i].aps[j];
        }
    }
    
    // Calcola preprocessing su tutti gli AP
    Features *dummy_features = malloc(total_aps * sizeof(Features));
    if (!dummy_features) {
        fprintf(stderr, "Errore allocazione memoria per features temporanee\n");
        free(dataset_features);
        free(all_aps);
        return 1;
    }
    
    aps_to_features_set(all_aps, dummy_features, total_aps, &global_preproc);
    
    printf("Parametri preprocessing globali:\n");
    printf("  MAC: %llu - %llu\n", global_preproc.min_mac, global_preproc.max_mac);
    printf("  RSSI: %d - %d\n", global_preproc.min_rssi, global_preproc.max_rssi);
    
    free(dummy_features);
    free(all_aps);

    // Per ogni posizione nel dataset, crea le features usando il preprocessing globale
    for (int i = 0; i < count; i++) {
        Features pos_features[MAX_APS];
        
        // Converti gli AP di questa posizione in features usando i parametri globali
        for (int j = 0; j < MAX_APS; j++) {
            // Calcola MAC come uint64_t
            uint64_t mac = 0;
            for (int k = 0; k < 6; k++) {
                mac |= ((uint64_t)data[i].aps[j].mac[k]) << (8 * k);
            }
            
            // Normalizza usando i parametri globali
            double diff_mac = (double)(global_preproc.max_mac - global_preproc.min_mac);
            double diff_rssi = (double)(global_preproc.max_rssi - global_preproc.min_rssi);
            if (diff_mac == 0) diff_mac = 1;
            if (diff_rssi == 0) diff_rssi = 1;
            
            pos_features[j].x = ((double)(mac - global_preproc.min_mac)) / diff_mac;
            pos_features[j].y = ((double)(data[i].aps[j].rssi - global_preproc.min_rssi)) / diff_rssi;
        }
        
        // Crea una "firma" per questa posizione
        // Usa una combinazione ponderata delle features degli AP
        double signature_x = 0, signature_y = 0;
        double total_weight = 0;
        
        for (int j = 0; j < MAX_APS; j++) {
            double weight = 1.0 + (double)j; // peso crescente per AP successivi
            signature_x += pos_features[j].x * weight;
            signature_y += pos_features[j].y * weight;
            total_weight += weight;
        }
        
        if (total_weight > 0) {
            signature_x /= total_weight;
            signature_y /= total_weight;
        }
        
        dataset_features[i].features.x = signature_x;
        dataset_features[i].features.y = signature_y;
        dataset_features[i].label.x = data[i].x;
        dataset_features[i].label.y = data[i].y;
        
        // Debug: stampa i primi 3 per verificare
        if (i < 3) {
            printf("Punto %d: pos=(%d,%d) features=(%.4f,%.4f)\n", 
                   i, data[i].x, data[i].y, signature_x, signature_y);
        }
    }

    // Calcola la "firma" per i dati rumorosi (query)
    Features noisy_pos_features[MAX_APS];
    for (int j = 0; j < MAX_APS; j++) {
        uint64_t mac = 0;
        for (int k = 0; k < 6; k++) {
            mac |= ((uint64_t)noisy_aps[j].mac[k]) << (8 * k);
        }
        
        double diff_mac = (double)(global_preproc.max_mac - global_preproc.min_mac);
        double diff_rssi = (double)(global_preproc.max_rssi - global_preproc.min_rssi);
        if (diff_mac == 0) diff_mac = 1;
        if (diff_rssi == 0) diff_rssi = 1;
        
        noisy_pos_features[j].x = ((double)(mac - global_preproc.min_mac)) / diff_mac;
        noisy_pos_features[j].y = ((double)(noisy_aps[j].rssi - global_preproc.min_rssi)) / diff_rssi;
    }
    
    double query_signature_x = 0, query_signature_y = 0;
    double total_weight = 0;
    for (int j = 0; j < MAX_APS; j++) {
        double weight = 1.0 + (double)j;
        query_signature_x += noisy_pos_features[j].x * weight;
        query_signature_y += noisy_pos_features[j].y * weight;
        total_weight += weight;
    }
    
    if (total_weight > 0) {
        query_signature_x /= total_weight;
        query_signature_y /= total_weight;
    }
    
    Features query_feature = { query_signature_x, query_signature_y };
    printf("Query features: (%.4f,%.4f)\n", query_signature_x, query_signature_y);

    // Test con diversi valori di k
    int k_values[] = {1, 3, 5, 7};
    int num_k_values = sizeof(k_values) / sizeof(k_values[0]);
    
    printf("\n=== RISULTATI KNN ===\n");
    for (int k_idx = 0; k_idx < num_k_values; k_idx++) {
        int k = k_values[k_idx];
        
        // Crea una copia del dataset per questo test (qsort modifica l'array)
        FeaturesLabel *dataset_copy = malloc(count * sizeof(FeaturesLabel));
        if (!dataset_copy) {
            fprintf(stderr, "Errore allocazione memoria per copia dataset\n");
            continue;
        }
        memcpy(dataset_copy, dataset_features, count * sizeof(FeaturesLabel));
        
        Pos estimated_pos = knn(dataset_copy, count, k, &query_feature);
        
        double error_distance = position_distance(test_point->x, test_point->y, 
                                                 (int)estimated_pos.x, (int)estimated_pos.y);
        
        printf("k=%d: Stimata (%d,%d) - Errore: %.2f unità\n", 
               k, (int)estimated_pos.x, (int)estimated_pos.y, error_distance);
        
        free(dataset_copy);
    }

    // Mostra i 5 vicini più prossimi per analisi
    printf("\n=== ANALISI 5 VICINI PIÙ PROSSIMI ===\n");
    FeaturesLabel *dataset_copy = malloc(count * sizeof(FeaturesLabel));
    if (!dataset_copy) {
        fprintf(stderr, "Errore allocazione memoria per analisi vicini\n");
        free(dataset_features);
        return 1;
    }
    memcpy(dataset_copy, dataset_features, count * sizeof(FeaturesLabel));
    
    // Ordina per distanza manualmente (bubble sort semplice ma efficace per pochi elementi)
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            double dx_i = dataset_copy[i].features.x - query_feature.x;
            double dy_i = dataset_copy[i].features.y - query_feature.y;
            double dist_i = sqrt(dx_i * dx_i + dy_i * dy_i);
            
            double dx_j = dataset_copy[j].features.x - query_feature.x;
            double dy_j = dataset_copy[j].features.y - query_feature.y;
            double dist_j = sqrt(dx_j * dx_j + dy_j * dy_j);
            
            if (dist_j < dist_i) {
                FeaturesLabel temp = dataset_copy[i];
                dataset_copy[i] = dataset_copy[j];
                dataset_copy[j] = temp;
            }
        }
    }
    
    // Mostra solo i primi 5 vicini
    int max_neighbors = (count < 5) ? count : 5;
    for (int i = 0; i < max_neighbors; i++) {
        double dx = dataset_copy[i].features.x - query_feature.x;
        double dy = dataset_copy[i].features.y - query_feature.y;
        double dist = sqrt(dx * dx + dy * dy);
        double pos_dist = position_distance(test_point->x, test_point->y,
                                           dataset_copy[i].label.x, dataset_copy[i].label.y);
        printf("Vicino %d: (%d,%d) - Dist.Feature: %.4f - Dist.Reale: %.2f\n", 
               i+1, dataset_copy[i].label.x, dataset_copy[i].label.y, dist, pos_dist);
    }

    // Dopo aver popolato dataset_features[...] e aver calcolato query_feature
    FILE *fout = fopen("features_dataset.csv", "w");
    if (!fout) {
        perror("Errore apertura features_dataset.csv");
        return 1;
    }
    // Intestazione
    fprintf(fout, "fx,fy,label_x,label_y,type\n");

    // Scrivi i punti di training
    for (int i = 0; i < count; i++) {
        fprintf(fout, "%.6f,%.6f,%d,%d,train\n",
            dataset_features[i].features.x,
            dataset_features[i].features.y,
            dataset_features[i].label.x,
            dataset_features[i].label.y);
    }

    // Scrivi il punto query
    fprintf(fout, "%.6f,%.6f,%d,%d,query\n",
        query_feature.x,
        query_feature.y,
        test_point->x,
        test_point->y);

    fclose(fout);
    printf("Dataset delle feature scritto in features_dataset.csv\n");

    free(dataset_features);
    free(dataset_copy);
    
    return 0;
}