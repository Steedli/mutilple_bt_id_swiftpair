# 多蓝牙身份 HID 键盘（支持 Swift Pair）

一个支持多个蓝牙身份的低功耗蓝牙（BLE）HID 键盘实现，允许同一设备与多个主机配对或在不同的设备配置之间切换。

## 功能特性

- **多蓝牙身份**：支持 3 个独立的蓝牙身份（bt_id 0、1、2）
- **微软 Swift Pair**：Windows 10/11 快速配对支持
- **自动回连**：使用隐私功能（RPA）自动回连到已配对的主机
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

- **nRF Connect SDK**：v3.2.0 或更高版本
- **Zephyr OS**：v4.2.99 或更高版本
- **工具链**：随 nRF Connect SDK 提供
- **VS Code**：安装 nRF Connect 扩展（推荐）

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
| **按键 3** | 切换身份 | 切换到下一个蓝牙身份（0→1→2→0）|
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
1. 上电 → 使用 bt_id 0
2. 与 PC-1（办公室）配对
3. 按按键 3 → 切换到 bt_id 1
4. 与 PC-2（家里）配对
5. 按按键 3 → 切换到 bt_id 2
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
CONFIG_BT_ID_MAX=3                    # 身份数量
CONFIG_BT_MAX_PAIRED=6                # 最大配对设备数（每个身份 2 个）

# 隐私功能实现自动回连
CONFIG_BT_PRIVACY=y                   # 启用 RPA 以实现自动回连
CONFIG_BT_RPA_TIMEOUT=900             # RPA 超时时间（15 分钟）

# 设置存储
CONFIG_BT_SETTINGS=y                  # 将绑定信息保存到 Flash
CONFIG_SETTINGS=y
CONFIG_FLASH=y
```

### 自定义

**更改设备名称**：
```ini
CONFIG_BT_DEVICE_NAME="您的设备名称"
```

**调整身份数量**：
```c
// 在 main.c 中
#define NUM_BT_IDS 3  // 更改为所需数量（1-3）
```

```ini
// 在 prj.conf 中
CONFIG_BT_ID_MAX=3    // 必须与 NUM_BT_IDS 匹配
```

## 故障排除

### 复位后设备不回连

**症状**：设备广播但不自动回连到 Windows

**解决方案**：
- 确认 prj.conf 中已启用 `CONFIG_BT_PRIVACY=y`
- 在 Windows 上，配对后首次回连需手动点击"连接"
- 检查设备身份地址是否一致（在日志中显示）

### 广播失败（错误 -12）

**症状**：日志显示 `Advertising failed to start (err -12)`

**解决方案**：
- 这在连接时尝试广播会发生
- 等待断开连接完成后再切换身份
- 代码已自动处理，有 500ms 延迟

### 切换身份后无法配对

**症状**："Refusing new pairing" 错误

**解决方案**：
- 启用隐私功能后，Windows 通过 IRK 识别设备
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

每个身份具有：
- **唯一的随机静态地址**：存储在 Flash 中
- **独立的 IRK（身份解析密钥）**：用于隐私
- **独立的绑定存储**：每个身份最多 2 个设备
- **独立的广播**：可以使用任何身份进行广播

### 隐私功能与回连

启用 `CONFIG_BT_PRIVACY=y` 后：
- 设备使用**可解析私有地址（RPA）**
- RPA 每 15 分钟更换一次（可配置）
- Windows 使用存储的 IRK 识别设备
- 实现自动回连

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
1. 遍历 0 到 NUM_BT_IDS-1
2. 检查当前身份是否已创建
3. 若未创建：调用 `bt_id_create()` 生成新身份
4. 若已存在：从 settings 中恢复
5. 打印所有身份的地址信息

---

#### `advertising_start()`
```c
static void advertising_start(void)
```
**功能**：使用当前蓝牙身份开始 BLE 广播

**详细说明**：
- 获取并显示当前身份的本地地址
- 检查当前身份是否有已绑定的设备
- 配置广播参数（使用快速广播间隔）
- 包含 Swift Pair 信标数据
- 启动广播并更新 `is_adv` 状态

**广播参数**：
- 间隔：BT_GAP_ADV_FAST_INT_MIN_1 ~ BT_GAP_ADV_FAST_INT_MAX_1 (30-60ms)
- 选项：BT_LE_ADV_OPT_CONN（可连接）
- 广播数据：设备名称、外观、Swift Pair 信标
- 扫描响应数据：完整设备名称、服务 UUID

**调用时机**：
- 应用启动后
- 断开连接后（如果未禁用 NFC OOB）
- 切换身份后

---

#### `switch_bt_id()`
```c
static void switch_bt_id(void)
```
**功能**：切换到下一个蓝牙身份

**详细说明**：
- 停止当前广播（如果正在广播）
- 断开所有现有连接
- 等待 500ms 确保清理完成
- 循环切换到下一个身份（0→1→2→0）
- 使用新身份重新开始广播

**状态转换**：
```
当前状态        操作              新状态
-------------------------------------------
广播中    → 停止广播      → 闲置
连接中    → 断开连接      → 闲置
闲置      → 切换 bt_id   → 广播新身份
```

**注意事项**：
- 切换期间会短暂中断服务（约 500ms）
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
1. 检查连接是否成功建立
2. 打印连接的设备地址和当前 bt_id
3. 更新绑定列表，检查是否为已绑定设备
4. 通知 HIDS 服务连接已建立
5. 将连接保存到 `conn_mode[]` 数组
6. 点亮连接状态 LED
7. 如果还有空闲连接槽，继续广播

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
6. 延迟 100ms 后重新开始广播

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

- **v1.0**（2025年12月）：初始实现
  - 多蓝牙身份支持
  - Swift Pair 集成
  - 使用隐私功能自动回连
  - 按身份管理绑定信息
