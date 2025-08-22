
<img width="1099" height="314" alt="image" src="https://github.com/user-attachments/assets/15ab0996-d049-4504-8225-8518f2b27659" />
<img width="867" height="662" alt="Untitled" src="https://github.com/user-attachments/assets/a51058e4-aef6-4372-af57-780177bdcd26" />


What to load (classic ESP32)

Use these three files/addresses:

File (from your build folder)	Offset (hex)
1-HelloWorld.ino.bootloader.bin	0x1000
1-HelloWorld.ino.partitions.bin	0x8000
1-HelloWorld.ino.bin (the app)	0x10000

Or Just merged.bin @ 0x0 

Common Issues

Wrong offset placement → If even one of the 3 bins went to the wrong address, the ESP32 boots garbage and nothing runs.

Must be exactly:

bootloader.bin → 0x1000

partitions.bin → 0x8000

app.bin (sketch) → 0x10000

Wrong flash mode / speed → Some ESP32 boards don’t like DIO + 40MHz.

Try switching SPI MODE to QIO and/or SPI SPEED to 80MHz or 20MHz.

Official Arduino core defaults are usually QIO, 40MHz.

Merged.bin is safer → When you flash the *.merged.bin at 0x0, the tool writes everything in one go, eliminating misalignment.





Flash Download Tool settings (ESP32)

Chip: ESP32

SPI SPEED: 40 MHz

SPI MODE: DIO

DoNotChgBin: checked (leave as-is)

Add three rows with the paths and offsets above.

COM: your port, BAUD: 115200 (or 460800 if your USB/UART is solid).

Put the board in boot mode (hold/strap IO0=GND then reset), then click START.

When it finishes “FINISH”, press EN/RESET.

Alternative: flash the merged image

Arduino also produced 1-HelloWorld.ino.merged.bin.
If you prefer one file:

Add just that file at offset 0x0 and flash.

Keep the same SPEED/MODE. (The merged image already contains the bootloader, partitions and app at their correct internal offsets.)

If you changed partitions

If you use a custom partitions layout, the partitions offset may differ. You can verify the exact offsets by checking the Arduino upload log 
(look for the esptool.py write_flash command) or your partitions.csv. Otherwise the defaults above are correct for standard ESP32 boards.
