setup_var - Tool to read/write values from/to Setup UEFI variable.

Usage: setup_var <offset> [new_value [8|16|32]]
offset: offset to read/write value
new_value: new value (if not specified - just read and show current value)
8|16|32: new value size (if not specified - autodetect from new value)

For offsets, value sizes and applicable value please refer to decompiled IFR from UEFI BIOS installed on the computer using https://github.com/donovan6000/Universal-IFR-Extractor

DISCLAMER: This tool may brick your UEFI BIOS if misused!
I don't responsible to any damage caused by this tool. Use it at your own risk.

