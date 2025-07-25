#include "monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_rom_sys.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "MONITOR";

// Variabili globali per il monitoraggio
static TaskHandle_t monitor_task_handle = NULL;
static bool continuous_monitoring_active = false;
static inference_monitor_t g_inference_monitor = {
    .inference_start_time = 0,
    .inference_end_time = 0,
    .memory_before = 0,
    .memory_after = 0,
    .memory_peak = 0,
    .cpu_usage_during_inference = 0,
    .task_switches_during_inference = 0,
    .inference_active = false
};
static uint32_t g_min_free_heap = UINT32_MAX;
static uint32_t g_max_alloc_heap = 0;

// Task per il monitoraggio continuo
static void monitor_task(void* pvParameters) {
    printf(TAG, "Task di monitoraggio avviato");
    
    while (continuous_monitoring_active) {
        // Monitora la memoria
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap < g_min_free_heap) {
            g_min_free_heap = free_heap;
        }
        
        // Monitora l'utilizzo massimo
        uint32_t total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        uint32_t used_heap = total_heap - free_heap;
        if (used_heap > g_max_alloc_heap) {
            g_max_alloc_heap = used_heap;
        }
        
        // Log ogni 5 secondi
        static uint32_t last_log_time = 0;
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time - last_log_time >= 5000) {
            printf(TAG, "Monitoraggio: Heap libero=%u, Min=%u, Max allocato=%u", 
                     free_heap, g_min_free_heap, g_max_alloc_heap);
            last_log_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Controlla ogni secondo
    }
    
    printf(TAG, "Task di monitoraggio terminato");
    vTaskDelete(NULL);
}

esp_err_t monitor_init(void) {
    // Inizializza le statistiche globali
    g_min_free_heap = esp_get_free_heap_size();
    g_max_alloc_heap = 0;
    memset(&g_inference_monitor, 0, sizeof(g_inference_monitor));
    ESP_LOGI(TAG, "Sistema di monitoraggio inizializzato");
    return ESP_OK;
}

void monitor_get_ram_stats(ram_stats_t* stats) {
    if (!stats) return;
    
    stats->total_heap = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    stats->free_heap = esp_get_free_heap_size();
    stats->min_free_heap = g_min_free_heap;
    stats->largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    // Statistiche per diverse capacità di memoria
    stats->heap_caps_total[MALLOC_CAP_8BIT] = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    stats->heap_caps_free[MALLOC_CAP_8BIT] = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    stats->heap_caps_largest_free[MALLOC_CAP_8BIT] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    stats->heap_caps_total[MALLOC_CAP_32BIT] = heap_caps_get_total_size(MALLOC_CAP_32BIT);
    stats->heap_caps_free[MALLOC_CAP_32BIT] = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    stats->heap_caps_largest_free[MALLOC_CAP_32BIT] = heap_caps_get_largest_free_block(MALLOC_CAP_32BIT);
    
    stats->heap_caps_total[MALLOC_CAP_INTERNAL] = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    stats->heap_caps_free[MALLOC_CAP_INTERNAL] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    stats->heap_caps_largest_free[MALLOC_CAP_INTERNAL] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    
   
}

