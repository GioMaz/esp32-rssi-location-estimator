#pragma once

#include "network.h"
#include "common.h"
#include <stdint.h>

typedef struct {
    uint64_t min_mac, max_mac;
    int8_t min_rssi, max_rssi;
} PreprocData;

typedef struct {
    double x, y;
} Features;

typedef struct {
    Features features;
    Pos label;
} FeaturesLabel;

void aps_to_features_set(const AccessPoint aps[], Features features_set[], uint64_t count,
                         PreprocData *preproc_data);

Pos knn(FeaturesLabel fl_set[], uint32_t count, uint32_t k, Features *query);
