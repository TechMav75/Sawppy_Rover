/*

Using ESP32 RMT to read servo pulses sent by radio remote control receiver.
Publishes result to a queue of joy_msg.

Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html
Reference: https://github.com/JustinOng/sumo/blob/master/software/sumo/src/configure_rmt.c
  (via https://www.reddit.com/r/esp32/comments/bivbz1/can_a_esp32_read_the_pwm_signal_of_a_rc_receiver/)

Copyright (c) Roger Cheng

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#include "joy_rmt_rc.h"

#ifdef USE_JOY_RMT_RC

void check(bool value, char* output)
{
  if (value)
  {
    printf(output);
  }
}

void joy_rmt_rc_read_task(void* pvParameter)
{
  esp_err_t er = ESP_OK;
  joy_msg  message;

  // Get ready to use caller-allocated queue for communicating joystick data
  QueueHandle_t xJoystickQueue;
  if (NULL == pvParameter)
  {
    printf("ERROR: joy_rmt_rc_read_task parameter is null. Expected handle to joystick data queue.\n");
    vTaskDelete(NULL); // Delete self.
  }
  xJoystickQueue = (QueueHandle_t)pvParameter;

  // Configure RMT peripheral for each RC channel
  RingbufHandle_t ringbuffers[axis_count];

  for (int i = 0; i < axis_count; i++)
  {
    rmt_config_t rmt_rx_config = {
      .rmt_mode = RMT_MODE_RX,
      .channel = rc_channels[i].channel,
      .gpio_num = rc_channels[i].pin,
      .clk_div = rmt_clock_divider,
      .mem_block_num = 1,
      .rx_config.filter_en = true,
      .rx_config.filter_ticks_thresh = rmt_filter_threshold,
      .rx_config.idle_threshold = rmt_idle_threshold,
    };

    er = rmt_config(&rmt_rx_config);
    check(ESP_OK != er, "ERROR: rmt_config failed\n");

    er = rmt_driver_install(rc_channels[i].channel, 1024, 0);
    check(ESP_OK != er, "ERROR: rmt_driver_install failed\n");

    er = rmt_get_ringbuf_handle(rc_channels[i].channel, &(ringbuffers[i]));
    check(ESP_OK != er, "ERROR: rmt_get_ringbuf_handle failed\n");
    check(NULL == ringbuffers[i], "ERROR: rmt_get_ringbuf_handle gave a NULL handle\n");

    er = rmt_rx_start(rc_channels[i].channel, true);
    check(ESP_OK != er, "ERROR: rmt_rx_start failed\n");
  }

  // Read loop
  size_t length = 0;
  rmt_item32_t *items = NULL;
  uint32_t duration;
  while(true)
  {
    for (int i = 0; i < axis_count; i++)
    {
      duration = 0;
      items = (rmt_item32_t *)xRingbufferReceive(ringbuffers[i], &length, portMAX_DELAY);
      if (NULL != items)
      {
        // Convert length from number of bytes to number of entries
        length /= sizeof(rmt_item32_t);

        // Read high period of most recent data point
        rmt_item32_t recent = items[length-1];
        if (0 != recent.level0)
        {
          duration = recent.duration0;
        }
        else if (0 != recent.level1)
        {
          duration = recent.duration1;
        }

        // Return memory to ring buffer
        vRingbufferReturnItem(ringbuffers[i], (void*)items);

        // Clamp between min & max
        if (duration < rc_receive_min)
        {
          duration = rc_receive_min;
        }
        else if (duration > rc_receive_max)
        {
          duration = rc_receive_max;
        }

        // Convert range from (rc_receive_min, rc_receive_max) to (-1, 1)
        message.axes[i] = -1.0 + 2.0*((float)duration-rc_receive_min)/(rc_receive_max-rc_receive_min);
      }
      else
      {
        // No data? Failsafe to center.
        message.axes[i] = 0.0;
      }
    }

    message.timeStamp = xTaskGetTickCount();

    xQueueOverwrite(xJoystickQueue, &message);
  }
}

#endif // #ifdef USE_JOY_RMT_RC
