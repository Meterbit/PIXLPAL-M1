#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>


#include "mtb_graphics.h"
#include "mtb_text_scroll.h"
#include "gifdec.h"
#include "mtb_graphics.h"
#include "mtb_nvs.h"
#include "mtb_gif_parser.h"
//#include "esp_heap_caps.h"

uint8_t mtb_draw_gif(const char *dGifPath, uint16_t xAxis, uint16_t yAxis, uint32_t loopCounter){
	if(litFS_Ready != pdTRUE) return 1;		// No reading from littlefs if the otaStorage was not successful.
	String imageFilePath = "/littlefs" + String(dGifPath);
	uint16_t width, height, colorHolder;
	gd_GIF *gif = gd_open_gif(imageFilePath.c_str());
	width = gif->width;
	height = gif->height;
	if (width < 129 && height < 65)
	{
		uint8_t *buffer = (uint8_t *)malloc(width * height * 3);
		//uint8_t *buffer = (uint8_t *)heap_caps_calloc(width * height * 3,sizeof(uint8_t), MALLOC_CAP_SPIRAM);
		for (uint32_t looped = 0; looped < loopCounter; looped++)
		{
			while (gd_get_frame(gif))
			{
				gd_render_frame(gif, buffer);

				for (uint8_t p = 0, x = xAxis; p < width; p++, x++){
					for (uint8_t q = 0, y = yAxis; q < height; q++, y++){
			   colorHolder = dma_display->color565(buffer[q*width*3 + p*3], buffer[q*width*3 + p*3 + 1], buffer[q*width*3 + p*3 + 2]); 
			  dma_display->drawPixel(x, y, colorHolder);    	// update color
					}
				}

				vTaskDelay(pdMS_TO_TICKS(gif->gce.delay * 10));
			}
			gd_rewind(gif);
		}
		free(buffer);
		return 0;
	}
	else
		//ESP_LOGI(TAG, "Gif Dimensions is: Width = %d; Height = %d;\n", width, height);
	gd_close_gif(gif);
	return 1;
}