void monitor_print_ram_stats(void) {
    ram_stats_t stats;
    monitor_get_ram_stats(&stats);
    
    printf("\n=== STATISTICHE RAM TOTALE ===\n");
    printf("Heap totale: %zu bytes (%.1f MB)\n", stats.total_heap, (float)stats.total_heap / 1024 / 1024);
    printf("Heap libero: %zu bytes (%.1f MB)\n", stats.free_heap, (float)stats.free_heap / 1024 / 1024);
    printf("Heap minimo libero: %zu bytes (%.1f MB)\n", stats.min_free_heap, (float)stats.min_free_heap / 1024 / 1024);
    printf("Blocco libero più grande: %zu bytes (%.1f MB)\n", stats.largest_free_block, (float)stats.largest_free_block / 1024 / 1024);
    printf("Utilizzo: %.1f%%\n", (float)(stats.total_heap - stats.free_heap) / stats.total_heap * 100);
    
    printf("\n--- DETTAGLI PER TIPO DI MEMORIA ---\n");
    
    // PSRAM (32-bit/8-bit)
    size_t psram_total = stats.heap_caps_total[MALLOC_CAP_32BIT] - stats.heap_caps_total[MALLOC_CAP_INTERNAL];
    size_t psram_free = stats.heap_caps_free[MALLOC_CAP_32BIT] - stats.heap_caps_free[MALLOC_CAP_INTERNAL];
    if (psram_total > 0) {
        printf("🟦 PSRAM (Memoria Esterna):\n");
        printf("   Totale: %zu bytes (%.1f MB)\n", psram_total, (float)psram_total / 1024 / 1024);
        printf("   Libero: %zu bytes (%.1f MB)\n", psram_free, (float)psram_free / 1024 / 1024);
        printf("   Utilizzo: %.1f%%\n", (float)(psram_total - psram_free) / psram_total * 100);
        printf("   Blocco max: %zu bytes (%.1f MB)\n", 
               stats.heap_caps_largest_free[MALLOC_CAP_32BIT], 
               (float)stats.heap_caps_largest_free[MALLOC_CAP_32BIT] / 1024 / 1024);
    }
    
    // SRAM (Internal)
    printf("🟨 SRAM (Memoria Interna):\n");
    printf("   Totale: %zu bytes (%.1f KB)\n", stats.heap_caps_total[MALLOC_CAP_INTERNAL], (float)stats.heap_caps_total[MALLOC_CAP_INTERNAL] / 1024);
    printf("   Libero: %zu bytes (%.1f KB)\n", stats.heap_caps_free[MALLOC_CAP_INTERNAL], (float)stats.heap_caps_free[MALLOC_CAP_INTERNAL] / 1024);
    printf("   Utilizzo: %.1f%%\n", (float)(stats.heap_caps_total[MALLOC_CAP_INTERNAL] - stats.heap_caps_free[MALLOC_CAP_INTERNAL]) / stats.heap_caps_total[MALLOC_CAP_INTERNAL] * 100);
    printf("   Blocco max: %zu bytes (%.1f KB)\n", 
           stats.heap_caps_largest_free[MALLOC_CAP_INTERNAL], 
           (float)stats.heap_caps_largest_free[MALLOC_CAP_INTERNAL] / 1024);
    
    // RTC SRAM (Memoria Ultra-Veloce)
    size_t total_rtc = heap_caps_get_total_size(MALLOC_CAP_RTCRAM);
    size_t free_rtc = heap_caps_get_free_size(MALLOC_CAP_RTCRAM);
    if (total_rtc > 0) {
        printf("🟪 RTC SRAM (Memoria Ultra-Veloce):\n");
        printf("   Totale: %zu bytes (%.1f KB)\n", total_rtc, (float)total_rtc / 1024);
        printf("   Libero: %zu bytes (%.1f KB)\n", free_rtc, (float)free_rtc / 1024);
        printf("   Utilizzo: %.1f%%\n", (float)(total_rtc - free_rtc) / total_rtc * 100);
        printf("   Blocco max: %zu bytes (%.1f KB)\n", 
               heap_caps_get_largest_free_block(MALLOC_CAP_RTCRAM),
               (float)heap_caps_get_largest_free_block(MALLOC_CAP_RTCRAM) / 1024);
    } else {
        printf("🟪 RTC SRAM: Non disponibile\n");
    }
    
    printf("================================\n\n");
}

void monitor_log_ram_usage(const char* context) {
    ram_stats_t stats;
    monitor_get_ram_stats(&stats);
    
    printf("[%s] RAM: Libero=%zu, Min=%zu, Utilizzo=%.1f%%", 
              context, stats.free_heap, stats.min_free_heap, 
              (float)(stats.total_heap - stats.free_heap) / stats.total_heap * 100);
}

