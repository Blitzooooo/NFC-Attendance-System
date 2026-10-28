# NFC 应用模块使用手册

> **分类：应用程序**（业务逻辑层，封装 RC522 驱动完成考勤读卡与串口发卡协议）

## 1. 模块作用

NFC 考勤的业务封装：把 RC522 底层的"寻卡 / 认证 / 读写块"封装成
"寻卡 → 验证 → 读卡 → 校验 → 分类 → 读位图"的高层流程，并实现 PC 串口发卡协议
（ISSUE / READ / IMGA / IMGN / IMGD / UPDATEIMG / CLEAR / LIST / LOST / UNLOST）。

## 2. 函数清单

| 函数 | 做了什么 | 用了什么底层 | 为什么这么设计 |
|------|---------|-------------|---------------|
| `NFC_Init()` | 初始化 RC522 | `RC522_Platform_Init` + `RC522_ConfigISOType('A')` | 统一入口，屏蔽平台细节 |
| `NFC_ScanCard()` | 寻卡 | `RC522_ScanCard` | 薄封装，便于换卡协议 |
| `NFC_ReadCard()` | 认证 + 读 Block1 + 校验 + 分类 + 读头像/姓名/部门位图 | `RC522_AuthState` / `RC522_ReadBlock` | 一次读全量，供 GUI 显示 |
| `NFC_ReadCardHeader()` | 仅读 Block1 头，不读位图 | 同上 | 全量读约 1s，仅读头约 10ms；重复刷卡只刷头省时间 |
| `NFC_WaitCardOff()` | 等待卡离开射频场 | `RC522_WaitCardOff` | 防止重复触发 |
| `NFC_Recover()` | 复位 RC522 并重配 | `RC522_Reset` + `ConfigISOType` | 命令失败后清 FIFO 脏状态 |
| `NFC_ProcessCommand()` | 串口发卡协议 | `rc522` + `record` | 上位机与设备交互入口 |
| `NFC_LockReader/UnlockReader` | RC522 并发锁 | 原子标志 | uartTask 与 nfcTask 共享 RC522，防竞争 |

## 3. 关键实现

- **认证重试**：`NFC_READ_RETRY = 3`，失败先 `RC522_Halt()` + `osDelay(5)` 再重新寻卡。
- **数据校验**：Block1 前 14 字节求和 == 末 2 字节校验和，且 `statusFlag != 0xFF`。
- **分类**：`cardType` 0x02=管理员卡、0x01=图像卡、其余=普通卡。
- **位图布局**：头像 24 块(48×64)、姓名 10 块(80×16)、部门 10 块(80×16)。

## 4. 与其它模块耦合

- 依赖：`rc522`（硬件）、`delay_us`、`record`（黑名单/记录）、`display`（共享位图缓冲）。
- 被调用：`freertos.c` 的 nfcTask / uartTask。

## 5. 注意事项

- **结构体布局不可改**（`NFC_CardData_t` 与卡上数据、上位机协议强耦合）。
- 读卡必须持有 `NFC_LockReader` 锁，避免与串口发卡竞争。
- 时间同步期间（`DISP_IsTimeSyncBusy`）禁止读卡。
- 扇区间 `osDelay(2)` 是为让 guiTask 刷 OLED；与阻塞式 I2C 的优先级冲突见 README 第 18.5 节。
