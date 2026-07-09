#!/bin/bash
# Flash watch_bootloader.elf to STM32F411CEU6 via J-Link
#ELF_FILE="build/Debug/watch.elf"
ELF_FILE="watch_bootloader.elf"

if [ ! -f "$ELF_FILE" ]; then
    echo "❌ $ELF_FILE not found. Run cmake --build first."
    exit 1
fi

echo "Flashing $(basename $ELF_FILE) to STM32F411CEU6..."

# Write commands to a temp file, JLinkExe V9.30 needs a real file for -CommanderScript
CMD_FILE=$(mktemp /tmp/jlink_cmds.XXXXXX)
cat > "$CMD_FILE" << EOF
loadfile $ELF_FILE
r
g
exit
EOF

/usr/local/bin/JLinkExe -device STM32F411CE -if SWD -speed 4000 -autoconnect 1 -CommanderScript "$CMD_FILE"
RESULT=$?
rm -f "$CMD_FILE"

if [ $RESULT -eq 0 ]; then
    echo "✅ Flash successful!"
else
    echo "❌ Flash failed"
    exit $RESULT
fi
