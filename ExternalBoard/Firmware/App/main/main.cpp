#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

#include "usb/usb_host.h"
#include "usb/hid_host.h"

#include "driver/uart.h"
#include "uart_cmd.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "usb_joy_uart";

// ================= UART =================
#define UART_PORT       UART_NUM_1
#define UART_TX_PIN     17
#define UART_RX_PIN     18
#define UART_BAUD_RATE  115200
#define UART_BUF_SIZE   1024

EB_UART_CMD uart_cmd;

static void uart_init_app(void)
{
    const uart_config_t cfg = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(
        UART_PORT,
        UART_TX_PIN,
        UART_RX_PIN,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));
}

uint8_t prev_report[128] = {0};
uint16_t prev_report_len = 0;

static void uart_send_report(const uint8_t *data, size_t len)
{
    if(len == prev_report_len && memcmp(prev_report, data, MIN(len, sizeof(prev_report))) == 0) 
    {
        return;
    }

    uart_cmd.send_cmd(EB_UART_CMD::CMD_INPUT, data, len);
    
    memcpy(prev_report, data, MIN(len, sizeof(prev_report)));
    prev_report_len = len;
}

// ================= APP QUEUE =================
typedef enum {
    APP_EVENT_HID_HOST = 0,
} app_event_group_t;

typedef struct {
    app_event_group_t event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

static QueueHandle_t app_event_queue = NULL;

// ================= USB HOST TASK =================
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Сообщаем app_main, что USB host уже поднят
    xTaskNotifyGive((TaskHandle_t)arg);

    while (1) {
        uint32_t event_flags = 0;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "usb_host_lib_handle_events failed: %s", esp_err_to_name(err));
            continue;
        }

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "No more clients");
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }

    ESP_LOGI(TAG, "USB host uninstall");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

// ================= HID CALLBACKS =================
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                        const hid_host_interface_event_t event,
                                        void *arg)
{
    uint8_t data[64] = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;

    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
            ESP_ERROR_CHECK(
                hid_host_device_get_raw_input_report_data(
                    hid_device_handle,
                    data,
                    sizeof(data),
                    &data_length
                )
            );

            // Отправляем HID report в UART1
            uart_send_report(data, data_length);
            break;

        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG,
                     "HID disconnected, proto=%d subclass=%d",
                     dev_params.proto,
                     dev_params.sub_class);
            
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            uart_cmd.send_cmd(EB_UART_CMD::CMD_INPUT_DISCONNECTED);
            break;

        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGW(TAG,
                     "HID transfer error, proto=%d subclass=%d",
                     dev_params.proto,
                     dev_params.sub_class);
            uart_cmd.send_cmd(EB_UART_CMD::CMD_INPUT_ERROR);
            break;

        default:
            ESP_LOGW(TAG, "Unhandled HID interface event: %d", event);
            break;
    }
}

static void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                                  const hid_host_driver_event_t event,
                                  void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED: {
            ESP_LOGI(TAG,
                     "HID connected, proto=%d subclass=%d",
                     dev_params.proto,
                     dev_params.sub_class);

            const hid_host_device_config_t dev_config = {
                .callback = hid_host_interface_callback,
                .callback_arg = NULL,
            };

            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));

            // Для boot-устройств можно переключить в BOOT protocol.
            // Для generic joystick чаще это не требуется, но не мешает только для boot subclass.
            if (dev_params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
                ESP_ERROR_CHECK(
                    hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT)
                );
            }

            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            uart_cmd.send_cmd(EB_UART_CMD::CMD_INPUT_CONNECTED);
            break;
        }

        default:
            ESP_LOGW(TAG, "Unhandled HID driver event: %d", event);
            break;
    }
}

static void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                                     const hid_host_driver_event_t event,
                                     void *arg)
{
    const app_event_queue_t evt = {
        .event_group = APP_EVENT_HID_HOST,
        .hid_host_device = {
            .handle = hid_device_handle,
            .event = event,
            .arg = arg,
        },
    };

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt, 0);
    }
}

// ================= MAIN =================
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Start USB HID -> UART");

    uart_init_app();
	
	uart_send_report((uint8_t*)"Hello!", 5);

    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create app_event_queue");
        return;
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        usb_lib_task,
        "usb_events",
        4096,
        xTaskGetCurrentTaskHandle(),
        2,
        NULL,
        0
    );
    assert(task_created == pdTRUE);

    // Ждём, пока USB host поднимется
    ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(2000));

    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL,
    };

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    ESP_LOGI(TAG, "Waiting for HID device...");

    while (1) {
        app_event_queue_t evt;
        if (xQueueReceive(app_event_queue, &evt, portMAX_DELAY)) {
            if (evt.event_group == APP_EVENT_HID_HOST) {
                hid_host_device_event(
                    evt.hid_host_device.handle,
                    evt.hid_host_device.event,
                    evt.hid_host_device.arg
                );
            }
        }
    }
}