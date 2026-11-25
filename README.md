# HX711 Web Scale / HX711 网页耗材秤

## 中文：当前接口与流程

本版将读数接口与耗材记录分离。设备负责采样、稳定判断、重量唯一匹配和保存记录；页面负责余量计算及显示。旧版 reading 字段不再提供，页面与固件需一起升级。

### 接线、安装与校准

ESP32-C3：HX711 DT 接 GPIO6、SCK 接 GPIO7；新建耗材按钮为 GPIO3 接 GND，内部上拉。GPIO4/5 预留 I2C，当前不驱动显示屏。安装 `esp32 by Espressif Systems`（已验证 3.3.11）和 `HX711 by bogde`，选择 `ESP32C3 Dev Module`，串口 115200。

首次使用进入 `/settings`，清空称台进行“空台归零”，然后放上已知砝码输入重量并校准。未校准不显示虚假克数。本次代码改造不会自动归零、校准或清空库存。

### `GET /api/reading`

响应严格只有四个字段：

```json
{"raw":135000,"state":1,"id":3,"value":1350.0}
```

| 字段 | 含义 |
|---|---|
| `raw` | 最近一次 HX711 原始整数；异常时可能是最后一次读数，不作为重量使用 |
| `state` | 0 下料，1 上料，99 未校准，98 传感器异常 |
| `id` | 已确认的耗材 ID；首次未稳定或未识别、确认下料、普通模式及未校准为 0；确认后波动或传感器异常保留 ID |
| `value` | 相对于空台归零点的总重量，单位克；只要有缓存 raw 且已校准就计算，包含 state=98；尚无读数或未校准时为 `null` |

98 优先于 99：尚无采样或传感器实际读取持续失败时报告98。主循环每轮等待HX711最多约2ms，仅累计实际等待，无读数累计3000ms才标记故障；HTTP处理时间不计入，缓存年龄和数字满量程均不再触发98。已缓存raw保持并用于计算value，98时该数值为历史重量。重量至少达到上料阈值才进入上料；阈值间保持此前上下料状态；连续约 0.6 秒达到空台范围确认下料。初始阈值间状态为下料。

稳定判断采用最近 10 点、3 g 内波动及约 1.5 秒持续稳定，仅用于首次确认及耗材使用量更新。首次未稳定时 `id=0`；一旦确认，重量波动及实际传感器读取失败均保留确认 ID，读取故障由 `state=98` 表达。有缓存且已校准时value仍为计算值。无新读数不参与卸料判定，读取恢复或长采样间隔后重新积累稳定窗口；确认下料、切换普通模式或重新校准时清除身份。异常或不稳定期间不更新耗材用量、不周期保存。

### 耗材记录、匹配和使用更新

`GET /api/spools` 返回数组，每条只包含以下五项：

```json
[{"id":3,"material":0,"color":"#808080","tare":350.0,"total":1350.0}]
```

`material`：0 PLA，1 PETG，2 ABS，3 TPU，4 其他；`tare` 为空盘/随卷支架重量，`total` 为该卷已保存的稳定总重量，均为克。最多 32 卷。总重不足以推算空盘重量的新记录，以及空盘未知的旧记录，使用 `tare=null` 标记待确认；页面不会把未知空盘当作零。

1. 首次上料尚未确认且未稳定时 `id=0`。稳定后仅比较总重量；容差内只有一条候选才自动确认，多条候选或没有候选均保持 0，不按“最近”强选。
2. `state=1,id=0` 时可新建或选取历史记录；设备会拒绝尚未稳定的操作，稍后手动重试即可。
3. 新建记录的总重量取稳定总重，空盘重量 = 稳定总重 − 设置中的新耗材初始净重。初始净重默认 1000 g；若结果为负数，仍然新增并立即确认 ID，空盘重量保存为待确认（`tare=null`），创建成功时提示总重不足，列表持续标记问题；填写正确空盘重量后解除标记并显示余量。其他新记录仍按差值推算空盘重量，可编辑材料、颜色、空盘重量。
4. 人工选取历史耗材会立即将该记录的总重量同步为当前稳定总重，并确认 ID；同步允许增加或减少。保存失败则回滚，不发布成功 ID。
5. 已确认耗材使用中，只记录稳定重量的下降；加重不会自动增大记录总重。定期保存需同时满足设置的时间间隔和最小减少量；`id=0` 时不进行周期使用更新。
6. 下料时保存此前已确认耗材最后的更低稳定总重。若最终写入失败，保留待保存数据并重试，成功前不允许确认另一卷。这是此前确认记录的最终保存，不是对未知耗材定期更新。
7. 颜色、材料和空盘重量可编辑；当前/待完成保存的耗材不能删除。未知或损坏存储不会被空库存覆盖。

