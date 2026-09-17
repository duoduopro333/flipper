# BT Scanner - Flipper Zero BLE 蓝牙扫描 FAP 应用

## 功能说明

这是一个为 Flipper Zero 开发的蓝牙低功耗（BLE）设备扫描第三方应用（FAP）。应用提供以下功能：

- 扫描并显示周围BLE设备的MAC地址
- 显示设备信号强度（RSSI）
- 解析并显示广播中的设备名称
- 简洁的GUI界面，支持列表导航
- 按OK键开始/停止扫描

## 项目结构

```
bt_scanner/
├── application.fam      # 应用清单文件（构建配置）
├── bt_scanner.c         # 主程序源代码
├── bt_scanner_10px.png  # 应用图标（10x10像素 1-bit PNG）
├── images/              # 图片资源目录
└── README.md            # 本说明文件
```

## 开发环境搭建

### 前置要求

1. **Flipper Zero 设备** - 任何已刷写官方固件或第三方固件的版本
2. **USB数据线** - 用于连接设备和电脑
3. **uFBT 工具** - Flipper 官方微构建工具（推荐用于FAP开发）

### 安装 uFBT

```bash
# 使用pip安装ufbt（需要Python 3.7+）
pip install ufbt

# 验证安装
ufbt --version
```

### 编译应用

```bash
# 进入应用目录
cd bt_scanner

# 编译FAP文件（会自动下载对应版本SDK）
ufbt

# 编译成功后，产物位于 dist/ 目录下
# 文件名为 bt_scanner.fap
```

### 部署到设备

**方法1：使用uFBT一键部署（推荐开发者）**
```bash
ufbt launch
```
此命令会自动编译并通过USB将FAP安装到已连接的设备上。

**方法2：手动复制**
1. 将 `bt_scanner.fap` 文件复制到Flipper Zero SD卡的 `apps/Bluetooth/` 目录下
2. 重启设备
3. 在主菜单「应用 → Bluetooth」中找到 "BT Scanner" 启动

**方法3：qFlipper图形工具**
使用qFlipper软件的文件管理器功能，将fap文件上传到对应目录。

## 使用说明

1. 启动应用后会看到扫描界面
2. 按 **OK键** 开始扫描附近BLE设备
3. 按 **上/下键** 在设备列表中导航选择
4. 扫描结果显示：
   - 左侧 `>` 标记表示当前选中的设备
   - MAC地址格式：`XX:XX:XX:XX:XX:XX`
   - 右侧数字为RSSI信号强度（数值越大约好，例如-45比-78信号强）
   - 下方为设备广播名称（如果有）
5. 再次按 **OK键** 停止扫描
6. 按 **返回键（Back）** 退出应用

## 关于蓝牙扫描API的重要说明

### 官方固件（OFW）限制
Flipper Zero官方固件目前对第三方FAP开放的BLE API主要集中在蓝牙profile配置、HID连接、广播等功能，**完整的被动BLE扫描（接收所有广播包）API尚未完全对第三方FAP开放**。这是因为Flipper的蓝牙协处理器运行特定固件，扫描功能由系统服务管理。

### 本应用状态
- ✅ 完整的GUI应用框架（ViewPort、按键处理、界面绘制）
- ✅ 蓝牙系统服务接入（RECORD_BT）
- ✅ 设备数据结构、列表管理、RSSI显示
- ✅ ADV广播包名称解析逻辑
- ⚠️ 真实扫描需要依赖固件支持的BLE扫描回调
- 📋 当前代码包含演示模式，展示UI效果

### 在第三方固件上实现真实扫描
如果你使用Momentum、Unleashed、RogueMaster等第三方固件，这些固件扩展了蓝牙API。你可以通过以下方式启用真实扫描：

1. 查找对应固件中的 `furi_hal_bt_extra.h` 头文件
2. 使用 `furi_hal_bt_extra_beacon_*` 系列API或扩展的gap扫描API
3. 注册GAP广告事件回调，在回调中调用 `add_device()` 函数即可

关键修改点在 `simulate_scan_tick()` 函数处，替换为注册真实BLE广告回调：
```c
// 示例：在Momentum固件上启用BLE扫描
#include <furi_hal_bt_extra.h>
static void ble_adv_cb(const GapEvent* event, void* context) {
    if(event->type == GapEventTypeAdvertisingReport) {
        // 解析event中的MAC、RSSI、ADV数据
        add_device(app, mac, rssi, adv_data, adv_len);
    }
}
// 注册回调后启动扫描
furi_hal_bt_extra_beacon_set_callback(ble_adv_cb, app);
furi_hal_bt_extra_beacon_start();
```

## 扩展开发建议

你可以在此基础上扩展更多功能：

1. **设备详情页** - 显示完整ADV数据、服务UUID列表
2. **RSSI历史** - 绘制信号强度曲线图
3. **设备过滤** - 按名称、MAC、RSSI阈值过滤
4. **日志导出** - 将扫描结果保存到SD卡文件
5. **信标检测** - 识别iBeacon、Eddystone等特定协议
6. **连接功能** - 点击设备尝试BLE连接并枚举服务

## 故障排除

1. **API版本不匹配错误**
   - 运行 `ufbt update` 更新本地SDK到最新版本
   - 检查设备固件版本，确保与SDK版本对应

2. **应用不显示在菜单中**
   - 确认fap文件放在了 `apps/Bluetooth/` 目录下
   - 重启Flipper Zero刷新应用列表
   - 确认application.fam中的fap_category设置正确

3. **编译错误**
   - 确保使用ufbt而非系统gcc直接编译
   - 运行 `ufbt clean` 清理后重新编译
   - 检查C代码语法，确保头文件路径正确

## 版本兼容性

- 固件API版本：v1.x（适用于0.99+及以上版本官方固件）
- 测试编译工具链：arm-none-eabi-gcc（ufbt自动管理）
- 支持固件：官方OFW、Momentum、Unleashed、RogueMaster

## 许可证

本示例代码采用MIT许可证发布，可自由修改和分发。
