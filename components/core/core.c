#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>

#define MAX_MAC_LEN 18
#define MAX_AP_PER_POINT 4
#define MAX_ROWS 100
#define MAP_WIDTH 10
#define MAP_HEIGHT 10
#define K_NEIGHBORS 3

typedef struct {
    int rssi;
    char mac[MAX_MAC_LEN];
} AccessPoint;

typedef struct {
    AccessPoint aps[MAX_AP_PER_POINT];
    int num_aps;
} Fingerprint;

typedef struct {
    int x;
    int y;
    float distance;
} Neighbor;

// Fingerprint map globale
Fingerprint fingerprint_map[MAP_HEIGHT][MAP_WIDTH];
int map_width = 0;
int map_height = 0;

// Funzione distanza
float calculate_distance(Fingerprint *a, Fingerprint *b) {
    float distance = 0.0f;
    int matches = 0;

    for (int i = 0; i < a->num_aps; i++) {
        for (int j = 0; j < b->num_aps; j++) {
            if (strcmp(a->aps[i].mac, b->aps[j].mac) == 0) {
                float diff = a->aps[i].rssi - b->aps[j].rssi;
                distance += diff * diff;
                matches++;
                break;
            }
        }
    }

    if (matches == 0) return INFINITY;
    return sqrtf(distance);
}

// Trova i K vicini
void find_k_nearest(Fingerprint *input, Neighbor *neighbors) {
    for (int i = 0; i < K_NEIGHBORS; i++) {
        neighbors[i].distance = FLT_MAX;
    }

    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            float dist = calculate_distance(input, &fingerprint_map[y][x]);
            for (int k = 0; k < K_NEIGHBORS; k++) {
                if (dist < neighbors[k].distance) {
                    for (int j = K_NEIGHBORS - 1; j > k; j--) {
                        neighbors[j] = neighbors[j - 1];
                    }
                    neighbors[k].x = x;
                    neighbors[k].y = y;
                    neighbors[k].distance = dist;
                    break;
                }
            }
        }
    }
}

// Stima posizione
void estimate_position(Fingerprint *input, float *est_x, float *est_y) {
    Neighbor neighbors[K_NEIGHBORS];
    find_k_nearest(input, neighbors);

    float sum_x = 0, sum_y = 0, sum_weights = 0;

    for (int i = 0; i < K_NEIGHBORS; i++) {
        if (neighbors[i].distance == FLT_MAX) continue;
        float weight = 1.0f / (neighbors[i].distance + 0.0001f);
        sum_x += neighbors[i].x * weight;
        sum_y += neighbors[i].y * weight;
        sum_weights += weight;
    }

    if (sum_weights > 0) {
        *est_x = sum_x / sum_weights;
        *est_y = sum_y / sum_weights;
    } else {
        *est_x = -1;
        *est_y = -1;
    }
}

// Leggi il CSV
int load_csv(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Errore apertura file");
        return 0;
    }

    char line[512];
    int max_x = 0, max_y = 0;

    while (fgets(line, sizeof(line), fp)) {
        int x, y;
        char macs[MAX_AP_PER_POINT][MAX_MAC_LEN];
        int rssis[MAX_AP_PER_POINT];

        sscanf(line, "%d,%d,%17[^,],%d,%17[^,],%d,%17[^,],%d,%17[^,],%d",
               &x, &y,
               macs[0], &rssis[0],
               macs[1], &rssis[1],
               macs[2], &rssis[2],
               macs[3], &rssis[3]);

        Fingerprint *fp_cell = &fingerprint_map[y][x];
        fp_cell->num_aps = MAX_AP_PER_POINT;
        for (int i = 0; i < MAX_AP_PER_POINT; i++) {
            strncpy(fp_cell->aps[i].mac, macs[i], MAX_MAC_LEN);
            fp_cell->aps[i].rssi = rssis[i];
        }

        if (x + 1 > max_x) max_x = x + 1;
        if (y + 1 > max_y) max_y = y + 1;
    }

    fclose(fp);
    map_width = max_x;
    map_height = max_y;
    return 1;
}

// Test
void run_test() {
    srand(time(NULL));
    int x = rand() % map_width;
    int y = rand() % map_height;

    Fingerprint original = fingerprint_map[y][x];
    Fingerprint noisy_input = original;

    for (int i = 0; i < noisy_input.num_aps; i++) {
        int noise = (rand() % 11) - 5; // da -5 a +5
        noisy_input.aps[i].rssi += noise;
    }

    float est_x, est_y;
    estimate_position(&noisy_input, &est_x, &est_y);

    printf("Posizione reale     : (%d, %d)\n", x, y);
    printf("Posizione stimata   : (%.2f, %.2f)\n", est_x, est_y);
}

int main() {
    if (!load_csv("wifi_map.csv")) {
        return 1;
    }

    run_test();

    return 0;
}