页面余量计算为 `max(0, 总重量 − tare)`：当前确认卷使用 reading 的 `value`，其他卷使用记录的 `total`。总重与空盘重量分开保存，重新校准不重新解释已保存克数。推算余量的准确度取决于初始净重设置是否符合实际，可编辑空盘重量进行修正。突然断电仍可能丢失尚未满足保存条件的消耗。

### 页面与请求调度

`/` 为耗材/普通称重页面，`/settings` 为配置、校准与固件上传。设备执行并保存所选模式。普通模式停止耗材匹配、新建、选用、编辑、删除及周期库存写入，`id` 始终为0；普通去皮仅在当前网页内保存偏移，显示 `value − 本地去皮值`，不会修改设备空台零点或 reading 总重量。切入普通模式前保存上一耗材的最终消耗；保存失败时不宣称模式切换成功。刷新页面会清除此临时偏移。

普通模式提供“去皮”“记录”“导出 CSV”按钮，不再重复显示“普通称重/普通模式”标题。点击记录保存当前显示的重量（含去皮，精确到显示的0.1g）、日期和说明，最新记录在前；说明直接编辑并自动保存。轮询不会重建说明输入框。记录保存于同一浏览器、同一访问地址的本地存储（hx711.ordinary-log.v1），刷新后恢复，临时去皮仍会重置。记录、编辑、导出都不调用设备API。

CSV为日期、重量(g)、说明三列，使用UTF-8 BOM和CRLF，支持中文、引号与换行；日期按浏览器本地时间显示与导出，说明中的常见公式前缀以文本导出。断线、传感器异常或未校准时禁止记录失效读数，已有记录仍可编辑和导出。浏览器存储异常时明确提示，并保留内存记录供导出，不覆盖不可读的旧数据。设置页去掉可见“砝码重量”标签，保留输入框、无障碍名称与校准按钮。


设备使用原生WebServer单连接顺序处理，分段POST直接交给核心解析。已删除自定义前置收包检查、完整包等待和4KiB预读缓冲。空闲连接有其他请求排队时释放让位；接收采用核心约5秒空闲等待，不再使用自定义4秒绝对期限。首次读数失败后仍可显式选择模式，模式未确认时记录/库存操作仍受保护。

读数每次请求结束（成功、失败或超时）后等待 3 秒再读取，读写仍串行，列表同步可能进一步延后下一次读数。连续 3 次 reading 失败才显示“连接中断”并清空重量；前两次保留原显示，暂停依赖新读数的操作。任一次读取成功即清零失败次数并立即恢复显示。写操作暂停轮询，不会自动重发结果不确定的新增/删除。

完整列表首次读取、耗材写操作后强制读取，此外距离上次列表请求结束至少 60 秒再同步（受读数轮询节奏影响）；ID变化不额外触发列表请求。列表支持 ETag/304，未变更时不传数组。列表失败同样等待 60 秒重试，不累计 reading 失败次数；存储不可读时保留旧数据并显示错误。

设置页只读取一次配置，不轮询。已移除客户端字段类型、地址、阈值、密码与表单校验；API 返回的错误直接显示。设备端仍校验配置与保存事务。保留超时、错误提示、操作忙碌锁和配置令牌。

### 全部配置参数保留

| 参数 | 默认值 / 范围 | 用途 |
|---|---|---|
| `nominalG` | 1000；100–5000 g | 新建时推算空盘重量；不重算已有记录 |
| `matchToleranceG` | 5；0.5–50 g | 总重量唯一匹配容差 |
| `emptyLimitG` | 20；1–100 g | 空台判定 |
| `loadedLimitG` | 80；最高 500 g，至少比空台高 20 g | 上料判定 |
| `intervalMinutes` | 5；1–120 整数分钟 | 已确认卷定期保存间隔 |
| `minimumChangeG` | 2；0.5–100 g | 定期保存所需最小减少量 |

