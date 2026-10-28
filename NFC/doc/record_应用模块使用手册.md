# record 记录存储应用模块使用手册

> **分类：应用程序**（业务逻辑层，SPI Flash 持久化存储）

## 1. 模块作用

从 display 拆分出的独立存储模块，负责系统配置、刷卡记录、LRU 缓存、黑名单的
Flash 持久化与 CRC16 校验，确保掉电不丢。

## 2. 函数清单

| 函数 | 做了什么 | 用了什么底层 | 为什么这么设计 |
|------|---------|-------------|---------------|
| `CRC16_Calc()` | CCITT-FALSE CRC16 | 软件移位 | 所有 Flash 数据校验 |
| `Config_Load/Save()` | 系统配置读/写 | `W25QXX_Read/Write` | 主 + 备双区，任一 CRC 失败自动回退 |
| `Record_Init()` | 读记录头，校验 magic | `W25QXX_Read` | 上电恢复写指针/总数 |
| `Record_Add()` | 追加记录 + 回绕 | `W25QXX_Write` | 循环写 128 条/扇区 |
| `Record_QueryLast()` | 倒扫 Flash 找最近记录 | `W25QXX_Read` + CRC | 跳过 seqNum=0 / 损坏记录 |
| `Record_List()` | 输出记录列表 | — | 供上位机 LIST 查询 |
| `Record_FixAllTimeOffsets()` | NTP 校准后回写 OFS | 遍历 + CRC 重算 | 修正未校准记录时间 |
| `Record_MarkAllUploaded / GetUnuploadCount` | 上传追踪 | — | 心跳 PEND 字段 |
| `Cache_Find/Update/Clear()` | LRU 卡片在场缓存 | RAM 数组 | 避免每次都倒扫 Flash |
| `Blacklist_*` | 挂失名单增删查 | Flash 扇区4 + CRC | 掉电保持，最多 50 条 |

## 3. Flash 布局

| 区域 | 地址 | 大小 | 内容 |
|------|------|------|------|
| 主配置 | `0x000000` | 16B | SysConfig_t + magic + CRC16 |
| 备配置 | `0x001000` | 16B | 配置备份 |
| 记录头 | `0x002000` | 32B | 写指针/总数/上传偏移 |
| 记录区 | `0x003000` | 4096B | 128 条 × 32B 循环写 |
| 黑名单 | `0x004000` | 208B | 50 条 UID + CRC16 |

## 4. 与其它模块耦合

- 依赖：`w25qxx`（Flash 驱动）。
- 被调用：NFC（黑名单）、display（配置/记录）、freertos（心跳 PEND）。

## 5. 注意事项

- **结构体布局不可改**（Flash 持久化兼容）。
- 配置双区写入顺序：先备份后主区。
- `Record_FixAllTimeOffsets` 需在时间同步锁内执行，禁止并发刷卡/串口。
- 记录区擦写寿命有限，循环写已均摊。
