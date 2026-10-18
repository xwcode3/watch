#!/bin/bash
# Flash watch_app.elf to STM32F411CEU6 via J-Link
#
# 注意：本脚本有两种调用方式，工作目录不同：
#   1) CMake target `flash`  --> cwd = build/Debug（默认行为）
#   2) 项目根目录 ./flash.sh  --> cwd = 项目根
# 所以不能写相对路径，一律用脚本自身位置推导出绝对路径。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ELF_FILE="$SCRIPT_DIR/build/Debug/watch_app.elf"

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

/usr/local/bin/JLinkExe -device STM32F411CE -if SWD -speed 400 -autoconnect 1 -CommanderScript "$CMD_FILE"
RESULT=$?
rm -f "$CMD_FILE"

if [ $RESULT -eq 0 ]; then
    echo "✅ Flash successful!"
else
    echo "❌ Flash failed"
    exit $RESULT
fi
