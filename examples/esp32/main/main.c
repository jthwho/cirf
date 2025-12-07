/*
 * CIRF ESP32 Example
 *
 * This example demonstrates using embedded resources on ESP32.
 * It shows both direct symbol access and runtime library functions.
 *
 * Use case: Embedded web server serving static files from flash.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* CIRF headers */
#include "web_resources.h"
#include <cirf/runtime.h>

static const char *TAG = "cirf_example";

/*
 * Callback for iterating over all files
 */
static void print_file_info(const cirf_file_t *file, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "  %s (%s, %u bytes)", file->path, file->mime, (unsigned)file->size);
}

/*
 * Simulate serving a file (like a web server would)
 */
static void serve_file(const char *path)
{
    ESP_LOGI(TAG, "Request for: %s", path);

    /* Look up the file using runtime library */
    const cirf_file_t *file = cirf_find_file(&web_resources_root, path);

    if (file) {
        ESP_LOGI(TAG, "Found: %s (%s, %u bytes)", file->name, file->mime, (unsigned)file->size);

        /* In a real web server, you would send the data over HTTP */
        /* For this example, we'll just print the first 100 bytes */
        size_t preview_len = file->size < 100 ? file->size : 100;
        ESP_LOGI(TAG, "Content preview:\n%.*s%s",
                 (int)preview_len, (const char *)file->data,
                 file->size > 100 ? "..." : "");
    } else {
        ESP_LOGW(TAG, "File not found: %s", path);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "CIRF ESP32 Example");
    ESP_LOGI(TAG, "=================================");

    /* ========================================
     * Direct symbol access (no runtime needed)
     * ======================================== */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Direct symbol access:");
    ESP_LOGI(TAG, "-----------------------");

    /* Access root folder info */
    ESP_LOGI(TAG, "Root contains %u files, %u folders",
             (unsigned)web_resources_root.file_count,
             (unsigned)web_resources_root.child_count);

    /* Access specific file directly by symbol */
    ESP_LOGI(TAG, "index.html size: %u bytes", (unsigned)web_resources_file_index_html->size);
    ESP_LOGI(TAG, "index.html mime: %s", web_resources_file_index_html->mime);

    /* Access file in subdirectory */
    ESP_LOGI(TAG, "api/config.json path: %s", web_resources_file_api_config_json->path);

    /* ========================================
     * Runtime library functions
     * ======================================== */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Runtime library functions:");
    ESP_LOGI(TAG, "---------------------------");

    /* Get metadata */
    const char *version = cirf_get_metadata(
        web_resources_root.metadata,
        web_resources_root.metadata_count,
        "version"
    );
    ESP_LOGI(TAG, "Resource version: %s", version ? version : "unknown");

    /* List all embedded files */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "All embedded files:");
    cirf_foreach_file_recursive(&web_resources_root, print_file_info, NULL);

    /* Count total files */
    size_t total_files = cirf_count_files(&web_resources_root);
    size_t total_folders = cirf_count_folders(&web_resources_root);
    ESP_LOGI(TAG, "Total: %u files in %u folders", (unsigned)total_files, (unsigned)total_folders);

    /* ========================================
     * Simulated web server requests
     * ======================================== */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Simulated web requests:");
    ESP_LOGI(TAG, "------------------------");

    serve_file("index.html");
    serve_file("api/config.json");
    serve_file("css/style.css");
    serve_file("nonexistent.html");  /* Should show "not found" */

    /* ========================================
     * Memory usage info
     * ======================================== */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Memory info:");
    ESP_LOGI(TAG, "-------------");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Example complete!");

    /* Keep the task alive */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
