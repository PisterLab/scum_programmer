# scum_programmer

install
=======

- Install the latest [Embedded Studio for ARM](https://www.segger.com/downloads/embeded-studio), v4.52c known to work
- Download the latest [nRF SDK](https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK/Download), unzip the `nRF5_SDK_*.zip` into `C:/nrf`, version 16.0.0 known to work

build & load
============

- Clone repository
- Copy `scum_programmer_firmware` into `C:/nrf/examples/peripheral`
- Double click on `C:\nrf\nRF5_SDK_17.0.2_d674dde\examples\peripheral\scum_programmer_firmware\pca10056\blank\ses\uart_pca10056.emProject`
- Ctrl + F7 to build
- Plug in the nRF52840-DK into the PC via USB port 
- Target > Connect J-Link
- Target > Erase all
- Target > Download solution

run
===
- Find the COM port of your nRF52840-DK in the Device manager
- Edit `scum_programmer_software/comm.py` to set variable `programmer_port` to that COM port
- Run `scum_programmer_software/comm.py`, no command line parameters
    - Currently takes forever because of #6




