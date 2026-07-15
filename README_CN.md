# 多蓝牙身份 HID 键盘（支持 Swift Pair）

一个支持多个蓝牙身份的低功耗蓝牙（BLE）HID 键盘实现，允许同一设备与多个主机配对或在不同的设备配置之间切换。

## 功能特性

- **多蓝牙身份**：支持 3 个独立的蓝牙身份（bt_id 1、2、3；bt_id 0 是协议栈自动创建的默认身份，本项目不使用）
- **微软 Swift Pair**：Windows 10/11 快速配对支持
- **定向广播**：优先尝试对已绑定设备做高频定向广播，失败/超时后再回退到常规 Swift Pair 广播
- **自动回连**：使用固定的身份地址自动回连到已配对的主机（已关闭 RPA/隐私功能，详见下方[RPA / 身份地址](#rpa--身份地址)）
- **HID 键盘**：标准 USB HID 键盘协议支持
- **绑定管理**：每个身份独立的绑定存储和管理
- **电池服务**：BLE 电池服务（模拟）
- **多主机支持**：每个身份可以与不同的主机配对

## 硬件要求

- **支持的开发板**：
  - nRF54H20 DK 应用核心 (nrf54h20dk_nrf54h20_cpuapp)
  - nRF54H20 DK 射频核心 (nrf54h20dk_nrf54h20_cpurad)
  - nRF54L15 DK (nrf54l15dk_nrf54l15_cpuapp)
  - nRF54LM20 DK (nrf54lm20dk_nrf54lm20a_cpuapp)
  - 其他兼容 nRF Connect SDK 的开发板

- **外设**：
  - 4 个按键（DK 板载按键 1-4）
  - 4 个 LED（DK 板载 LED 1-4）

## 软件要求

- **nRF Connect SDK**：v3.2.0 或更高版本（已在 v3.3.1 上验证）
- **Zephyr OS**：v4.2.99 或更高版本
- **工具链**：随 nRF Connect SDK 提供
- **VS Code**：安装 nRF Connect 扩展（推荐）

> 已在 **nRF54L15 DK** + NCS v3.3.1 上完成编译、烧录及回连测试。

## 编译和烧录

### 使用 VS Code 和 nRF Connect 扩展

1. 在 VS Code 中打开项目文件夹
2. 在 nRF Connect 面板中选择目标开发板
3. 点击 **Build** 按钮
4. 点击 **Flash** 按钮

### 使用命令行

```bash
# 进入项目目录
cd d:\workspace\mutilple_bt_id_swiftpair

# 为您的开发板编译
west build -b nrf54h20dk/nrf54h20/cpurad

# 烧录到设备
west flash
```

## 使用指南

### 初始设置

1. **设备上电** - LED1 开始闪烁（广播中）
2. **在 Windows PC 上**：
   - 打开 设置 → 蓝牙和其他设备
   - 点击"添加设备" → 蓝牙
   - 从列表中选择"swift"
   - 如有提示，按照配对说明操作
3. **连接建立** - LED2 常亮

### 按键功能

| 按键 | 功能 | 描述 |
|------|------|------|
| **按键 1** | 文本输入 | 循环发送 "hello\n" 字符 |
| **按键 2** | Shift 键 | 作为 Shift 修饰键 |
| **按键 3** | 切换身份 | 切换到下一个蓝牙身份（1→2→3→1）|
| **按键 4** | 删除绑定 | 删除当前身份的绑定信息 |

### LED 指示灯

| LED | 状态 | 含义 |
|-----|------|------|
| **LED 1** | 闪烁 | 广播中 / 等待连接 |
| **LED 1** | 熄灭 | 已连接 |
| **LED 2** | 常亮 | 设备已连接 |
| **LED 2** | 熄灭 | 设备已断开 |
| **LED 3** | 常亮 | 大写锁定已激活 |
| **LED 3** | 熄灭 | 大写锁定未激活 |

### 多身份工作流程

#### 场景 1：每个身份对应不同的 PC

```
1. 上电 → 使用 bt_id 1
2. 与 PC-1（办公室）配对
3. 按按键 3 → 切换到 bt_id 2
4. 与 PC-2（家里）配对
5. 按按键 3 → 切换到 bt_id 3
6. 与 PC-3（笔记本）配对

现在您可以：
- 复位设备 → 自动回连到上次使用的身份
- 在身份之间切换以连接到不同的 PC
```

#### 场景 2：自动回连

```
1. 与 PC 配对
2. 复位设备 → 自动回连 ✓
3. 关闭 PC 蓝牙 → 设备开始广播
4. 打开 PC 蓝牙 → 自动回连 ✓
```

## 配置说明

### 关键配置选项（`prj.conf`）

```ini
# 蓝牙身份支持
CONFIG_BT_ID_MAX=4                    # bt_id 0（默认，不使用）+ 3 个可用身份
CONFIG_BT_MAX_PAIRED=6                # 最大配对设备数（每个身份 2 个）

# 关闭 RPA（隐私功能）——原因见下方"RPA / 身份地址"
CONFIG_BT_PRIVACY=n

# 设置存储
CONFIG_BT_SETTINGS=y                  # 将绑定信息保存到 Flash
CONFIG_SETTINGS=y
CONFIG_FLASH=y
```

```ini
# Kconfig（本工程自定义选项，见 Kconfig 文件）
CONFIG_BT_DIRECTED_ADVERTISING=y      # 回退到 Swift Pair 广播前先尝试定向广播
```

### 自定义

**更改设备名称**：
```ini
CONFIG_BT_DEVICE_NAME="您的设备名称"
```

**调整身份数量**：
```c
// 在 main.c 中
#define NUM_BT_IDS 3  // 更改为所需的可用身份数量（1-3）；bt_id 0 始终不使用
```

```ini
// 在 prj.conf 中
CONFIG_BT_ID_MAX=4    // 必须等于 NUM_BT_IDS + 1（因为 bt_id 0 也会被自动创建）
```

## 故障排除

### 复位后设备不回连

**症状**：设备广播但不自动回连到 Windows

**解决方案**：
- 检查日志中打印的身份地址（`Local address for bt_id N: ...`）在每次复位后是否保持一致——由于已关闭 RPA/隐私功能，该地址是固定的，不应发生变化
- 在 Windows 上，配对后首次回连需手动点击"连接"
- 确认开机日志中 `Found N bonded device(s) for bt_id N: ...` 里能看到对端地址

### 定向广播启动失败（错误 -22）

**症状**：日志出现 `Directed advertising failed to start (err -22), retrying shortly`，有时紧跟在 `Directed advertising to ... timed out` 之后

**解决方案**：
- 这是预期内的瞬时现象，会自动恢复。蓝牙host栈会在`connected()`/`disconnected()`回调返回之后才真正释放上一次连接对象的最后一个引用，所以紧接着对同一地址重试定向广播时会短暂返回`-EINVAL`。代码已自动在100ms后重试（`dir_adv_start_retry_work`），除非持续失败很久，否则无需干预。

### 广播失败（错误 -12）

**症状**：日志显示 `Advertising failed to start (err -12)`

**解决方案**：
- 这在设备仍处于连接状态时尝试广播会发生
- 现在切换身份（按键 3）会等待真正的`disconnected()`回调后才重启广播（事件驱动，而不是固定延迟），正常切换身份流程下不应再出现此问题

### 切换身份后无法配对

**症状**："Refusing new pairing" 错误

**解决方案**：
- 每个bt_id都有各自固定的身份地址和独立的绑定存储，同一台PC在同一时刻只会与其中一个bt_id绑定
- 要将不同身份与同一 PC 配对：
  1. 切换身份（按键 3）
  2. 在 Windows 上：删除/取消配对设备
  3. 重新添加设备

### 绑定存储已满

**症状**：多次绑定后配对失败

**解决方案**：
- 使用按键 4 删除当前身份的绑定
- 或修改 prj.conf 中的 `CONFIG_BT_MAX_PAIRED` 值

## 技术细节

### 蓝牙身份系统

每个身份（bt_id 1、2、3；bt_id 0 是默认身份，不使用）具有：
- **唯一的随机静态地址**：固定不变，存储在 Flash 中
- **独立的绑定存储**：每个身份最多 2 个设备
- **独立的广播**：可以使用任何身份进行广播

### RPA / 身份地址

`CONFIG_BT_PRIVACY=n`：设备广播时使用每个身份**固定的随机静态地址**，而不是轮换的可解析私有地址（RPA）；无论是定向广播还是常规Swift Pair广播，每次`bt_le_adv_start()`都会带上`BT_LE_ADV_OPT_USE_IDENTITY`选项。

**为什么关闭RPA**：实测发现部分蓝牙控制器（在某些Razer笔记本上观察到）无法通过IRK解析本设备的RPA地址——Scan Response会显示为"Unknown BD_ADDR"，主机因此拒绝回连。改用固定的身份地址广播后，不再依赖对端的IRK解析数据库。代价是设备地址不再为隐私目的而轮换，但由于本设备本身是固定外设，且各身份的绑定信息本就互相隔离，这个取舍是可以接受的。

### 定向广播

`CONFIG_BT_DIRECTED_ADVERTISING=y`（默认开启）：在回退到常规Swift Pair广播之前，设备会先尝试对**当前身份**的已绑定设备做快速、高频的定向广播（`BT_LE_ADV_CONN_DIR`）：

1. 调用`advertising_start()`时，会把当前bt_id的已绑定地址快照进队列（`dir_adv_bonds_queue`），并跳过`conn_mode[]`里仍显示为已连接的对端。
2. 每个候选地址最多重试3次（`DIR_ADV_MAX_RETRIES`）；每次尝试都是高频定向广播，按蓝牙规范约1.28秒后超时（通过`connected()`回调、`err=BT_HCI_ERR_ADV_TIMEOUT`上报）。
3. 如果`bt_le_adv_start()`本身返回`-EINVAL`（见[故障排除](#定向广播启动失败错误--22)），会在100ms后重试，而不是当作硬失败处理。
4. 30秒的软件兜底定时器（`directed_adv_timeout_work`）确保即使出现其他异常，设备最终也会回退到常规广播。
5. 所有候选地址都尝试完（或本来就没有候选）后，回退到常规Swift Pair广播。

**身份切换与定向广播的关系**：切换bt_id（按键3）时，绝不能在上一个连接还没真正断开完成前就重建定向广播候选队列，否则本来想回连的那个对端会被误判为"仍处于连接中"而被过滤掉。因此`switch_bt_id()`不再使用固定延迟——如果当前有活动连接，会先请求断开并设置`pending_id_switch`标志，真正的身份切换与广播重启会推迟到`disconnected()`回调里、确认`conn_mode[]`已清空之后才执行。

### 微软 Swift Pair

Swift Pair 信标包含：
- 微软供应商 ID：0x0006
- 信标 ID：0x03
- 设备名称："swift"
- 在 Windows 10/11 中显示为快速配对通知

### 绑定管理

绑定信息存储在 Flash 中：
- 设备复位后保持
- 按身份存储
- 可单独或全部擦除

## API 参考

### 关键函数

#### `create_bt_ids()`
```c
static void create_bt_ids(void)
```
**功能**：创建和初始化多个蓝牙身份

**详细说明**：
- 检查每个身份是否已存在（从 Flash 中恢复）
- 如果身份不存在，创建新的随机静态地址和 IRK
- 显示每个身份的地址信息
- 在应用启动时调用一次

**工作流程**：
1. 遍历 bt_id 1 到 NUM_BT_IDS（bt_id 0 是默认身份，不做处理）
2. 检查当前身份是否已创建
3. 若未创建：调用 `bt_id_create()` 生成新身份
4. 若已存在：从 settings 中恢复
5. 打印身份 1..NUM_BT_IDS 的地址信息

---

#### `advertising_start()` / `advertising_continue()`
```c
static void advertising_start(void)
static void advertising_continue(void)
```
**功能**：使用当前蓝牙身份开始 BLE 广播，优先尝试对已绑定设备做定向广播

**详细说明**：
- `advertising_start()` 为当前bt_id重建定向广播候选队列（`dir_adv_bonds_queue`，跳过已连接的对端）并重置重试计数，然后调用 `advertising_continue()`
- `advertising_continue()` 获取并显示当前身份的本地地址，然后：
  - 若有可用的定向广播候选（新的或当前正在重试的），发起高频定向广播（`BT_LE_ADV_CONN_DIR` + `BT_LE_ADV_OPT_USE_IDENTITY`），并启动30秒兜底定时器
  - 否则，发起带Swift Pair信标的常规广播（`BT_LE_ADV_OPT_CONN` + `BT_LE_ADV_OPT_USE_IDENTITY`，间隔`BT_GAP_ADV_FAST_INT_MIN_1`~`MAX_1`，30-60ms）
- 定向广播的超时/重试工作项也会直接重入 `advertising_continue()`（不经过队列重建这一步），以免打断正在进行中的重试序列

**调用时机**：
- 应用启动后
- 断开连接后（如果未禁用 NFC OOB，且没有待处理的bt_id切换）
- bt_id切换完成后（通过`disconnected()`触发，见下方`switch_bt_id()`）
- 由定向广播的重试/超时工作项间接调用

---

#### `switch_bt_id()` / `switch_bt_id_continue()`
```c
static void switch_bt_id(void)
static void switch_bt_id_continue(void)
```
**功能**：切换到下一个蓝牙身份

**详细说明**：
- `switch_bt_id()` 停止当前广播（如果正在广播），并取消定向广播相关的工作项
- 如果当前没有活动连接，直接调用 `switch_bt_id_continue()`
- 如果有活动连接，设置 `pending_id_switch = true`，请求断开连接后直接返回——**不使用固定延迟**
- `disconnected()` 会检查 `pending_id_switch`；一旦确认 `conn_mode[]` 已清空，就清除该标志并调用 `switch_bt_id_continue()`
- `switch_bt_id_continue()` 循环切换到下一个身份（1→2→3→1），并调用 `advertising_start()`

**为什么改成事件驱动而不是固定延迟**：如果在上一个连接还没真正断开完成前就重启广播（并重建定向广播候选队列），`dir_adv_bond_find()` 会把这个对端误判为"仍处于连接中"而过滤掉，导致设备退化成慢速的常规广播而不是快速定向广播。等待真正的 `disconnected()` 事件，无论断连实际耗时多久，都能避免这个竞态。

**状态转换**：
```
当前状态                操作                       新状态
--------------------------------------------------------------
广播中          → 停止广播                    → 闲置
连接中          → 请求断开，设置标志           → 等待 disconnected()
disconnected()且标志已设置 → conn_mode[]已清空  → 切换bt_id，广播新身份
```

**注意事项**：
- 切换期间的服务中断时长取决于实际断连耗时（不再是固定延迟）
- 已配对的设备需要重新连接
- 绑定信息按身份独立保存

---

#### `delete_bond_for_current_id()`
```c
static void delete_bond_for_current_id(void)
```
**功能**：删除当前身份的一个绑定设备

**详细说明**：
- 枚举当前身份的所有绑定设备
- 使用循环索引选择要删除的绑定
- 调用 `bt_unpair()` 删除选定的绑定
- 自动移动到下一个索引，多次按键可删除多个绑定

**使用场景**：
- 绑定存储已满，需要腾出空间
- 不再使用某个已配对的主机
- 解决配对冲突问题

**示例**：
```
当前身份有 2 个绑定：[PC-1, PC-2]
第 1 次按键 → 删除 PC-1
第 2 次按键 → 删除 PC-2
第 3 次按键 → 无绑定可删除
```

### 连接回调函数

#### `connected()`
```c
static void connected(struct bt_conn *conn, uint8_t err)
```
**功能**：设备连接建立时的回调

**参数**：
- `conn`：连接句柄
- `err`：错误码（0 表示成功）

**执行流程**：
1. 若 `err == BT_HCI_ERR_ADV_TIMEOUT`（对该对端的定向广播超时）：取消定向广播相关工作项，清除 `is_adv`，直接调用 `advertising_continue()` 重试下一个候选或回退广播，然后返回
2. 若 `err` 为其他失败：打印日志后返回
3. 连接成功：取消定向广播相关工作项，打印连接的设备地址和当前 bt_id
4. 更新绑定列表，检查是否为已绑定设备
5. 通知 HIDS 服务连接已建立
6. 将连接保存到 `conn_mode[]` 数组
7. 点亮连接状态 LED
8. 如果还有空闲连接槽，继续广播

**状态变化**：
- LED2 点亮（连接状态）
- 停止 LED1 闪烁（停止广播，如果所有槽已满）
- `is_adv` 设置为 false（如果停止广播）

---

#### `disconnected()`
```c
static void disconnected(struct bt_conn *conn, uint8_t reason)
```
**功能**：设备断开连接时的回调

**参数**：
- `conn`：断开的连接句柄
- `reason`：断开原因代码（如 0x13 = 远程用户终止连接）

**执行流程**：
1. 打印断开信息和原因
2. 通知 HIDS 服务连接已断开
3. 从 `conn_mode[]` 数组中移除连接
4. 检查是否还有其他活动连接
5. 如果无活动连接，熄灭连接 LED
6. 若 `pending_id_switch` 已设置且已无活动连接：清除该标志并调用 `switch_bt_id_continue()`（见 `switch_bt_id()`），然后返回
7. 否则，短暂延迟后重新开始广播

**常见断开原因**：
- `0x08`：连接超时
- `0x13`：远程用户终止连接
- `0x16`：本地主机终止连接
- `0x3d`：连接失败建立

---

#### `security_changed()`
```c
static void security_changed(struct bt_conn *conn, 
                            bt_security_t level,
                            enum bt_security_err err)
```
**功能**：安全级别变化时的回调

**参数**：
- `conn`：连接句柄
- `level`：新的安全级别（1-4）
- `err`：错误码（0 表示成功）

**安全级别**：
- **Level 1**：无安全（无加密，无认证）
- **Level 2**：加密，无 MITM 保护
- **Level 3**：加密 + MITM 保护
- **Level 4**：加密 + MITM + Secure Connections

**典型流程**：
```
连接建立 → Level 1（无安全）
  ↓
配对开始 → Level 2（加密）
  ↓
配对完成 → Level 3（加密 + MITM）
```

**日志示例**：
```
Security changed: E8:B0:C5:83:6D:EA (public) level 2
```

## 许可证

SPDX-License-Identifier: LicenseRef-Nordic-5-Clause

版权所有 (c) 2018-2025 Nordic Semiconductor ASA

## 支持

如有问题：
- 查看 [nRF Connect SDK 文档](https://docs.nordicsemi.com)
- 访问 [DevZone](https://devzone.nordicsemi.com)
- 通过串口终端查看日志（115200 8N1）

## 版本历史

- **v1.1**：移植到 NCS v3.3.1，在 nRF54L15 DK 上验证
  - 关闭 RPA/隐私功能（`CONFIG_BT_PRIVACY=n` + `BT_LE_ADV_OPT_USE_IDENTITY`），解决部分主机蓝牙控制器无法解析本设备RPA地址的兼容性问题
  - 新增定向广播支持（`CONFIG_BT_DIRECTED_ADVERTISING`），带每候选重试、30秒兜底定时器，以及针对`bt_le_adv_start()`瞬时`-EINVAL`的短延时重试
  - 可用身份编号改为 bt_id 1-3（bt_id 0 不使用）；`CONFIG_BT_ID_MAX` 提高到 4
  - 身份切换（`switch_bt_id()`）改为事件驱动而不是固定500ms延迟，修复了切换身份后定向广播被误跳过的竞态问题
- **v1.0**（2025年12月）：初始实现
  - 多蓝牙身份支持
  - Swift Pair 集成
  - 使用隐私功能自动回连
  - 按身份管理绑定信息