Wi-Fi 名称、密码、无密码网络和备用热点配置也保留。设置经 `GET/POST /api/settings` 成组读取/保存；归零 `POST /api/base-zero`，砝码校准 `POST /api/calibrate`（`grams`）。耗材写接口为 `/api/spools/create`、`select`（`id`）、`update`（`id,color,material,tare`）、`delete`（`id`）。页面通过 `POST /api/mode` 同步设备模式，只提交mode参数；每次reading的 `X-Scale-Mode` 响应头确认实际模式，JSON仍只有四个字段。模式设置重启后保留；不确定的切换结果通过后续读数确认而不自动重发。旧 `/api/tare` 仅为旧客户端兼容保留，新页面不调用。

### 存储兼容与回退

新记录以克数和版本 2 保存到 NVS `web-scale/recordsV2`。旧 `records` 键保留原始字节；只有旧记录结构、校验和、内容以及校准有效时才在内存转换，之后实际保存写新键。旧空盘未知则保留 null。新/旧记录损坏、版本不符或迁移所需校准缺失时禁止库存写入，绝不初始化并覆盖旧数据。配置 `deviceCfg` 及旧配置回退、`offset/tared/factor` 校准键保留。

此前设备升级后未读取到原校准和两条记录，此异常尚未恢复；新迁移逻辑不代表已找回那些数据。回退需使用完整旧源码/旧固件；旧固件不会读取 `recordsV2` 后续改动，只能看到保留的旧记录。刷写前保留源码与固件快照。

### 网络和更新

草图默认 Wi-Fi 可由设置页保存的配置覆盖。未配置或持续无法连接时启动开放备用热点 `Scale-xxxxxx`，访问 `http://192.168.4.1/settings`；只支持 2.4 GHz。设备地址为 `http://scale-xxxxxx.local/settings` 或路由器分配 IP。

`POST /api/update` 上传 multipart 字段 `firmware`，使用 ESP32-C3 应用 `.bin`，不可用合并引导镜像替代。固件成功写入并验证后重启。同步 HTTP 服务仍一次处理一个连接；慢连接有收发期限，OTA 有文件流空闲及回调时间预算，但上传期间仍占用服务。当前 OTA 弱网传输可能中断，失败后只能重试，不能据“文件已发送完整”判定更新成功。

### 验证与文件

核心文件：`InventoryLogic.h` 克数记录/规则，`Measurement.ino` 采样/状态，`InventoryApi.ino` 四字段接口与库存操作，`Storage.ino` 兼容加载，`ScalePage.h` 页面计算；`SettingsPage.h`、`SettingsApi.ino` 保留全部参数。

验证命令：

```sh
python3 tests/run_native_tests.py
python3 tests/settings_api_test.py
python3 tests/inventory_api_test.py
node tests/request_scheduling_test.mjs
node tests/page_polling_test.mjs
node tests/scale_page_browser_test.mjs
node tests/settings_page_browser_test.mjs
python3 tools/generate_web_pages.py --check
```

修改页面后先运行 `python3 tools/generate_web_pages.py`，再按 `esp32:esp32:XIAO_ESP32C3` 编译。原生计时/存储与浏览器模拟覆盖业务路径；没有实体操作时不能替代真实上料、卸料、砝码和弱信号测试。

## English: current contract and workflow

The device samples, determines stability, uniquely matches gross weight and persists records. The browser calculates remaining filament. Firmware and pages must be upgraded together; the former reading schema is removed.

### Hardware and calibration

ESP32-C3: HX711 DT GPIO6, SCK GPIO7; new-spool button GPIO3 to GND with pull-up. GPIO4/5 are reserved for I2C; no display driver is provided. Install Espressif ESP32 (tested 3.3.11) and bogde HX711; choose ESP32C3 Dev Module, serial 115200.

Use `/settings` to zero an empty platform and calibrate with a known weight. This update performs no automatic tare/calibration or inventory clearing.

### Reading and inventory contract

`GET /api/reading` returns exactly `raw,state,id,value`. Raw is the last integer ADC sample. State is 0 unloaded, 1 loaded, 99 uncalibrated, or 98 sensor fault. State98 takes priority for absent samples or actual sensor-read failure. Each loop spends at most approximately2ms waiting for HX711;3000ms accumulated actual waiting without data marks failure. HTTP time is excluded; cache age and numeric ADC endpoints alone do not trigger98. Value is gross grams computed from cached raw whenever calibration exists, including state98; null only without a cached sample or calibration. Id identifies the confirmed spool. Initial unstable/unmatched loads, confirmed unload, ordinary mode and uncalibrated state return zero; fluctuations and sensor faults retain an already confirmed ID.

