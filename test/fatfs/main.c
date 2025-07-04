/* main.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/

#include <stdio.h>
//
#include "pico/stdlib.h"
//
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

/**
 * @file main.c
 * @brief Minimal example of writing to a file on SD card
 * @details
 * This program demonstrates the following:
 * - Initialization of the stdio
 * - Mounting and unmounting the SD card
 * - Opening a file and writing to it
 * - Closing a file and unmounting the SD card
 */

int main() {
    // Initialize stdio
    stdio_init_all();

    //スイッチ入力があるまで待機(usbシリアルコンソールの監視の開始まで)
    gpio_init(15);
    gpio_set_dir(15, GPIO_IN);
    while (!gpio_get(15));

    puts("Hello, world!");

    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html
    FATFS fs;
    FRESULT fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // Open a file and write to it
    FIL fil;
    const char* const filename = "test.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        printf("f_printf failed\n");
    }

    DIR dir;
    static FILINFO fno;
	char path[256] = "";

    fr = f_opendir(&dir, "");                   /* Open the directory */
    if (fr == FR_OK) {
        while(1) {
            fr = f_readdir(&dir, &fno);           /* Read a directory item */
            if (fno.fname[0] == 0) break;          /* Break on error or end of dir */
            if (fno.fattrib == AM_DIR) {            /* The item is a directory */
            } else {                               /* The item is a file. */
                f_printf(&fil,"%s\n", fno.fname);
            }
        }
        f_closedir(&dir);
    }

    // Close the file
    fr = f_close(&fil);
    if (FR_OK != fr) {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    // Unmount the SD card
    f_unmount("");

    puts("Goodbye, world!");
}
