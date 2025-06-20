#pragma once

#include "dataset.h"
#include "freertos/idf_additions.h"
#include "utils.h"

void handle_training_state(Dataset *dataset, Pos *pos, QueueHandle_t direction_queue);
