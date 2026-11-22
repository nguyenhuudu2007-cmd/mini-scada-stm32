#!/usr/bin/env bash
# check-env.sh — Kiểm tra công cụ và phần cứng cho dự án STM32 RTC–Servo Host.
# Dùng: ./scripts/check-env.sh
# Script mẫu để đọc khi học bash: hàm, mảng, vòng lặp, điều kiện, exit code.

set -u

missing=0

ok()   { printf '  \e[32m✅\e[0m %-24s %s\n' "$1" "$2"; }
fail() { printf '  \e[31m❌\e[0m %-24s %s\n' "$1" "$2"; missing=$((missing + 1)); }
warn() { printf '  \e[33m⚠️ \e[0m %-24s %s\n' "$1" "$2"; }

check_cmd() {
    local cmd="$1" hint="$2"
    if command -v "$cmd" >/dev/null 2>&1; then
        ok "$cmd" "$(command -v "$cmd")"
    else
        fail "$cmd" "$hint"
    fi
}

echo "== Toolchain & Flash"
check_cmd arm-none-eabi-gcc    "sudo dnf install arm-none-eabi-gcc-cs"
check_cmd STM32_Programmer_CLI "cài STM32CubeProgrammer"
check_cmd st-flash             "sudo dnf install stlink"
check_cmd openocd              "sudo dnf install openocd"
check_cmd cmake                "sudo dnf install cmake"

echo "== Debug & Đo lường"
check_cmd pulseview  "sudo dnf install pulseview"
check_cmd sigrok-cli "sudo dnf install sigrok-cli"
check_cmd picocom    "sudo dnf install picocom"
check_cmd socat      "sudo dnf install socat"

echo "== Host & Tự động hóa"
check_cmd qtcreator  "sudo dnf install qt-creator"
check_cmd qmake6     "sudo dnf install qt6-qtbase-devel"
check_cmd git        "sudo dnf install git"
check_cmd shellcheck "sudo dnf install ShellCheck"

echo "== Quyền truy cập"
if id -nG | grep -qw dialout; then
    ok "nhóm dialout" "mở được /dev/ttyUSB*"
else
    fail "nhóm dialout" "sudo usermod -aG dialout \$USER rồi đăng nhập lại"
fi

echo "== Phần cứng đang cắm"
if lsusb 2>/dev/null | grep -qi '0483:3748\|st-link'; then
    ok "ST-Link" "đã nhận"
else
    warn "ST-Link" "chưa cắm"
fi

shopt -s nullglob
ports=(/dev/ttyUSB* /dev/ttyACM*)
if (( ${#ports[@]} > 0 )); then
    ok "Cổng serial" "${ports[*]}"
else
    warn "Cổng serial" "chưa cắm cáp USB-RS232"
fi

echo
if (( missing == 0 )); then
    echo "Môi trường đã sẵn sàng."
else
    echo "Còn thiếu $missing mục ❌."
fi
exit "$missing"