A load enters at the configured loaded threshold; between thresholds retain the previous state. Approximately 0.6 s near empty confirms removal. Initial intermediate state is unloaded. Stability uses 10 samples, a 3 g spread and approximately 1.5 s persistence. Stability gates initial confirmation and consumption updates, not an already confirmed ID. Fluctuations and actual sensor-read failure retain that ID; faults are represented by state98 with numeric cached value when calibrated. No-data polls do not drive unload detection. Recovery or long sampling gaps rebuild the stability window before consumption updates. Confirmed unload, ordinary-mode selection and recalibration clear identity. Unstable/fault periods perform no consumption updates or periodic checkpoints.

`GET /api/spools` returns records containing exactly `id,material,color,tare,total`: material 0 PLA/1 PETG/2 ABS/3 TPU/4 other, hex color, empty assembly tare and saved stable total in grams. Capacity is 32. New records below nominal net weight and legacy unknown tares use null to mark an unconfirmed empty weight; the browser highlights the record instead of assuming zero.

Match only after stability, and only when exactly one record lies within the configured gross-weight tolerance. Multiple/no candidates leave id zero. With state 1/id 0 the user can create or select a record; the device rejects unstable actions for manual retry.

Creation sets total from stable gross and tare to `stable gross − nominalG`. A negative inferred tare does not block creation: persist tare=null, confirm the new ID, show a success warning and retain a visible record marker. Editing a valid tare clears the marker and restores remaining mass. Manual selection synchronizes the chosen total immediately, allowing an explicit increase/decrease, and confirms id only after persistence succeeds. Failures roll back.

Automatic usage updates only decrease the confirmed record's total, requiring stability plus configured save interval and minimum decrease. Id-zero readings never trigger periodic consumption writes. Removal flushes the previously confirmed lower stable total; a failed final save is retained/retried and blocks another confirmation until successful. Active/pending-save records cannot be deleted.

The page computes `max(0,total−tare)`, using live reading value for the confirmed spool and saved total for other records. Unknown legacy tare displays as unknown. Saved gram weights do not change meaning after recalibration. Inferred remaining mass depends on the configured initial net mass matching reality; edit tare to correct it. Sudden power loss can still lose consumption not yet checkpointed.

### Browser and settings

`/` shows spool/ordinary views; `/settings` contains configuration, calibration and OTA. The device owns and persists mode. Ordinary mode disables spool matching, create/select/edit/delete and periodic inventory writes, with id always zero. The browser-local tare displays `value−local offset` without changing device gross readings. Final previous-spool consumption is saved before entering ordinary mode; a failed save leaves mode unchanged. Refreshing clears this offset.

Ordinary view adds Tare, Record and Export CSV buttons without duplicate weighing/mode captions. Record snapshots the displayed/tared weight at 0.1g precision, date and editable notes, newest first. Notes save on input, and polling preserves editor focus. Records persist under hx711.ordinary-log.v1 for the same browser and origin; reopening restores records while temporary tare resets. Logging, editing and exporting make no device API calls.

CSV contains local date, weight(g) and notes with UTF-8 BOM, CRLF, quoted Unicode/newline escaping and common formula prefixes neutralized in notes. Disconnection, sensor errors and missing calibration disable invalid captures; existing records remain editable/exportable. Storage errors preserve exportable in-memory drafts and never overwrite unreadable stored data. Settings removes the visible calibration-weight caption while retaining its input, accessible name and calibration button.


Native WebServer handles one client at a time and parses segmented POSTs directly. Custom preflight/framing and the4KiB buffer are removed. Idle clients yield to queued connections. Receive waits use core defaults (approximately5s idle), with no custom4s absolute receive deadline. Mode choices remain manually retryable after initial read failure; logging/inventory controls remain gated until actual mode confirmation.

Reading polling waits3s after completion, including failures/timeouts; serialized inventory work may further delay the next read. Only3 consecutive reading failures show offline and clear weights. The first2 preserve the display while disabling actions requiring fresh weight. Any successful read resets the counter and immediately restores connection display. Writes pause polling; uncertain mutations are never automatically resent. Inventory syncs initially, after explicit inventory writes and at least60s after the previous list attempt completes, including failed attempts. ID changes do not trigger extra list requests. ETag/304 avoids unchanged payloads; inventory errors preserve cached records and do not count as reading failures.

