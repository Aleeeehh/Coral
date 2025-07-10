#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

// Struttura per le statistiche della RAM
typedef struct {
    size_t total_heap;
    size_t free_heap;
    size_t min_free_heap;
    size_t largest_free_block;
    size_t heap_caps_total[MALLOC_CAP_8BIT];
    size_t heap_caps_free[MALLOC_CAP_8BIT];
    size_t heap_caps_largest_free[MALLOC_CAP_8BIT];
} ram_stats_t;

// Struttura per le statistiche delle task
typedef struct {
    char task_name[32];
    UBaseType_t priority;
    uint32_t stack_high_water_mark;
    uint32_t stack_size;
    uint32_t runtime_stats;
    uint8_t core_id;
    uint32_t cpu_percentage;
} task_stats_t;

// Struttura per le statistiche generali del sistema
typedef struct {
    uint32_t uptime_ms;
    uint32_t free_heap_size;
    uint32_t min_free_heap_size;
    uint32_t max_alloc_heap_size;
    uint32_t cpu_freq_mhz;
    uint8_t cpu_cores;
    uint32_t cpu_usage_core0;
    uint32_t cpu_usage_core1;
} system_stats_t;

// Struttura per il monitoraggio dell'inferenza
typedef struct {
    uint32_t inference_start_time;
    uint32_t inference_end_time;
    uint32_t memory_before;
    uint32_t memory_after;
    uint32_t memory_peak;
    uint32_t cpu_usage_during_inference;
    uint32_t task_switches_during_inference;
    bool inference_active;
} inference_monitor_t;

// Struttura per le informazioni della Flash
typedef struct {
    uint32_t flash_size;
    uint32_t flash_speed;
    uint32_t flash_mode;
    uint32_t flash_chip_id;
    uint32_t flash_chip_size;
    uint32_t flash_chip_speed;
} flash_info_t;

// Struttura per le informazioni delle partizioni
typedef struct {
    char label[32];
    uint32_t address;
    uint32_t size;
    uint8_t type;
    uint8_t subtype;
    bool encrypted;
} partition_info_t;

// Inizializzazione del sistema di monitoraggio
esp_err_t monitor_init(void);

// Funzioni per il monitoraggio della RAM
void monitor_get_ram_stats(ram_stats_t* stats);
void monitor_print_ram_stats(void);
void monitor_log_ram_usage(const char* context);

// Funzioni per il monitoraggio delle task
void monitor_get_task_stats(task_stats_t* stats, size_t* num_tasks);
void monitor_print_task_stats(void);
void monitor_print_task_summary(void);

// Funzioni per il monitoraggio del sistema
void monitor_get_system_stats(system_stats_t* stats);
void monitor_print_system_stats(void);

// Funzioni per il monitoraggio dell'inferenza
void monitor_inference_start(void);
void monitor_inference_end(void);
void monitor_inference_get_stats(inference_monitor_t* stats);
void monitor_inference_print_stats(void);

// Funzioni per il monitoraggio continuo
void monitor_start_continuous_monitoring(void);
void monitor_stop_continuous_monitoring(void);

// Funzioni per il monitoraggio dettagliato della memoria
void monitor_memory_region_details(void);
void monitor_heap_caps_details(void);

// Funzioni per il monitoraggio delle performance
void monitor_performance_benchmark(void);
void monitor_print_performance_summary(void);

// Funzioni per il monitoraggio della Flash e partizioni
void monitor_get_flash_info(flash_info_t* info);
void monitor_print_flash_info(void);
void monitor_print_partitions_info(void);
void monitor_print_storage_summary(void);

// Funzioni di utilità
uint32_t monitor_get_free_heap_size(void);
uint32_t monitor_get_min_free_heap_size(void);
uint32_t monitor_get_largest_free_block(void);
uint8_t monitor_get_cpu_usage_percentage(void);

#ifdef __cplusplus
}
#endif

#endif // MONITOR_H 