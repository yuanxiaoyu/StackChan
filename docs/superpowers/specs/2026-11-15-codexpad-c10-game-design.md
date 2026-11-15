# CodexPad C10 游戏适配

用户已确认：新建独立分支，以游戏为主要用途，沿用 Setup → Apps → Gamepad 连接入口；支持方向、A/B、Start/Select、暂停菜单和断线重连。

## 连接与输入

保持官方 ESP-IDF 6.1 / Mooncake / HAL 架构，复用现有 NimBLE central 与手机 BLE peripheral 的同一个 host。C10 不是 HID：使用 FFA0 服务、FFA1 通知和严格的 8 字节 `<IBBBB` 状态（32 位小端按键位图与四个 8 位轴）。实现依据官方公开协议，不引入 Arduino、Python 运行时或第二个 BLE host。

摇杆按 64/191 边界映射，保留斜向；C10 左摇杆 Y 值增大对应上，减小对应下，X 值减小对应左、增大对应右。通用 HID 保留自身的轴方向。A/B/Start 保持原意。C10 只有 A、B、X、Y、Start 五个实体键，将 X 映射 Select，Y 映射暂停菜单。也兼容协议中原有 Select/Home 位。按下与松开都使用完整状态快照；异常长度释放按键，防止粘键；不把无变化通知间隔当作断线。

扫描识别 CodexPad 名称、FFA0 广播服务或带 `0xffff` 厂商号与 `CodexPad` 标识的厂商数据，兼容名称在扫描响应中的情况。沿用手动选择、NVS 记忆、开机直连和断开后重试。

## GAME 模式

GAME 启动前读取保存的手柄信息。存在已保存的蓝牙手柄或 BLE host 已启动时，只启动/复用 BLE，不自动启动 ESP-NOW；避免两套无线栈同时消耗内部 RAM。无保存手柄且 BLE 未启动时保留原遥控器的默认行为。蓝牙游戏模式中的 Remote 按钮显示为蓝牙连接提示，不在游戏中叠加另一套无线栈。

模拟器任务直接轮询 BLE 快照，UI 与游戏共享相同输入源。BLE 断开立即释放输入并打开暂停菜单；重连不会自动继续。原 ESP-NOW 的掉线宽限保留。GAME 不关闭或改写 AI/手机绑定配置。

当前基线把音频驱动改为 16 kHz，但游戏 APU 仍硬编码 24 kHz。游戏 APU 应使用实际输出采样率，避免蓝牙游戏中音高/节奏错误。

## 检查与交付

回归覆盖全部 C10 键、轴边界/斜向/松开、无效包、广播身份、BLE/ESP-NOW 启动选择和 BLE 断开行为。运行仓库宿主 CTest 与完整 ESP-IDF 构建。实机检查扫描、连接、游戏列表、古巴战士输入、X/Select、Y/菜单、断开暂停及重连；没有 C10 时明确标注未验收，不将编译成功视为实机兼容。

## 依据

- https://github.com/CodexPad/codex_pad_c10
- https://github.com/CodexPad/codex_pad_bleak_lib/blob/main/codex_pad.py （参考版本 2.0.0；不复制库代码）
- https://github.com/CodexPad/codex_pad_c10/blob/main/assets/images/zh-CN/functional_illustration.jpg
