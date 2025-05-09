#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t get_position_handler(httpd_req_t *req);
esp_err_t static_file_handler(httpd_req_t *req);