void monitor_get_task_stats(task_stats_t* stats, size_t* num_tasks) {
    if (!stats || !num_tasks) return;
    
    *num_tasks = uxTaskGetNumberOfTasks();
    if (*num_tasks > 20) *num_tasks = 20; // Limita a 20 task per sicurezza
    
    TaskStatus_t* task_status_array = (TaskStatus_t*)malloc(*num_tasks * sizeof(TaskStatus_t));
    if (!task_status_array) return;
    
    uint32_t total_runtime;
    *num_tasks = uxTaskGetSystemState(task_status_array, *num_tasks, &total_runtime);
    
    for (size_t i = 0; i < *num_tasks; i++) {
        strncpy(stats[i].task_name, task_status_array[i].pcTaskName, 31);
        stats[i].task_name[31] = '\0';
        stats[i].priority = task_status_array[i].uxCurrentPriority;
        stats[i].stack_high_water_mark = task_status_array[i].usStackHighWaterMark;
                // Calcola dimensione stack basata su high water mark
        stats[i].stack_size = task_status_array[i].usStackHighWaterMark;
        stats[i].runtime_stats = task_status_array[i].ulRunTimeCounter;
        stats[i].core_id = task_status_array[i].xCoreID;
        stats[i].cpu_percentage = total_runtime > 0 ? 
                                 (task_status_array[i].ulRunTimeCounter * 100) / total_runtime : 0;
    }
    
    free(task_status_array);
}

void monitor_print_task_stats(void) {
    task_stats_t stats[20];
    size_t num_tasks;
    monitor_get_task_stats(stats, &num_tasks);
    
    printf("\n=== STATISTICHE TASK ===\n");
    printf("%-20s %-8s %-8s %-8s %-8s %-8s %-8s\n", 
           "Nome", "Core", "Priorità", "Stack", "HWM", "CPU%", "Runtime");
    printf("------------------------------------------------------------\n");
    
    for (size_t i = 0; i < num_tasks; i++) {
        printf("%-20s %-8d %-8d %-8lu %-8lu %-8lu %-8lu\n",
               stats[i].task_name,
               stats[i].core_id,
               stats[i].priority,
               stats[i].stack_size,
               stats[i].stack_high_water_mark,
               stats[i].cpu_percentage,
               stats[i].runtime_stats);
    }
    printf("================================\n\n");
}

void monitor_print_task_summary(void) {
    task_stats_t stats[20];
    size_t num_tasks;
    monitor_get_task_stats(stats, &num_tasks);
    
    uint32_t total_cpu_core0 = 0, total_cpu_core1 = 0;
    uint32_t tasks_core0 = 0, tasks_core1 = 0;
    
    for (size_t i = 0; i < num_tasks; i++) {
        if (stats[i].core_id == 0) {
            total_cpu_core0 += stats[i].cpu_percentage;
            tasks_core0++;
        } else if (stats[i].core_id == 1) {
            total_cpu_core1 += stats[i].cpu_percentage;
            tasks_core1++;
        }
    }
    
    printf("\n=== RIEPILOGO TASK ===\n");
    printf("Core 0: %lu task, CPU totale: %lu%%\n", tasks_core0, total_cpu_core0);
    printf("Core 1: %lu task, CPU totale: %lu%%\n", tasks_core1, total_cpu_core1);
    printf("Task totali: %zu\n", num_tasks);
    printf("=====================\n\n");
}

