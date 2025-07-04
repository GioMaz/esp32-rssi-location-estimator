#include "state_training.h"
#include "dataset.h"
#include "gpio.h"
#include "ap_scan.h"

#include <stdio.h>
#include <string.h>

#include "freertos/idf_additions.h"

Pos dir_to_offset[] = {
    [LEFT] = {-1.0, 0.0},
    [RIGHT] = {1.0, 0.0},
    [UP] = {0.0, 1.0},
    [DOWN] = {0.0, -1.0},
};

void handle_training_state(Dataset *dataset, Pos *pos, QueueHandle_t direction_queue, QueueHandle_t scan_queue)
{
    // Check for direction change
    Direction direction;
    if (xQueueReceive(direction_queue, &direction, 0)) {
        pos->x += dir_to_offset[direction].x;
        pos->y += dir_to_offset[direction].y;
        printf("Moved to position (%3.1f, %3.1f).\n", pos->x, pos->y);
    }

    // Check for scan command
    unsigned char signal = 0;
    if (xQueueReceive(scan_queue, &signal, 0) && signal) {
        printf("Scanning position (%3.1f, %3.1f)...\n", pos->x, pos->y);

        int scan_iterations = SCAN_ITERATIONS;
        while (scan_iterations-- && dataset->data_count < DATASET_SIZE) {
            // Create temporary datapoints
            AccessPoint aps[APS_SIZE];

            // Scan datapoints
            uint8_t ap_count = ap_scan(aps);

            // Copy scanned datapoints to dataset
            for (int j = 0; j < ap_count; j++) {
                dataset_insert_ap(dataset, &aps[j], *pos);
            }
        }

        if (dataset->data_count == DATASET_SIZE) {
            printf("ERROR: Max number of datapoints reached\n");
        }
    }
}
