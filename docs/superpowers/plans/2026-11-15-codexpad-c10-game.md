# CodexPad C10 游戏适配实施计划

**目标：** 在 `codex/codexpad-c10-game` 上实现已确认的 Setup 蓝牙配对与 GAME 使用路径。

**设计：** [适配设计](../specs/2026-11-15-codexpad-c10-game-design.md)。复用 NimBLE，单独封装 C10 协议，GAME 根据保存手柄或现有 BLE host 选择无线输入。

**约束：** 保持官方 ESP-IDF 6.1、Mooncake、HAL；不提交密钥、ROM、设备备份或构建产物；不增加其他 BLE 库；自行执行，不委派子 Agent。

- [ ] 协议：在 `firmware/tests/codexpad_test.cpp` 先复现 X/Y 缺失、9 字节误接收和广播身份边界，再实现 `hal/gamepad/codexpad_protocol.{h,cpp}`；原 HID 回归保留。
- [ ] 路由：在 `firmware/tests/game_input_test.cpp` 覆盖已绑定但关机的 BLE 手柄仍应选择 BLE、未绑定的原遥控器路径及掉线暂停；实现 `game_input.h` 并接入 GAME，快照暴露保存状态和 BLE 初始化状态。
- [ ] BLE：扫描识别 C10 名称/厂商/FFA0，沿用 FFA1 订阅和自动重连；无效通知清空输入。Setup 显示连接状态和按键，便于实机确认。
- [ ] 游戏：使用实际 codec 采样率初始化 APU；检查列表、暂停、Select、Start 全路径。
- [ ] 验证：宿主 CMake/CTest、完整固件构建、补丁反向检查与 diff 检查；更新游戏说明，实机条件允许时测试并记录限制。