Settings loads once without polling. Client type/address/range/password/native form checks were removed; server validation, grouped persistence, request errors/timeouts, busy locks and settings tokens remain. All configuration is retained: nominalG 1000 (100–5000 g), matchToleranceG 5 (0.5–50 g), emptyLimitG 20 (1–100 g), loadedLimitG 80 (at least empty+20, max 500 g), intervalMinutes 5 (1–120 integer), minimumChangeG 2 (0.5–100 g), and Wi-Fi configuration. Nominal affects new tares only.

Settings: GET/POST `/api/settings`; base zero: POST `/api/base-zero`; calibration: POST `/api/calibrate` with grams. Inventory mutations: POST `/api/spools/create`, `/select` with id, `/update` with id/color/material/tare, `/delete` with id. The page explicitly posts only mode to `/api/mode`; every reading response confirms actual mode via X-Scale-Mode without adding JSON fields or polls. Mode survives reboot; uncertain changes are read back rather than resent. Legacy `/api/tare` remains for older clients; the new page does not call it. Gross reading semantics remain fixed.

### Persistence, network and deployment

V2 gram records use `web-scale/recordsV2`. The original `records` bytes are preserved. Valid v1 records convert in memory only with valid calibration and are persisted to the new key on a subsequent save. Corrupt/version-incompatible/unmigratable data blocks inventory writes rather than replacing it with empty records. Existing configuration fallback and offset/tared/factor keys remain.

The earlier device upgrade's missing calibration/two records remain unresolved; the migration code does not prove those data have been recovered. Rollback restores the previous complete source/firmware; older firmware sees retained old records and cannot read later v2 updates.

Wi-Fi configuration overrides sketch defaults. An open Scale-xxxxxx fallback AP provides `http://192.168.4.1/settings`; 2.4 GHz only. Normal address is `http://scale-xxxxxx.local/settings` or assigned IP. OTA uses POST `/api/update`, multipart firmware, and an ESP32-C3 application binary (not a merged boot image). Reboot follows successful image verification. HTTP remains synchronous; bounded connection handling and upload callback/idle limits do not make OTA asynchronous. Weak-signal uploads can fail and need retries; sending the complete file is not proof of success.

Run the validation commands above and regenerate embedded pages before compiling `esp32:esp32:XIAO_ESP32C3`. Native timing/storage adapters and browser mocks prove workflow behavior; they do not replace physical load/unload/calibration or controlled RF tests.


### 采样异常显示 / Sensor-error display

reading 中 raw 是最后一次成功采样值，实际读取失败时仍保留，已校准时value仍由该缓存计算；state=98告知数据可能过时，不能据raw判断电量。HTTP阻塞不会仅因采样年龄触发98。页面在98期间保留同一模式最近有效总重、普通去皮重量或耗材余量，显示“传感器异常，读数暂未更新；显示上次有效数据”。缓存仅用于显示，记录/去皮/新建及选用耗材仍按当前状态禁用；首次无有效数据仍显示 --。采样恢复立即更新并消除告警。模式变更或未校准99清除缓存；网络失败原有显示逻辑不变。保持四字段接口及设备存储格式。

raw retains the last successful sample even during actual sensor-read failure; when calibrated, value still computes from that cache. state98 marks the value as potentially historical, not low battery. HTTP blocking cannot trigger98 merely by aging the sample. During state98 the page retains the last valid same-mode gross, locally tared weight or spool remaining, with an explicit stale-data warning. Cache is display-only: record/tare/create/select remain disabled by current state. Without a prior valid sample, display stays --. Recovery immediately updates the value and clears the warning. Mode changes and uncalibrated state99 clear the cache; network-failure behavior is unchanged. The four-field schema and device persistence format are preserved.


### 耗材记录总重 / Total weight in spool records

耗材行按“总重 · 余量 · 空盘”排列。当前耗材总重随有效实时读数变化（传感器异常沿用上一有效显示），其他耗材使用已保存的稳定总重；空盘待确认时仍显示总重并保留警告。该显示不会把瞬时波动写入耗材记录。
Spool rows show total, remaining, then tare. The active spool uses valid live gross readings (retained last-good display during sensor faults); other spools use saved stable total. Unknown tare retains its warning and still shows total. Displaying live fluctuations does not persist them to inventory.


T0 文案调整 / Notice wording: “选用”未满足条件的提示改为“请先稳定耗材,持续3s.”；仅更改提示，稳定检测窗口/阈值/计时保持不变。
Selection precondition notice now reads “请先稳定耗材,持续3s.”. Wording only; stability window, thresholds and timer unchanged.