void monitor_get_system_stats(system_stats_t* stats) {
    if (!stats) return;
    
    stats->uptime_ms = esp_timer_get_time() / 1000;
    stats->free_heap_size = esp_get_free_heap_size();
    stats->min_free_heap_size = g_min_free_heap;
    stats->max_alloc_heap_size = g_max_alloc_heap;
    stats->cpu_freq_mhz = esp_rom_get_cpu_ticks_per_us() * 1000000 / 1000000;
    stats->cpu_cores = 2; // ESP32 ha 2 core
    
    // Calcola utilizzo CPU per core
    task_stats_t task_stats[20];
    size_t num_tasks;
    monitor_get_task_stats(task_stats, &num_tasks);
    
    stats->cpu_usage_core0 = 0;
    stats->cpu_usage_core1 = 0;
    
    for (size_t i = 0; i < num_tasks; i++) {
        if (task_stats[i].core_id == 0) {
            stats->cpu_usage_core0 += task_stats[i].cpu_percentage;
        } else if (task_stats[i].core_id == 1) {
            stats->cpu_usage_core1 += task_stats[i].cpu_percentage;
        }
    }
}

void monitor_print_system_stats(void) {
    system_stats_t stats;
    monitor_get_system_stats(&stats);
    
    printf("\n=== STATISTICHE SISTEMA ===\n");
    printf("Uptime: %lu ms\n", stats.uptime_ms);
    printf("Heap libero: %lu bytes\n", stats.free_heap_size);
    printf("Heap minimo: %lu bytes\n", stats.min_free_heap_size);
    printf("Heap massimo allocato: %lu bytes\n", stats.max_alloc_heap_size);
    printf("Freq CPU: %lu MHz\n", stats.cpu_freq_mhz);
    printf("CPU Core 0: %lu%%\n", stats.cpu_usage_core0);
    printf("CPU Core 1: %lu%%\n", stats.cpu_usage_core1);
    printf("==========================\n\n");
}

void monitor_inference_start(void) {
    g_inference_monitor.inference_start_time = esp_timer_get_time() / 1000;
    g_inference_monitor.memory_before = esp_get_free_heap_size();
    g_inference_monitor.memory_peak = g_inference_monitor.memory_before;
    g_inference_monitor.inference_active = true;
    g_inference_monitor.task_switches_during_inference = 0;
    
    printf("Monitoraggio inferenza iniziato - Memoria: %lu bytes", g_inference_monitor.memory_before);
}

void monitor_inference_end(void) {
    if (!g_inference_monitor.inference_active) return;
    
    g_inference_monitor.inference_end_time = esp_timer_get_time() / 1000;
    g_inference_monitor.memory_after = esp_get_free_heap_size();
    g_inference_monitor.inference_active = false;
    
    uint32_t duration = g_inference_monitor.inference_end_time - g_inference_monitor.inference_start_time;
    int32_t memory_diff = g_inference_monitor.memory_before - g_inference_monitor.memory_after;
    
    printf("Monitoraggio inferenza completato:\n");
    printf("Durata: %lu ms\n", duration);
    printf("Memoria prima: %lu bytes\n", g_inference_monitor.memory_before);
    printf("Memoria dopo: %lu bytes\n", g_inference_monitor.memory_after);
    printf("Differenza memoria: %ld bytes\n", memory_diff);
    printf("Memoria di picco: %lu bytes\n", g_inference_monitor.memory_peak);
}

void monitor_inference_get_stats(inference_monitor_t* stats) {
    if (!stats) return;
    memcpy(stats, &g_inference_monitor, sizeof(inference_monitor_t));
}

void monitor_inference_print_stats(void) {
    if (!g_inference_monitor.inference_active && g_inference_monitor.inference_end_time == 0) {
        printf("Nessuna inferenza monitorata\n");
        return;
    }
    
    printf("\n=== STATISTICHE INFERENZA ===\n");
    if (g_inference_monitor.inference_active) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        uint32_t elapsed = current_time - g_inference_monitor.inference_start_time;
                printf("Inferenza in corso: %lu ms\n", elapsed);
    } else {
        uint32_t duration = g_inference_monitor.inference_end_time - g_inference_monitor.inference_start_time;
        printf("Durata inferenza: %lu ms\n", duration);
    }
    
    printf("Memoria prima: %lu bytes\n", g_inference_monitor.memory_before);
    printf("Memoria dopo: %lu bytes\n", g_inference_monitor.memory_after);
    printf("Memoria di picco: %lu bytes\n", g_inference_monitor.memory_peak);
    printf("Differenza memoria: %ld bytes\n",
           (long)(g_inference_monitor.memory_before - g_inference_monitor.memory_after));
    printf("Task switches: %lu\n", g_inference_monitor.task_switches_during_inference);
    printf("============================\n\n");
}

