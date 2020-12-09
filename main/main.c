#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// USER INCLUDES
#include "driver/gpio.h"
#include <driver/i2c.h>
#include <esp_err.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <netdb.h>
#include <sys/socket.h>

#include <cJSON.h>

#include <ssd1306/ssd1306.h>


//USER DEFINES
#define BUTTON GPIO_Pin_14
#define WEB_SERVER "worldtimeapi.org"
#define WEB_PORT 80
#define WEB_URL "http://worldtimeapi.org/api/ip"

#define SCL_PIN 5
#define SDA_PIN 4
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

uint8_t buff[DISPLAY_HEIGHT*DISPLAY_WIDTH/8];

// int i2c_master_port = I2C_NUM_0;

static const char *TAG = "time";

static const char *REQUEST = "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

ssd1306_t dev = 
    {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = SSD1306_I2C_ADDR_0,
        .screen = SSD1306_SCREEN, // or SH1106_SCREEN
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT
    };

esp_err_t loadPins(void)
{
	gpio_config_t io_config;
	io_config.intr_type = GPIO_INTR_DISABLE;
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pin_bit_mask = ((1ULL << GPIO_NUM_14));
    io_config.pull_up_en = 1;
    io_config.pull_down_en = 0;
    return gpio_config(&io_config);
}

esp_err_t loadI2C(void)
{
	int i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = SCL_PIN;
    conf.scl_pullup_en = 1;
    conf.clk_stretch_tick = 300;
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode));
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));

    // init ssd1306

    if (ssd1306_init(&dev))
    	return ESP_OK;
    else
    	return ESP_FAIL;
}

void clear_buffer()
{
	for (int i = 0; i < DISPLAY_HEIGHT*DISPLAY_WIDTH/8; i++)
		buff[i] = 0x00;
}

void get_time()
{
	const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[2048];

    while(1) {
        int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 0, "DNS successful.",
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);

      	vTaskDelay(1000 / portTICK_PERIOD_MS);

        ssd1306_load_frame_buffer(&dev, buff);

        char ip_str[] = "IP - ";

        strcat(ip_str, inet_ntoa(*addr));

        ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 10, ip_str,
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);

        ssd1306_load_frame_buffer(&dev, buff);

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 20, "Socket allocated",
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);
      	
        ssd1306_load_frame_buffer(&dev, buff);


        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 30, "Connected!",
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);
      	
        ssd1306_load_frame_buffer(&dev, buff);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);

            int length = sizeof(recv_buf)/sizeof(recv_buf[0])-603;
            char token[length];

            for (int i = 0; i < length; i++)
            {
                token[i] = recv_buf[603+i];
            }

            cJSON *root = cJSON_Parse(token);
            const cJSON *json_Time = NULL;
            json_Time = cJSON_GetObjectItemCaseSensitive(root, "datetime");
            if (cJSON_IsString(json_Time) && (json_Time->valuestring != NULL))
            {                                            
	            char *datetime = cJSON_GetObjectItemCaseSensitive(root, "datetime")->valuestring;

	            char *date = strtok(datetime, "T");

	            char *time = strtok(strtok(NULL, "T"), ".");



	            clear_buffer();
	            ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 0, time,
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	            ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 10, date,
	            OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	            ssd1306_load_frame_buffer(&dev, buff);
	            ssd1306_display_on(&dev, true);
            }

            cJSON_Delete(root);
            

        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
        close(s);

        vTaskDelay(3000 / portTICK_PERIOD_MS);
        break;
    }

}


void looper(void *pvParameters)
{
	loadPins();
    loadI2C();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    ssd1306_display_on(&dev, true);
	clear_buffer();
	ssd1306_load_frame_buffer(&dev, buff);

    while(1)
    {
    	if (!gpio_get_level(GPIO_NUM_14))
    	{
    		printf("%s\n", "here");
            ssd1306_display_on(&dev, true);
    		clear_buffer();
    		ssd1306_draw_string(&dev, buff, font_builtin_fonts[FONT_FACE_GLCD5x7], 0, 0, "Fetching the time!",
            OLED_COLOR_WHITE, OLED_COLOR_BLACK);
            ssd1306_load_frame_buffer(&dev, buff);
    		vTaskDelay(1000 / portTICK_PERIOD_MS);
    		clear_buffer();
            get_time();
    	}
    	else
    	{
    		ssd1306_display_on(&dev, false);
    	}

    	vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
	xTaskCreate(&looper, "looper", 16384, NULL, 5, NULL);
}
