#include "core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t mac_to_feature(const uint8_t mac[6])
{
    uint64_t result = 0;
    for (int i = 0; i < 6; i++) {
        result |= ((uint64_t)mac[i]) << (8 * i);
    }
    return result;
}

static double euclidean_dist(const Features *f_1, const Features *f_2)
{
    double x = f_2->x - f_1->x;
    double y = f_2->y - f_1->y;
    return sqrt(x * x + y * y);
}

static int cmp(void *query, const void *fl_1, const void *fl_2)
{
    Features *f_1 = &((FeaturesLabel *)fl_1)->features;
    Features *f_2 = &((FeaturesLabel *)fl_2)->features;
    double dist_1 = euclidean_dist((Features *)query, f_1);
    double dist_2 = euclidean_dist((Features *)query, f_2);

    if (dist_1 < dist_2)
        return -1;
    else if (dist_1 > dist_2)
        return 1;
    else
        return 0;
}

static void ap_to_features(const PreprocData *pd, const AccessPoint *ap, Features *features)
{
    uint64_t mac = mac_to_feature(ap->mac);
    int8_t rssi = ap->rssi;

    double diff_mac = (double)(pd->max_mac - pd->min_mac);
    double diff_rssi = (double)(pd->max_rssi - pd->min_rssi);

    if (diff_mac == 0)
        diff_mac = 1;
    if (diff_rssi == 0)
        diff_rssi = 1;

    features->x = ((double)(mac - pd->min_mac)) / diff_mac;
    features->y = ((double)(rssi - pd->min_rssi)) / diff_rssi;
}

void aps_to_features_set(const AccessPoint aps[], Features features_set[], uint64_t count,
                         PreprocData *preproc_data)
{
    uint64_t min_mac = UINT64_MAX;
    uint64_t max_mac = 0;
    int8_t min_rssi = INT8_MAX;
    int8_t max_rssi = INT8_MIN;

    for (uint32_t i = 0; i < count; i++) {
        uint64_t mac = mac_to_feature(aps[i].mac);
        min_mac = MIN(min_mac, mac);
        max_mac = MAX(max_mac, mac);

        int8_t rssi = aps[i].rssi;
        min_rssi = MIN(min_rssi, rssi);
        max_rssi = MAX(max_rssi, rssi);
    }

    preproc_data->min_mac = min_mac;
    preproc_data->max_mac = max_mac;
    preproc_data->min_rssi = min_rssi;
    preproc_data->max_rssi = max_rssi;

    /*
    printf("min_mac: %llu\n", min_mac);
    printf("max_mac: %llu\n", max_mac);
    printf("min_rssi: %d\n", min_rssi);
    printf("max_rssi: %d\n", max_rssi);
    */

    for (uint32_t i = 0; i < count; i++) {
        ap_to_features(preproc_data, &aps[i], &features_set[i]);
        //printf("FEATURES %u: %lf %lf\n", i, features_set[i].x, features_set[i].y);
    }
}

Pos knn(FeaturesLabel fl_set[], uint32_t count, uint32_t k, Features *query)
{
#ifdef __APPLE__ // macOS version
    qsort_r(fl_set, count, sizeof(FeaturesLabel), query, cmp);
#else // Linux / glibc version
    qsort_r(fl_set, count, sizeof(FeaturesLabel), cmp, query);
#endif

    uint64_t n = MIN(k, count);
    double x_mean = 0;
    double y_mean = 0;
    for (uint32_t i = 0; i < n; i++) {
        printf("x: %hd y: %hd\n", fl_set[i].label.x, fl_set[i].label.y);
        x_mean += (double)fl_set[i].label.x;
        y_mean += (double)fl_set[i].label.y;
    }
    x_mean /= (double)n;
    y_mean /= (double)n;
    return (Pos){.x = x_mean, .y = y_mean};
}