void monitor_start_continuous_monitoring(void) {
    if (continuous_monitoring_active) {
        ESP_LOGW(TAG, "Monitoraggio continuo già attivo");
        return;
    }
    
    continuous_monitoring_active = true;
    xTaskCreatePinnedToCore(monitor_task, "monitor_task", 4096, NULL, 1, &monitor_task_handle, 1);
    printf("Monitoraggio continuo avviato\n");
}

void monitor_stop_continuous_monitoring(void) {
    if (!continuous_monitoring_active) {
        ESP_LOGW(TAG, "Monitoraggio continuo non attivo");
        return;
    }
    
    continuous_monitoring_active = false;
    if (monitor_task_handle) {
        vTaskDelete(monitor_task_handle);
        monitor_task_handle = NULL;
    }
    printf("Monitoraggio continuo fermato\n");
}

void monitor_memory_region_details(void) {
    printf("\n=== DETTAGLI REGIONI MEMORIA ===\n");
    
    // Informazioni sui blocchi di memoria
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_8BIT);
    
    printf("Blocchi totali: %u\n", info.total_blocks);
    printf("Blocchi liberi: %u\n", info.free_blocks);
    printf("Bytes allocati: %u\n", info.total_allocated_bytes);
    printf("Bytes minimi liberi: %u\n", info.minimum_free_bytes);
    printf("Blocco libero più grande: %u\n", info.largest_free_block);
    
   
    
    
    // 8-bit e 32-bit (per completezza)
    printf("\n--- CAPACITÀ COMBINATE ---\n");
    size_t total_8bit = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    size_t free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    printf("8-bit (SRAM+PSRAM): %zu/%zu bytes (%.1f%% libero)\n", free_8bit, total_8bit, 
           (float)free_8bit/total_8bit*100);
    
    size_t total_32bit = heap_caps_get_total_size(MALLOC_CAP_32BIT);
    size_t free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    printf("32-bit (SRAM+PSRAM): %zu/%zu bytes (%.1f%% libero)\n", free_32bit, total_32bit, 
           (float)free_32bit/total_32bit*100);
    
    printf("================================\n\n");
}

void monitor_heap_caps_details(void) {
    printf("\n=== DETTAGLI HEAP CAPS ===\n");
    
    // Mostra tutti i tipi di memoria disponibili
    printf("Tipi di memoria disponibili:\n");
    
    size_t total, free, largest;
    
    total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    printf("MALLOC_CAP_8BIT: Totale=%zu, Libero=%zu, Max=%zu\n", total, free, largest);
    
    total = heap_caps_get_total_size(MALLOC_CAP_32BIT);
    free = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    largest = heap_caps_get_largest_free_block(MALLOC_CAP_32BIT);
    printf("MALLOC_CAP_32BIT: Totale=%zu, Libero=%zu, Max=%zu\n", total, free, largest);
    
    total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    printf("MALLOC_CAP_INTERNAL: Totale=%zu, Libero=%zu, Max=%zu\n", total, free, largest);
    
    
    total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    printf("MALLOC_CAP_SPIRAM: Totale=%zu, Libero=%zu, Max=%zu\n", total, free, largest);
    
    printf("=============================\n\n");
}

