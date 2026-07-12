# NFC考勤系统上位机

这是一个 C++/Qt Widgets 上位机示例工程，协议与《2026专业实践综合设计II——NFC考勤系统设计》中的上位机阶段一要求对齐。

- 自动列出当前主机串口，并优先选择常见 USB 串口设备。
- 支持选择串口号、波特率，显示串口接收区。
- 支持 CID、8位学号、PTS积分、姓名、部门、卡类型录入。
- 卡类型支持普通卡、图像卡、管理员卡。
- 支持 `READ` 读卡并解析 `UID/SID/PTS/TYPE`。
- 支持 `ISSUE` 写账户头。
- 支持头像缩放为 48x64、灰度化、二值化并预览。
- 支持姓名、部门渲染为 80x16 单色位图并预览。
- 支持按块发送 `IMGA00~23`、`IMGN00~09`、`IMGD00~09`，再发送 `UPDATEIMG`。
- 普通卡和管理员卡也会写入姓名/部门位图；头像块发送空白图像。图像卡会写入真实头像。
- 支持 `CLEAR:UID` 清卡。
- 支持 `LIST:ALL` 和 `LIST:N` 查询考勤记录。
- 发卡记录会保存到程序目录下的 `issue_records.csv`。
- 发卡时会把头像、姓名图像、部门图像保存到程序目录的 `issued_cards` 文件夹；读卡得到 UID 后，上位机会从本地记录恢复姓名、部门和图片预览。

## 串口协议

协议采用纯 ASCII 英文命令文本帧，每条命令以 `\n` 结束。

### 读卡

```text
READ
```

下位机响应：

```text
UID:XXXXXXXX
SID:XXXXX
PTS:XXXX
TYPE:X
```

### 发卡账户头

```text
ISSUE:CID_HEX,SID_DEC,PTS,CTYPE
```

`CTYPE`：0=普通卡，1=图像卡，2=管理员卡。
本工程把 `SID_DEC` 作为 8 位学号处理，例如 `19195227`。

### 图像卡数据

```text
IMGA00:HEX32
...
IMGA23:HEX32
IMGN00:HEX32
...
IMGN09:HEX32
IMGD00:HEX32
...
IMGD09:HEX32
UPDATEIMG
```

头像为 48x64 单色位图，共 384 字节，分 24 块发送。姓名和部门各为 80x16 单色位图，各 160 字节，分别分 10 块发送。

### 清卡

```text
CLEAR:UID_HEX
```

### 查询记录

```text
LIST:ALL
LIST:N
```

下位机记录格式：

```text
REC:SEQ=N|UID=XXXXXXXX|SID=XXXXX|TYPE|YYYY-MM-DD HH:MM:SS|DEV=N|STATUS
```

如果下位机返回 `ERR:UNKNOWN_CMD`，说明当前固件没有实现 `LIST:ALL` 或 `LIST:N`。本上位机会在 `LIST:ALL` 失败后自动尝试一次 `LIST`，但最终仍需以下位机实际支持的命令为准。

### 读取卡内图像块

为了让没有本地发卡记录的卡片也能恢复头像、姓名和部门，上位机在读到图像卡或本地找不到 UID 记录时，会发送：

```text
READIMG
```

下位机需要按卡片扇区映射读取图像块，并回传与写卡相同的块格式：

```text
IMGA00:HEX32
...
IMGA23:HEX32
IMGN00:HEX32
...
IMGN09:HEX32
IMGD00:HEX32
...
IMGD09:HEX32
IMG:END
```

块到扇区映射：

- 头像 24 块：扇区 1~8，每扇区块 0~2。
- 姓名 10 块：扇区 9~11，每扇区块 0~2，加扇区 12 块 0。
- 部门 10 块：扇区 12 块 1~2，加扇区 13~14 每扇区块 0~2，加扇区 15 块 0~1。

## 编译方式

1. 用 Qt Creator 打开 `NfcAttendanceHost.pro`。
2. 确认 Qt 安装包含 `Qt Serial Port` 模块。
3. 选择 MinGW 或 MSVC Kit。
4. 构建并运行。

## 演示建议

至少演示 5 次发卡，普通卡和图像卡交替进行。图像卡头像建议使用真实人脸照片，并尽量保证边缘完整、五官清晰。
