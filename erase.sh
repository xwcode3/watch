#!/bin/bash
# Full chip erase of STM32F411CEU6 via J-Link
echo "⚠️  WARNING: This will erase ALL flash content (Bootloader + APP)!"
echo ""
read -p "Are you sure? Type YES to continue: " CONFIRM

if [ "$CONFIRM" != "YES" ]; then
    echo "Cancelled."
    exit 1
fi

CMD_FILE=$(mktemp /tmp/jlink_cmds.XXXXXX)
cat > "$CMD_FILE" << EOF
erase
exit
EOF

echo "Erasing entire STM32F411CEU6 flash..."
/usr/local/bin/JLinkExe -device STM32F411CE -if SWD -speed 4000 -autoconnect 1 -CommanderScript "$CMD_FILE"
RESULT=$?
rm -f "$CMD_FILE"

if [ $RESULT -eq 0 ]; then
    echo "✅ Full chip erase successful!"
else
    echo "❌ Erase failed"
    exit $RESULT
fi
