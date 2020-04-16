# psp2hfw
The goal of this repository is to provide tools and instructions for running a hybrid firmware (HFW) on the Playstation Vita console
 - in this case a different firmware/software version than the bootloaders version(s).
 
# Preparation
0) Make sure that you have the latest version of enso_ex ( https://github.com/SKGleba/enso_ex ) installed and running
1) Download the correct 0syscall6 bootmgr version for from https://github.com/SKGleba/enso_ex/tree/master/sdrecovery/BOOTMGR/0syscall6 and put in ux0:eex/data/ as bootmgr.e2xp
2) Download 0syscall6 HFW version from https://github.com/SKGleba/0syscall6 , put it in ur0:tai/ and add to taiHEN config.txt
3) Download the correct firmware version fix from https://github.com/SKGleba/enso_ex/tree/master/sdrecovery/PATCH/fw_spoof and put in ux0:eex/payloads/
4) Download the correct sector redirect script from https://github.com/SKGleba/psp2hfw/tree/master/scripts/sector_redir and put in ux0:eex/payloads/
    - This is optional for non-dualfw users but highly recommended for recovery purposes
5) Copy os0:sm/update_service_sm.self to ux0:eex/data/ as zss_ussm.self; if you have a decrypted kprxauth sm - put it in there as zss_ka.elf
6) Open the enso_ex installer and sync scripts; after reboot make sure that 0syscall6 is working correctly, do it before vita goes to sleep
    - In os0 you should have the following files: bootmgr.e2xp, patches.e2xd, and zss_ussm.self.
  
## Additional steps for dual-fw users
Dual FW lets you run the second firmware from a SD card in a GC-SD adapter; it is experimental and expected to be less stable (but safer) than having a single fw.

0) Make sure that you have your GC-SD adapter inserted and detected by the system; use either YAMT or mount as uma0 using another tool
1) Download and run the emmcfw->gcsd clone tool from https://github.com/SKGleba/psp2hfw/tree/master/tools/clone_int2ext_fwonly ; it will clone firmware/software sectors to GCSD
    - It is recommended to use xerpi's plugin loader for that ( https://bitbucket.org/xerpi/vita_plugin_loader )
    - If the load fails it means that the SD card was not detected; also cloning will take some time
    - After the clone finishes uninstall your GC-SD driver unless you are using sony's (internal manufacturing mode or yamt)
2) Connect your vita to a power supply and boot holding START, if it boots - emunand works
    - You can make sure by deleting/adding a file in tm0 and rebooting normally; if the change is not present - emunand works
    - Sony's built-in GC-SD driver is very strict in terms of compatibility, not all sd cards will work
    - For the rest of this guide use the emunand instead of the internal emmc, HFW will be installed to the emunand.
  
# Usage

## Compatibility
Currently not all firmwares can be installed on top of current base bootloaders [3.60 | 3.65]
 - Check out the issues tab to know what needs to be done to increase compatibility
 - Please note that not all homebrew apps/plugins are compatible with all firmwares; HenKaku and TaiHen may be incompatible too.

### Base: 3.60
 - 3.61 everything works fine.
 
### Base: 3.65
 - 3.67 - 3.73 everything works fine.
 
## Installation
1) Download and install the HFW installer (HFWI.vpk); make sure that you have unsafe homebrew enabled in henkaku settings
2) Download the desired firmware PUP and extract os0/vs0 fs images from it, you may use https://github.com/TeamMolecule/sceutils for that
    - psst, if you don't have the required keys use this fork: https://github.com/zecoxao/sceutils
3) Put them in ux0:data/hfw/ as os0.bin and vs0.bin; if you are using dualfw put os0:patches.e2xd in ux0:data/hfw/patches.e2xd
4) Open the installer and press [start] to flash, it may take some time; after the flash completes it will show you the current HFW info and ask to reboot
    - if the vita does not reboot follow the steps in the recovery section
5) Open the enso_ex installer and sync scripts.
 
## Uninstallation
1) Download your original firmware PUP and extract the vs0 fs image from it
2) Put it in ux0:data/hfw/ as vs0_r.bin; if you want to go back to a pristine os0 put it as os0_r.bin
3) Open the installer and press [start] to restore, it may take some time; after the restore completes it will ask you to reboot
    - if the vita does not reboot follow the steps in the recovery section.
  
## Recovery
With enso_ex you should be able to recover from all possible HFW related soft/"hard" bricks.
 - If you corrupt boot_config.txt or suspect that it causes a bootloop hold VOLUP at boot.
 
### Bootloop
0) If you synced incompatible patches follow the [Bootloop - broken patches] section; skip this step if you already did it
1) Download the correct sdrestore image from https://github.com/SKGleba/psp2hfw/tree/master/recovery , its version should match the current bootloaders version
2) Flash this image to an SD card and put it in the GC-SD adapter into the PS Vita GC slot
3) Connect the console to a power source and hold [select] and [power] for 20-30s, then keep holding select; it will restore the previous os0.
    - If the vita does not show the logo, hold power for 30s afterwards and see if it works
    - If the vita shows the bootlogo - go into safe mode and reinstall the firmware, do not follow the next steps
4) Dump the first 0x200 bytes from the SD card (using [read] in win32dimg or dd) and open using a hex editor
    - if the first 4 bytes are BE BA FE CA (0xcafebabe) - payload did not run (either faulty sd2vita/slot or incorrect image; or just held select for too short); retry from step 1
    - if the first 4 bytes are EF BE AD DE (0xdeadbeef) - should not happen, weird, retry from step 1
    - if the first 4 bytes are EF BE FE CA (0xcafebeef) - payload finished, if the vita does not boot up then the inactive/recovery os0 is broken; follow the next steps
    - if the bytes 12-20 are not 00 - the flash or read failed, make sure its a correct image and retry from step 1; if it still fails create an issue here on github
5) Download the correct sdosflash image from https://github.com/SKGleba/psp2hfw/tree/master/recovery , its version should match the current bootloaders version
6) Flash this image to an SD card and put it in the GC-SD adapter into the PS Vita GC slot
7) Connect the console to a power source and hold [select] and [power] for 20-30s, then keep holding select; it will flash a clean os0.
    - If the vita does not show the logo, hold power for 30s afterwards and see if it works; if it still does not - create an issue here on github
    - If the vita shows the bootlogo - go into safe mode and reinstall the firmware.
  
### Bootloop - broken patches
0) You can not just hold VOLDOWN to skip patches since bootmgr is required for HFW to boot.
1) Download the correct cleanboot image from https://github.com/SKGleba/psp2hfw/tree/master/recovery , its version should match the current bootloaders version
2) Flash this image to an SD card and put it in the GC-SD adapter into the PS Vita GC slot
3) Connect the console to a power source and hold [select] and [power] for 20-30s, then keep holding select; it will skip all custom patches.
    - If the vita does not boot, follow the [Bootloop] section
	
# Notes
 - While psp2hfw is in beta it should be considered a PoC for advanced users; A 1-click method will come with the next firmware update.
 - My base testing firmware is 3.65 and i recommend it for using this toolset with, it assures full compatibility and ease of debugging.
 - You can find some pre-extracted filesystem images here: https://mega.nz/folder/v45lhYBR#t2TSvnynd50e76B5OTR2XA
  
# Credits
 - Team Molecule for henkaku, taihen, enso and the update_sm 0x50002 write primitive.
 - TheFlow for help with the sleep/resume stuff.
 
 
 
 
 
 