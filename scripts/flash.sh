#!/usr/bin/env bash
# flash.sh — BÀI TẬP BASH CHẶNG 1 (learn-bash-quickly, ch.02–05)
#
# Đặc tả: tự viết script đáp ứng các yêu cầu sau.
#
#   Dùng:   ./scripts/flash.sh [đường_dẫn_file.elf]
#
#   [ ] 1. Nếu không truyền tham số → tự tìm file .elf mới nhất trong firmware/**/Debug/
#          (gợi ý: find + ls -t, hoặc vòng lặp so sánh -nt)
#   [ ] 2. Không tìm thấy file → in lỗi ra stderr (>&2) và thoát với exit code 1
#   [ ] 3. In ra tên file, dung lượng, thời điểm build
#   [ ] 4. Nạp bằng:  STM32_Programmer_CLI -c port=SWD -w "<file>" -v -rst
#   [ ] 5. Nạp lỗi (exit code ≠ 0) → in gợi ý: "Kiểm tra ST-Link / dây SWD / BOOT0"
#   [ ] 6. In tổng thời gian chạy (gợi ý: biến $SECONDS)
#   [ ] 7. Chạy `shellcheck scripts/flash.sh` không còn cảnh báo
#
# Mở rộng (nếu còn thời gian):
#   [ ] Thêm tùy chọn -b để build trước khi nạp (sẽ dùng lại ở Chặng 2)
#   [ ] Ghi log mỗi lần nạp vào logs/flash.log, có timestamp
#
# Đọc scripts/check-env.sh làm mẫu về hàm, mảng và exit code.

set -euo pipefail

# TODO: viết code của bạn ở đây
echo "flash.sh chưa được viết — xem đặc tả ở đầu file." >&2
exit 1