void monitor_performance_benchmark(void) {
    printf("\n=== BENCHMARK PERFORMANCE ===\n");
    
    // Benchmark allocazione memoria
    uint32_t start_time = esp_timer_get_time();
    void* ptr = malloc(1024);
    uint32_t alloc_time = esp_timer_get_time() - start_time;
    
    if (ptr) {
        printf("Tempo allocazione 1KB: %lu us\n", alloc_time);
        free(ptr);
    }
    
    // Benchmark CPU
    start_time = esp_timer_get_time();
    volatile int dummy = 0;
    for (int i = 0; i < 1000000; i++) {
        dummy += i;
    }
    uint32_t cpu_time = esp_timer_get_time() - start_time;
    printf("Tempo loop 1M iterazioni: %lu us\n", cpu_time);
    
    // Benchmark task switching
    start_time = esp_timer_get_time();
    for (int i = 0; i < 1000; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    uint32_t task_time = esp_timer_get_time() - start_time;
    printf("Tempo 1000 task switches: %lu us\n", task_time);
    
    printf("============================\n\n");
}

void monitor_print_performance_summary(void) {
    printf("\n=== RIEPILOGO PERFORMANCE ===\n");
    
    // Statistiche generali
    system_stats_t sys_stats;
    monitor_get_system_stats(&sys_stats);
    
    printf("Uptime: %lu ms\n", sys_stats.uptime_ms);
    printf("Utilizzo memoria: %.1f%%\n", 
           (float)(sys_stats.max_alloc_heap_size) / (sys_stats.free_heap_size + sys_stats.max_alloc_heap_size) * 100);
    printf("CPU Core 0: %lu%%\n", sys_stats.cpu_usage_core0);
    printf("CPU Core 1: %lu%%\n", sys_stats.cpu_usage_core1);
    
    // Statistiche task
    task_stats_t task_stats[20];
    size_t num_tasks;
    monitor_get_task_stats(task_stats, &num_tasks);
    
    printf("Task attivi: %zu\n", num_tasks);
    
    // Trova il task con più utilizzo CPU
    uint32_t max_cpu = 0;
    char max_task_name[32] = "";
    for (size_t i = 0; i < num_tasks; i++) {
        if (task_stats[i].cpu_percentage > max_cpu) {
            max_cpu = task_stats[i].cpu_percentage;
            strncpy(max_task_name, task_stats[i].task_name, 31);
        }
    }
    
    if (max_cpu > 0) {
        printf("Task più CPU intensivo: %s (%lu%%)\n", max_task_name, max_cpu);
    }
    
    printf("===========================\n\n");
}

// Funzioni di utilità
uint32_t monitor_get_free_heap_size(void) {
    return esp_get_free_heap_size();
}

uint32_t monitor_get_min_free_heap_size(void) {
    return g_min_free_heap;
}

uint32_t monitor_get_largest_free_block(void) {
    return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

uint8_t monitor_get_cpu_usage_percentage(void) {
    system_stats_t stats;
    monitor_get_system_stats(&stats);
    return (stats.cpu_usage_core0 + stats.cpu_usage_core1) / 2;
}

// ===== FUNZIONI PER MONITORAGGIO FLASH E PARTIZIONI =====

void monitor_get_flash_info(flash_info_t* info) {
    if (!info) return;
    
    // Ottieni informazioni sulla flash usando le API corrette
    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size); // NULL = flash principale
    
    info->flash_size = flash_size;
    info->flash_mode = 0; // Non disponibile in esp_flash.h
    info->flash_chip_id = 0; // Non disponibile in esp_flash.h
    info->flash_chip_size = flash_size;
}

void monitor_print_flash_info(void) {
    flash_info_t info;
    monitor_get_flash_info(&info);
    
    printf("\n=== 🟫 INFORMAZIONI FLASH 🟫 ===\n");
    printf("Dimensione Flash: %lu bytes (%.1f MB)\n", info.flash_size, (float)info.flash_size / 1024 / 1024);
    printf("Velocità Flash: %lu MHz\n", info.flash_speed / 1000000);
    printf("Modalità Flash: Non disponibile\n");
    printf("Chip ID: Non disponibile\n");
    printf("==========================\n\n");
}

void monitor_print_partitions_info(void) {
    printf("\n=== ⬜️ INFORMAZIONI PARTIZIONI ⬜️ ===\n");
    
    // Ottieni la tabella delle partizioni
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    
    if (!it) {
        printf("Errore: Impossibile ottenere informazioni sulle partizioni\n");
        return;
    }
    
    printf("%-20s %-12s %-12s %-8s %-8s %-10s\n", 
           "Label", "Address", "Size", "Type", "Subtype", "Encrypted");
    printf("------------------------------------------------------------\n");
    
    const esp_partition_t* part;
    while ((part = esp_partition_get(it)) != NULL) {
        printf("%-20s 0x%08lx %-12lu %-8d %-8d %-10s\n",
               part->label,
               part->address,
               part->size,
               part->type,
               part->subtype,
               part->encrypted ? "Sì" : "No");
        it = esp_partition_next(it);
        if (it == NULL) break; // Esci se non ci sono più partizioni
    }
    
    esp_partition_iterator_release(it);
    printf("====================================\n\n");
}

void monitor_print_storage_summary(void) {
    printf("\n=== 💾 RIEPILOGO STORAGE DETTAGLIATO 💾 ===\n");
    
    // Informazioni Flash
    flash_info_t flash_info;
    monitor_get_flash_info(&flash_info);
    
    printf("🔧 Flash Hardware:\n");
    printf("   Dimensione totale: %lu bytes (%.1f MB)\n", flash_info.flash_size, (float)flash_info.flash_size / 1024 / 1024);
    printf("   Velocità: %lu MHz\n", flash_info.flash_speed / 1000000);
    printf("   Modalità: Non disponibile\n");
    
    // Informazioni Partizioni con dettagli di utilizzo
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (it) {
        uint32_t total_used_size = 0;
        
        // Calcola spazio effettivamente utilizzato
        const esp_partition_t* part;
        while ((part = esp_partition_get(it)) != NULL) {
            uint32_t part_used = 0;
            
            if (part->type == ESP_PARTITION_TYPE_APP) {
                // Per partizioni APP, ottieni la dimensione effettiva del firmware
                const esp_partition_t* running_part = esp_ota_get_running_partition();
                if (running_part && strcmp(part->label, running_part->label) == 0) {
                    // Per ora, usa la dimensione della partizione (sarà più accurata)
                    part_used = part->size;
                }
            } else if (part->type == ESP_PARTITION_TYPE_DATA) {
                // Per partizioni DATA, ottieni l'uso effettivo
                if (strstr(part->label, "nvs") != NULL) {
                    nvs_stats_t nvs_stats;
                    if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK) {
                        // Calcola spazio effettivamente utilizzato in NVS
                        part_used = nvs_stats.used_entries * nvs_stats.total_entries;
                    }
                } else {
                    // Per altre partizioni data, usa la dimensione completa
                    part_used = part->size;
                }
            } else {
                // Per altre partizioni (bootloader, partition table), usa la dimensione completa
                part_used = part->size;
            }
            
            total_used_size += part_used;
            it = esp_partition_next(it);
            if (it == NULL) break;
        }
        
        esp_partition_iterator_release(it);
        
        // Riepilogo generale
        printf("\n📊 Utilizzo Flash:\n");
        printf("   Spazio totale: %lu bytes (%.1f MB)\n", flash_info.flash_size, (float)flash_info.flash_size / 1024 / 1024);
        printf("   Spazio utilizzato: %lu bytes (%.1f MB)\n", total_used_size, (float)total_used_size / 1024 / 1024);
        printf("   Spazio libero: %lu bytes (%.1f MB)\n", 
               flash_info.flash_size - total_used_size, (float)(flash_info.flash_size - total_used_size) / 1024 / 1024);
        printf("   Percentuale utilizzata: %.1f%%\n", (float)total_used_size / flash_info.flash_size * 100);
        

    }
    
    printf("\n=====================================\n\n");
} 