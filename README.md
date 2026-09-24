# MTP Path Traveler

（全文全代码全部DeepSeek编写）

一个跨平台的命令行小工具，用于与 MTP（Media Transfer Protocol，

媒体传输协议）设备交互。支持枚举设备与存储卷、列出文件夹内容、
上传文件到设备上的指定路径。

> (本文件可能和**YD部分型号词典笔**搭配产生特殊效果，详细使用教程请参照 **YD使用说明.md**)


本工具面向**安全研究与互操作性测试**。它演示了当 MTP 客户端与
MTP 服务端对路径处理的理解不一致时，可能产生的意外行为。

---

## 背景

MTP 是大多数 Android 设备以及不少嵌入式 Linux 设备用来通过 USB
暴露文件存储的协议。文件通过三个字段定位：

- **Storage ID**（存储卷编号）
- **Parent Object Handle**（父对象句柄）
- **Filename**（文件名）

MTP 规范**并未强制要求**服务端如何对 filename 字段做净化处理，
各家实现差异很大。

一个客户端如果把含有路径分隔符（`/`、`\`）或相对路径片段（`..`）
的文件名原样转发给设备，就等于**把是否过滤的责任交给了设备**。
有的设备会过滤；有的不会。

本工具是 `libmtp` 的一层薄封装，让你**直接控制上述字段**，
以便观察特定设备在边界情况下的实际行为。

> **请仅对你自己拥有、或已获得明确授权的设备使用本工具。**

---

## 功能

- 枚举当前连接的 MTP 设备
- 列出存储卷（含容量与剩余空间）
- 递归列出文件夹内容
- 上传文件到设备上的任意目标路径
- 同时支持十进制与十六进制编号

---

## 编译

### Linux

```bash
sudo apt install libmtp-dev libusb-1.0-0-dev gcc pkg-config
./build_linux.sh
```

### Windows（MSYS2 MINGW64）

MSYS2 官方源里没有 `libmtp` 包，需要先从源码编译它。

```bash
# 1. 安装编译依赖
pacman -S --needed mingw-w64-x86_64-gcc \
                   mingw-w64-x86_64-libusb \
                   mingw-w64-x86_64-libiconv \
                   pkg-config make autoconf automake libtool

# 2. 下载 libmtp 源码
curl -L -o libmtp.tar.gz \
  "https://mirrors.lzu.edu.cn/ubuntu/pool/main/libm/libmtp/libmtp_1.1.21.orig.tar.gz"
tar -xzf libmtp.tar.gz
cd libmtp-1.1.21

# 3. 生成 configure 脚本
./autogen.sh
# 出现 autoupdate 询问时，回答：n

# 4. 修补 MinGW 下缺失的头文件
sed -i '1i #include <winsock2.h>' src/gphoto2-endian.h

# 5. 配置并编译
./configure --host=x86_64-w64-mingw32 --disable-shared --enable-static
make -j$(nproc)

# 6. 回到项目目录，编译本工具
cd ..
./build_msys2.sh
```

生成的 `mtp_traveler.exe` 是**完全静态链接**的，运行时不依赖任何 DLL。

### Windows 上的驱动配置

`libusb` **无法访问**绑定在 Windows 标准 WPD 驱动上的设备。你需要
使用 [Zadig](https://zadig.akeo.ie/) 把设备重新绑定到 **WinUSB**：

1. 打开 Zadig，勾选 `Options → List All Devices`
2. 在下拉框里选择目标设备（核对 VID / PID）
3. 目标驱动选 **WinUSB**
4. 点击 **Replace Driver**

恢复方法：打开「设备管理器」→ 右键设备 → **回滚驱动程序**
（或卸载后重新插拔）。

---

## 使用方法

```
mtp_traveler list-devices
mtp_traveler list-storage
mtp_traveler list-folder <storage_id> <parent_id> [depth]
mtp_traveler put <local_file> <parent_id> <target_path> [storage_id]
```

### 命令说明

| 命令 | 作用 |
|---|---|
| `list-devices` | 列出已连接的 MTP 设备 |
| `list-storage` | 列出第一个设备的存储卷 |
| `list-folder` | 列出指定文件夹内容（可递归） |
| `put` | 上传文件到指定目标路径 |

### 参数说明

- `<local_file>`：本地待上传文件
- `<parent_id>`：目标文件夹的 ID（可通过 `list-folder` 查看），
  支持十进制或十六进制（`0x...`）
- `<target_path>`：在设备上创建的文件名，**其中的路径分隔符会
  原样转发给设备**
- `[storage_id]`：可选，存储卷 ID（省略则自动探测）
- `[depth]`：可选，`list-folder` 的递归深度（默认 2）

---

## 使用示例

### 查看设备与存储卷

```bash
mtp_traveler list-devices
mtp_traveler list-storage
```

### 浏览文件夹树

```bash
mtp_traveler list-folder 3 0 3
```

### 上传普通文件

```bash
echo hello > sample.txt
mtp_traveler put sample.txt 1 "hello.txt"
```

### 使用带子目录的目标路径

```bash
mtp_traveler put sample.txt 1 "reports/2026/summary.txt"
```

### 观察设备如何处理特殊路径片段

```bash
mtp_traveler put sample.txt 1 "../escape.txt"
```

> 最后一个例子是**从安全角度最值得关注**的用法。
> 在行为规范的设备上，文件会落在目标文件夹内部；
> 在实现存在问题的设备上，文件可能被创建到预期范围之外。

---

## 输出说明

`mtp_traveler` 在尝试上传前，会先打印它**即将发送给设备的完整
字段值**（storage ID、parent ID、目标路径），方便你把客户端输入
和设备端行为一一对应。

上传成功时输出：

```
OK: file created on device.
```

上传失败时，会打印 `libmtp` 返回的错误码，并调用
`LIBMTP_Dump_Errorstack` 输出完整错误栈。

---

## 平台说明

| 平台 | 说明 |
|---|---|
| Linux / macOS | 安装标准 `libmtp` 包即可编译运行 |
| Windows | 需要用 MSYS2 编译，且目标设备需绑定 WinUSB 驱动 |
| WSL2 | 需通过 `usbipd-win` 把 USB 设备转发进 WSL |

---

## 项目结构

```
mtp-path-traveler/
├── src/
│   └── mtp_traveler.c     主程序源码
├── build_linux.sh         Linux 编译脚本
├── build_msys2.sh         MSYS2 编译脚本
├── run.bat                Windows 交互式菜单（可选）
├── README.md              本文件
├── LICENSE                MIT 许可证
└── .gitignore
```

---

## 免责声明

本工具**仅供安全研究与互操作性测试使用**。

- 请仅对**你本人拥有**、或**已获得设备所有者明确授权**的设备使用。
- 请勿用于未经授权的访问、数据窃取或任何非法用途。
- 使用者需自行承担因使用本工具所产生的一切后果。

作者不对任何滥用行为负责。

---

## 许可证

MIT。详见 `LICENSE`。