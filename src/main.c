/**
 * ,---------,       ____  _ __
 * |  ,-^-,  |      / __ )(_) /_______________ _____  ___
 * | (  O  ) |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * | / ,--Â´  |    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *    +------`   /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * Copyright 2023 Bitcraze AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

 #include <zephyr/kernel.h>
 #include <zephyr/sys/printk.h>
 
 #include <zephyr/drivers/clock_control.h>
 #include <zephyr/drivers/clock_control/nrf_clock_control.h>
 
 #include <zephyr/usb/usb_device.h>
 
 #include "radio_mode.h"
 #include "led.h"
 #include "crusb.h"
 #include "fem.h"
 #include "button.h"
 #include "esb.h"
 
 #include "rpc.h"
 #include "api.h"
 
 #include <tinycbor/cbor.h>
 #include <tinycbor/cbor_buf_reader.h>
 #include <tinycbor/cbor_buf_writer.h>
 
 #include <nrfx_clock.h>

 #ifdef CONFIG_LOAD_CFG
	#include <stdio.h>
	#include <string.h>
	#include <sign_scheme.h>
 #endif
 
 #include <zephyr/logging/log.h>
 LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

 #ifdef CONFIG_LOAD_CFG
 sign_config_t sign_config;
 void load_cfg(const char *cfg_path) {
    FILE *file = fopen(cfg_path, "r");
    if (!file) {
        printf("Config file not found! Using defaults.\n");
        return;
    }

    char key[50], value[100];
    while (fscanf(file, "%49s = %99s\n", key, value) == 2) {
        if (strcmp(key, "app_type") == 0) {
            config->app_type = atoi(value);
        } else if (strcmp(key, "scheme_type") == 0) {
            config->scheme_type = atoi(value);
		} else if (strcmp(key, "sign_size") == 0) {
            config->sign_size = atoi(value);
        } else if (strcmp(key, "private_key") == 0) {
            strncpy(config->private_key, value, sizeof(config->private_key));
        } else if (strcmp(key, "public_key") == 0) {
            strncpy(config->public_key, value, sizeof(config->public_key));
		} else if (strcmp(key, "log_file") == 0) {
            strncpy(config->log_file, value, sizeof(config->log_file));
        } else if (strcmp(key, "log_enabled") == 0) {
            config->log_enabled = atoi(value);
        }
    }

    fclose(file);
}
#endif

 int startHFClock(void)
 {
	 nrfx_clock_hfclk_start();
	 return 0;
 }
 
 #ifndef CONFIG_LEGACY_USB_PROTOCOL
 K_MUTEX_DEFINE(usb_send_buffer_mutex);
 void send_usb_message(char* data, size_t length) {
    static struct crusb_message message;
	
	#ifdef CONFIG_LOAD_CFG
	if (length > USB_MTU + sign_config.sign_size) {  // Ensure message + signature fits
		return;
	}
	#else
	if (length > USB_MTU) {  
		return;
	}
	#endif

	k_mutex_lock(&usb_send_buffer_mutex, K_FOREVER);

	// Generate a cryptographic signature for the message
	uint8_t signature[sign_config.sign_size];
	sign(signature, (uint8_t *)data, length);

	// Build the final signed message
	memcpy(message.data, &length, sizeof(uint16_t));  // Add header (message length)
	memcpy(message.data + sizeof(uint16_t), data, length);  // Add original message
	memcpy(message.data + sizeof(uint16_t) + length, signature, sign_config.sign_size);  // Add signature

	message.length = length + sign_config.sign_size + sizeof(uint16_t);
	crusb_send(&message);

	k_mutex_unlock(&usb_send_buffer_mutex);
}
 #endif
 
 void main(void)
 {
	 printk("Hello World! %s\n", CONFIG_BOARD);
 
	 // HFCLK crystal is needed by the radio and USB
	 startHFClock();
 
	 led_init();
	 led_pulse_green(K_MSEC(500));
	 led_pulse_red(K_MSEC(500));
	 led_pulse_blue(K_MSEC(500));
 
	 button_init();
 
 #ifndef CONFIG_LEGACY_USB_PROTOCOL
	 radio_mode_init();
 #else
	 esb_init();
 #endif

 #ifdef CONFIG_LOAD_CFG
 	 load_cfg("sign_scheme.cfg");
 #endif
 
	 fem_init();
 
	 // Test the FEM
	 printk("Enaabling TX\n");
	 fem_txen_set(true);
	 k_sleep(K_MSEC(10));
	 bool enabled = fem_is_pa_enabled();
	 printk("PA enabled: %d\n", enabled);
	 fem_txen_set(false);
 
	 printk("Enaabling RX\n");
	 fem_rxen_set(true);
	 k_sleep(K_MSEC(10));
	 enabled = fem_is_lna_enabled();
	 printk("LNA enabled: %d\n", enabled);
	 fem_rxen_set(false);
 
	 int ret = usb_enable(NULL);
	 if (ret != 0) {
		 LOG_ERR("Failed to enable USB");
		 return;
	 }
 
 #ifndef CONFIG_LEGACY_USB_PROTOCOL
	 rpc_transport_t usb_transport = {
		 .mtu = USB_MTU,
		 .send = send_usb_message,
	 };
 
	 // RPC loop
	 while (1) {
		static struct crusb_message message;
		static char response_buffer[USB_MTU];
	
		// Receber mensagem via USB
		crusb_receive(&message);
		LOG_INF("Received %d byte message from USB!", message.length);
	
		// Extrair cabeçalho, mensagem e assinatura
		uint16_t msg_length;
		memcpy(&msg_length, message.data, sizeof(uint16_t));
	
		uint8_t *msg_raw = message.data + sizeof(uint16_t);  // Extrair mensagem original
		uint8_t *signature = message.data + sizeof(uint16_t) + msg_length;  // Extrair assinatura
	
		// Obter a chave pública
		/*pki_t public_key = read_key(0);  // 0 = PUBLIC_KEY
	
		if (public_key.key_data == NULL) {
			LOG_ERR("Public key not loaded. Dropping message.");
			continue;
		}*/
	
		// Verificar a assinatura digital antes de processar
		int verify_result = verify(msg_raw, signature, msg_length);
	
		if (verify_result > 0) {
			LOG_INF("Valid signed message received.");
			
			// Enviar para processamento RPC
			rpc_error_t error = rpc_dispatch(&crazyradio2_rpc_api, msg_raw, msg_length, usb_transport, response_buffer);
			LOG_INF("Dispatching result: %d", error);
			
			// Registrar sucesso no log (opcional)
			send_log_data(rand(), 0b00011001, time(NULL), msg_length);
			
		} else {
			LOG_ERR("Invalid signature! Dropping message.");
			
			// Registrar erro no log (opcional)
			send_log_data(rand(), 0b00011010, time(NULL), msg_length);
		}
	}
 #else
	 while(1) {
		 k_sleep(K_MSEC(1000));
	 }
 #endif
 }
 