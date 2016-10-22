BLUETOOTH ARCADE OR JOYSTICK PCB FIRMWARE
==========

This document assumes using an NRF51822 Rev3 (ac) with 256KB of flash and 32KB of RAM. The whole project is
not provided because NordicSemiconductor examples are not opensource. So diff files and instruction are
explained here to build the firmware.


Two firmwares can be compiled for different uses:
 * Arcade : 2 digital axis with 10 buttons.
 * Analog : 5 analog axis with 14 buttons (not working).



**Caution** : Firmwares have to be tested. Analog firmware does not work during adc sampling. In fact, adc sampling is launched on
BLE_GAP_EVT_CONNECTED but leads to disconnection (peripheral or central?). It seems adc routines block ble bonding process and disconnect.




Download nRF5_SDK_12.1.0_0d23e2a and extract it to nrf_sdk12 directory.
In file components/ble/ble_radio_notification/ble_radio_notification.c, add #include "nrf_nvic.h" after #include <stdlib.h>.

Duplicate examples/ble_peripheral/ble_app_hids_keyboard to examples/ble_peripheral/nrf51_hid_joy.

Remove inside:
 * pca10040 
 * hex/*.hex
 * pca10028/s130/arm4
 * pca10028/s130/iar
 * ble_app_hids_keyboard.eww
 * pca10028/s130/arm5_no_packs/RTE/Device


Apply patch to (patch filename < patch_file):
 * main.c (main.patch)
 * pca10028/s130/config/sdk_config.h (sdk_config.patch)
 * pca10028/s130/armgcc/Makefile (makefile.patch)


Add files:
 * time.c
 * time.h
 * joy.c
 * joy.h


With Keil UVision :
Add files to project ("Add files to group appplication"):
 * joy.c
 * joy.h
 * time.c
 * time.h


Remove files or group:
 * Board Support Group
 * sensorsim.c on nRF_librairies
 * app_button.c on nRF_librairies
 * Add components/drivers_nrf/timer/nrf_drv_timer.c to nRF_Drivers group
 * Add components/drivers_nrf/nrf_soc_nosd/nrf_nvic.c to nRF_Drivers group
 * Add components/ble/ble_radio_notification/ble_radio_notification.c to nRF_BLE group


In the target option view: 
 * Change C/C++ and ASM define :
	* Remove BOARD_PCA10028
	* Replace NRF51422 by NRF51822
	* Add NRF_LOG_ENABLED=1 to enable UART log NRF_LOG_ENABLED=0 to disable it.
	* Add DEBUG to enable debug purposes.
	* Add ARCADE_INPUT for arcade firmware (default) or ANALOG_INPUT for analog firmware.
 * C/C++ Include paths:
    * Add ..\..\..\..\..\..\components\ble\ble_radio_notification
	



With GCC :
On pca10028/s130/armgcc directory, launch make.

