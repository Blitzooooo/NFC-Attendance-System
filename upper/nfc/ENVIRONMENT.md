# 环境配置

## 编译器

| 工具 | 版本 |
|------|------|
| C++ 编译器 | MinGW 13.1.0 64-bit (g++) |
| C++ 标准 | C++17 |

## Qt 框架

| 项 | 值 |
|----|-----|
| Qt 版本 | 6.11.1 |
| Qt 模块 | `core` `gui` `widgets` `serialport` `sql` |
| 构建系统 | qmake (.pro) |

## 数据库

| 项 | 值 |
|----|-----|
| 类型 | SQLite 3 |
| 驱动 | QSQLITE (Qt 内置) |
| 文件 | `attendance.db` (程序目录自动创建) |

## 目标平台

| 项 | 值 |
|----|-----|
| 操作系统 | Windows 10/11 64-bit |
| 类型 | 桌面 GUI 应用 |

## 硬件依赖

| 设备 | 说明 |
|------|------|
| NFC 读卡器 | 基于 RC522，通过串口连接 |
| 串口芯片 | CH340 / CP210x / STLink 等 USB-TTL |

## 安装步骤

1. 安装 [Qt 6.11.1](https://www.qt.io/download)（勾选 MinGW 13.1.0 64-bit 组件）
2. 确保安装时选中以下 Qt 模块：
   - Qt SerialPort
   - Qt SQL
3. 用 Qt Creator 打开 `NfcAttendanceHost.pro`
4. 左下角选择 `Desktop Qt 6.11.1 MinGW 64-bit` 套件
5. 点击构建（Ctrl+B）→ 运行（Ctrl+R）

## 运行时文件（自动生成）

| 文件 | 用途 |
|------|------|
| `attendance.db` | SQLite 考勤数据库 |
| `issue_records.csv` | 发卡记录备份 |
| `issued_cards/` | 卡面图像文件 |
