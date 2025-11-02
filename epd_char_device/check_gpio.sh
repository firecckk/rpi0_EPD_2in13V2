# 1. GPIO 状态检查
echo "=== GPIO Status ==="
cat /sys/kernel/debug/gpio
echo ""

# 2. 引脚状态检查  
echo "=== Pin Status ==="
raspi-gpio get 8 17 18 24 25
echo ""

# 3. 设备树检查
echo "=== Device Tree ==="
find /proc/device-tree -name "*epd*" 2>/dev/null
echo ""

# 4. 详细错误信息
echo "=== Kernel Messages ==="
sudo dmesg | grep -C5 "epd2in13v2"
