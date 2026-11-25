# Implementation Plan / 落地计划

## 中文

级别：T2。目标：在现有 ESP32-C3 网页秤上增加默认耗材模式、普通模式、多卷记录、自动识别、实体按钮与网页管理。非目标：本批不驱动 OLED，不接入 NFC，不提供云同步。4 线 I2C 屏预留 3V3、GND、GPIO4(SDA)、GPIO5(SCL)；HX711 保持 GPIO6/7，按钮使用 GPIO3 与 GND。

设计锚点：`HX711WebScale.ino` 的称重到网页流程。保持 HX711 砝码校准与耗材盘/支架去皮分离；普通模式临时去皮不能改变耗材记录所用的称台基准。支架随耗材一起取下，所以自动识别比较整卷总重。仅唯一候选落在误差范围内时自动选中。

任务树与边界：

1. 称重核心：稳定读数、空台/上料/取料判断、普通模式临时去皮。
2. 耗材逻辑：记录字段、余量计算、唯一匹配、新卷估算和待修正状态。
3. 存储与输入：版本化 NVS 记录、模式保存、GPIO3 按钮防抖。
4. 网页与串口：模式切换、当前卷、历史列表、编辑、手工选择和诊断。

按以上四批实施，每批以能编译和检查主要状态转移为回退点。风险：噪声导致误识别；相近重量无法区分；标称 1 kg 不是实际净重；掉电前最新余量尚未保存；重新校准影响换算。应对：稳定时间与迟滞、歧义保持待确认、标注估算/已校正、只在事件时写入、记录原始计数并用当前校准系数换算。

验收：普通模式可读数与临时去皮；空台取下时保存最后稳定余量；上料唯一匹配时选中，歧义或超差时不选；实体/网页新建不会重复创建；颜色、材料和余量修正可保存；断电重启后记录和模式存在。验证包括 Arduino 编译、逻辑场景测试、网页脚本检查；实机称重与按钮需设备验证。

## English

Level: T2. Objective: add a default spool mode, ordinary scale mode, multiple spool records, automatic matching, physical button, and browser management to the ESP32-C3 scale. Out of scope: driving the OLED, NFC integration, and cloud sync. Reserve 3V3, GND, GPIO4 (SDA), and GPIO5 (SCL) for a four-wire I2C display. HX711 remains on GPIO6/7; the button uses GPIO3 and GND.

Design anchor: the measurement-to-browser flow in `HX711WebScale.ino`. Keep sensor calibration separate from each spool's empty assembly tare. Ordinary-mode tare must not change the base used by inventory. Because the holder leaves with the spool, matching uses gross weight. Auto-select only one historical record within tolerance.

Task tree and boundaries: (1) measurement stability, load/removal events, and ordinary tare; (2) record data, remaining mass, unique matching, and estimated/unknown tare; (3) versioned NVS storage, saved mode, and button debounce; (4) browser controls and Serial diagnostics. Implement in four batches, compiling and checking state changes after each. Risks and mitigations: filter noisy readings; leave similar weights unresolved; mark nominal 1 kg as estimated; write on events rather than every sample; store raw counts so calibration changes can be reflected in calculations.

Acceptance: ordinary weighing and temporary tare work; removal stores the last stable remaining amount; only a unique in-tolerance load auto-selects; physical and browser creation do not duplicate records; color, material, and remaining corrections persist; records and mode survive restart. Validation: Arduino compilation, scenario tests, browser script check, and later device checks for weight and button behavior.

## 完成与验证

四批代码和中英文说明均已落地。代码复核通过：GPIO4/5 未被程序占用；GPIO3 仅作内部上拉按钮；普通临时去皮与耗材基准分离；断线旧读数不能触发新建；取料保存失败会保留待保存状态。ESP32-C3 编译通过（程序 79%，动态内存 11%）；原生逻辑测试覆盖空台/上料阈值、匹配边界、相近卷歧义、负向 HX711 系数及未知去皮；网页脚本语法检查通过。390px 与 1280px 浏览器检查均无横向溢出，390px 记录按钮右边界为 372px。

未覆盖：没有实机验证 HX711 噪声、GPIO3 按钮、Wi-Fi 重连或掉电恢复。标称 1 kg 得出的余量仍是估算；停电前未触发取料/编辑/切换时，最近变化可能尚未写入。回退可刷写旧版草图；旧版忽略新 `records` 键。

## Completion And Validation

All four code batches and bilingual instructions are complete. Code review passed: GPIO4/5 remain unused by this sketch; GPIO3 uses an internal pull-up; ordinary tare is separate from the spool base; stale disconnected readings cannot create a record; a failed removal save keeps the pending state. ESP32-C3 compilation passed (79% program storage, 11% dynamic memory). Native logic tests cover empty/load thresholds, matching boundaries, ambiguous similar spools, negative HX711 factors, and unknown tare. Browser script syntax passed. Browser checks at 390px and 1280px found no horizontal overflow; the right edge of the mobile record buttons was 372px.

Not yet covered on hardware: HX711 noise, GPIO3 button, Wi-Fi reconnection, and power-loss recovery. Nominal 1 kg results remain estimates. Recent consumption may be lost if power fails before removal, edit, or mode change. Roll back by flashing a retained earlier sketch; it ignores the new `records` key.

## 双页面与在线更新 / Two Pages And Online Update

级别：T3（在线更新会改写固件；用户已明确要求实施）。目标：`/` 只保留耗材秤与普通称重操作，`/settings` 集中放称台归零、砝码校准和网页固件上传。非目标：不增加远程版本检查或自动下载；不改变耗材数据格式。设计锚点是网页路由和 GPIO3 按钮。保持网页服务只在 Wi-Fi 就绪后启动，上传失败不重启，旧固件和 NVS 数据可继续使用。

批次：1. 拆分两个独立页面并检查导航；2. GPIO3 短按新建、长按 5 秒解锁一次 2 分钟的固件上传；3. 使用 ESP32 `Update` 将 `.bin` 写入 OTA 分区，仅成功完成后重启；4. 编译、接口/浏览器检查和中英文说明同步。风险：局域网未经认证、错误固件、断电和无 OTA 分区。应对：物理按钮限时授权、只接受 `.bin`、检查写入与结束结果、确认分区、失败时保留当前固件；USB 串口刷写作为回退路径。验收：称重页无校准控件；设置页可校准和查看更新锁定状态；未解锁上传被拒；短按不触发 OTA、长按不新建耗材；失败不重启，成功才重启。

Level: T3 (firmware upload rewrites the device; the user explicitly requested implementation). Goal: keep spool and ordinary weighing at `/`, and move base zero, calibration, and browser firmware upload to `/settings`. Out of scope: remote release checking and automatic downloads; the spool storage format stays unchanged. Anchor: web routes and the GPIO3 button. Preserve the Wi-Fi-before-server startup order; failed uploads leave the running firmware and NVS data available.

Batches: (1) split two pages and check navigation; (2) make GPIO3 short press create a spool and 5-second hold unlock one upload for two minutes; (3) stream a `.bin` to the ESP32 OTA partition through `Update`, rebooting only on success; (4) compile, inspect endpoints and layouts, and sync bilingual instructions. Risks: unauthenticated LAN access, wrong firmware, power loss, and a missing OTA partition. Mitigations: short physical authorization, `.bin` validation, checking each write and finalization, partition validation, and USB serial flashing as recovery. Acceptance: no calibration controls on the scale page; settings show calibration and update lock state; locked uploads fail; short press does not arm OTA and long press does not create a spool; failure does not reboot and success does.

### 本轮结论 / This Update

四批均已落地。代码复核通过：主页面无校准控件，设置页有独立路由；上传开始前消费一次实体授权；非法字段、非 `.bin`、无 OTA 分区、写入失败和中断均不会触发重启；成功结束后才安排重启。开始上传前保存当前耗材最后稳定读数。Arduino ESP32-C3 编译通过（程序分区 80%，动态内存 11%）；按钮授权测试覆盖 5 秒边界、2 分钟到期、一次性消费和计时器回绕；耗材逻辑测试及两个页面的脚本语法检查通过。390px 和 1280px 页面截图及宽度检查无横向溢出。

未覆盖：没有对真实设备上传固件或模拟掉电，不能确认具体板子的 OTA 分区与重启后的固件表现。局域网用户在实体按钮解锁窗口内可提交固件；错误板型固件可能需要 USB 恢复。回退路径是 USB 刷写此前版本；现有 NVS 校准与耗材记录保持原格式。

All four batches are implemented. Code review passed: calibration controls are absent from the scale page; Settings has its own route; a physical authorization is consumed before upload; invalid fields, non-`.bin` names, missing OTA partitions, failed writes, and aborted uploads do not schedule a restart; only a successful finalization does. The mounted spool's last stable reading is saved before upload. ESP32-C3 compilation passed (80% program partition, 11% dynamic memory). Gate tests cover the 5-second boundary, two-minute expiry, single use, and timer wraparound. Inventory tests and both page scripts passed. Screenshots and width checks at 390px and 1280px showed no horizontal overflow.

Not covered: no firmware upload or power-loss simulation on the physical device, so the board's actual OTA partition and post-reboot behavior remain unverified. A LAN user can submit firmware during the physically unlocked window; a wrong-board image may require USB recovery. Roll back by flashing an earlier version over USB. Existing NVS calibration and spool data formats are unchanged.

## 耗材参数设置 / Spool Configuration

级别：T2。目标：在 `/settings` 增加新耗材默认净重、历史匹配容差、空台阈值、上料阈值，并将其保存到 NVS；称重页继续只呈现操作。非目标：不改变已有耗材记录的去皮值，不开放稳定滤波和 OTA 授权参数。设计锚点：`InventoryLogic.h` 的分类/匹配函数及 `HX711WebScale.ino` 的新建流程。约束：空台阈值必须低于上料阈值；配置错误或保存失败时维持旧配置；默认净重只作用于后续新建记录。

执行批次：1. 参数模型和边界测试；2. 版本化 NVS 保存与称重路径接入；3. 设置页表单和接口；4. 编译、网页验证与中英文说明同步。风险：阈值组合导致误识别、部分写入、修改默认净重后用户误以为历史余量会变化。应对：成组验证与单个 NVS 数据块写入、显示适用范围、目的驱动测试。验收：默认值保持 1000/5/20/80 g；有效修改在重启后保留并改变新卷估算及匹配/分类；无效值被拒绝且旧值不变；历史记录不重算。

Level: T2. Goal: add nominal new-spool mass, historical match tolerance, empty threshold, and loaded threshold to `/settings`, persisted in NVS. The scale page stays focused on operations. Out of scope: changing existing spool tare values or exposing stability filtering and OTA authorization parameters. Anchor: classification/matching in `InventoryLogic.h` and new-record creation in `HX711WebScale.ino`. Invariants: empty threshold stays below loaded threshold; invalid or failed saves keep the previous settings; nominal mass applies only to future records.

Batches: (1) settings model and boundary tests; (2) versioned NVS storage and measurement integration; (3) settings form and API; (4) compilation, browser checks, and bilingual documentation. Risks: unsafe threshold combinations, partial writes, and confusion about historical estimates. Mitigations: grouped validation, one NVS blob, clear scope in the form, and purpose-driven tests. Acceptance: defaults remain 1000/5/20/80 g; valid changes persist across restart and affect new estimates/matching/classification; invalid changes are rejected without altering old values; existing records are not recalculated.

### 完成记录 / Completion

四批已完成。代码复核通过：四项参数在 `spoolCfg` 中成组保存；新建、匹配、空台/上料判断均读取同一配置；不合理阈值或与当前卷冲突的上料阈值被拒绝；旧记录的去皮原始计数不变。ESP32-C3 编译通过（程序 80%，动态内存 11%）；逻辑测试覆盖默认值、自定义匹配边界、分类阈值和无效组合，OTA 按钮测试仍通过；两页脚本语法通过，设置页在 390px/1280px 无横向溢出，并能加载模拟的已保存配置。未覆盖：尚无真实设备上的保存后断电重启和称重噪声验证。回退时旧版忽略新增 `spoolCfg` 键。

All four batches are complete. Code review passed: the four values are saved together in `spoolCfg`; creation, matching, and empty/load classification use the same settings; invalid thresholds and a loaded threshold conflicting with the active spool are rejected; existing raw tare records remain unchanged. ESP32-C3 compilation passed (80% program storage, 11% dynamic memory). Logic tests cover defaults, custom match boundaries, classification thresholds, and invalid combinations; OTA button tests still pass. Both page scripts passed syntax checks. Settings had no horizontal overflow at 390px/1280px and loaded mocked saved values. Not covered: persistence across a physical power cycle and measurement noise on the actual device. Older firmware ignores the new `spoolCfg` key on rollback.

## 功能拆分与更新锁移除 / Functional Split And Unlock Removal

级别：T3。目标：将超过千行的主草图按网页、存储、称重、耗材接口、设置接口及固件更新拆开；移除网页固件更新的 GPIO3 长按解锁，保留 `.bin`、OTA 分区和写入完成检查。用户已明确要求移除更新锁。非目标：不改接线、耗材记录格式或 OTA 上传方式。设计锚点：`HX711WebScale.ino`。约束：Arduino IDE 仍能直接打开同名主草图；GPIO3 仅负责新建耗材；失败上传不重启；网页地址与数据接口保持兼容。

影响边界：`ScalePage.h`/`SettingsPage.h` 保存页面；`Storage.ino` 保存配置与记录；`Measurement.ino` 管理采样状态；`InventoryApi.ino` 与 `SettingsApi.ino` 管理各自接口；`FirmwareUpdate.ino` 管理 OTA；主草图只留共享状态、路由、`setup`/`loop`。批次及回退点：1. 机械拆出两个页面并编译；2. 拆出存储/称重并编译；3. 拆出接口/OTA、移除解锁及旧测试并编译；4. 审查接口与网页状态、运行测试、同步中英文文档。风险：Arduino 多标签拼接顺序、隐藏的跨文件依赖、未经认证的局域网固件上传。通过显式函数声明、逐批编译、验证锁移除路径并在说明中明确风险控制。验收：主草图显著缩短；两个页面与原称重接口工作；固件上传不再需要长按，GPIO3 点击仍新建耗材；失败上传仍不切换固件。

Level: T3. Goal: split the thousand-line main sketch into pages, storage, measurement, inventory API, settings API, and firmware update; remove the GPIO3 long-press unlock while retaining `.bin`, OTA partition, and completed-write checks. The user explicitly requested removing the update lock. Out of scope: wiring, record format, and OTA transport changes. Anchor: `HX711WebScale.ino`. Invariants: Arduino IDE opens the same sketch; GPIO3 only creates spools; failed uploads do not reboot; page URLs and data APIs remain compatible.

Boundaries: `ScalePage.h` and `SettingsPage.h` own pages; `Storage.ino` owns persistence; `Measurement.ino` owns sampling; `InventoryApi.ino` and `SettingsApi.ino` own endpoints; `FirmwareUpdate.ino` owns OTA; the main sketch retains shared state, route registration, `setup`, and `loop`. Batches with compile checkpoints: (1) extract pages; (2) extract storage and measurement; (3) extract API/OTA, remove unlock and its tests; (4) review routes and UI, run tests, sync bilingual docs. Risks: Arduino tab concatenation order, hidden cross-file dependencies, and unauthenticated LAN firmware upload. Mitigate with explicit declarations, per-batch compilation, upload-path review, and clear documentation. Acceptance: main sketch is substantially shorter; both pages and weighing APIs remain functional; upload needs no button hold; GPIO3 press still creates a spool; failed uploads never switch firmware.

### 本轮完成 / Completion

主草图由 1186 行缩至 213 行，功能分布到 7 个 `.ino`/`.h` 文件；已删除旧更新解锁模块与专属测试。代码复核通过：网页不再显示解锁状态，上传端不检查实体授权，GPIO3 按下仅尝试新建耗材；`.bin` 文件名、OTA 分区、写入及结束结果仍受检查，只有成功返回后才安排重启。Arduino ESP32-C3 编译通过（程序 80%，动态内存 11%）；耗材逻辑测试及两个页面的 JavaScript 语法检查通过。验证目的分别是确认多标签可编译、原耗材判断仍符合边界场景，以及页面脚本可解析。未覆盖真实设备的上传、中断、断电和重启后行为；任何能访问设备网页的局域网用户现在都能提交固件。回退可用 USB 刷写旧版，NVS 数据格式未变。

The main sketch is down from 1186 to 213 lines, with behavior distributed across seven `.ino`/`.h` files; the obsolete unlock module and its dedicated test were removed. Code review passed: the page has no unlock state, the upload endpoint requires no physical authorization, and GPIO3 only attempts spool creation. The `.bin` name, OTA partition, write, and finalization checks remain; a restart is scheduled only after a successful response. ESP32-C3 compilation passed (80% program storage, 11% dynamic memory). Inventory logic tests and both page JavaScript syntax checks passed. These checks establish that the tabs compile together, core matching boundaries remain valid, and page scripts parse. Physical upload, interruption, power loss, and post-reboot behavior remain untested. Anyone on the local network who can reach the page can now submit firmware. Roll back over USB; the NVS format is unchanged.

## 耗材页读数与编辑 / Scale Page Readout And Editing

级别：T2。目标：耗材模式并列呈现当前余量和称重总量，记录标题同行放新建按钮，并修复保存记录后编辑区不收起。非目标：不修改称重计算、API、NVS 结构或设置页。设计锚点：`ScalePage.h` 的渲染和按钮事件。保持普通模式读数与去皮可用；传感器断线或未校准时不显示旧重量；保存失败时保留编辑输入。任务树与边界：1. 调整读数、记录标题和移动端布局；2. 统一异步操作成功/失败反馈并管理编辑区状态；3. 浏览器交互检查、编译和中英文说明。风险：定时刷新覆盖新建后的编辑状态、按钮忙碌时重复提交、移动端标题挤压。逐项验证成功/失败交互、普通模式和窄屏布局。验收：耗材模式显示两项数值，无底部原始读数和独立“当前耗材”区；新建按钮在“耗材记录”右侧；新建后可直接编辑；保存成功收起表单，失败保留；取消、选择、删除及模式切换仍可用。

Level: T2. Goal: show remaining and gross mass in spool mode, place New beside the records heading, and close the editor after a successful save. Out of scope: measurement math, APIs, NVS format, and Settings. Anchor: rendering and button events in `ScalePage.h`. Preserve ordinary weighing and tare; avoid stale weights on disconnect or before calibration; retain user input after a failed save. Batches: (1) readout, heading, and responsive layout; (2) async action feedback and editor state; (3) browser interaction checks, compile, and bilingual documentation. Risks: polling during new-record editing, duplicate clicks while busy, and narrow headings. Acceptance: two spool metrics, no lower raw row or separate Current spool section, New aligned with records, direct editing after create, editor closes only on save success, and cancel/select/delete/mode controls remain functional.

### 完成与验证 / Completion And Validation

代码复核通过：耗材读数主区显示余量和总重，普通模式保留普通读数与去皮；原始值行及独立“当前耗材”区已移除。网页新建成功后打开对应编辑区，保存成功后收起，失败时保留输入；取消、选用、删除和模式切换仍可操作。浏览器模拟在 390px 和 1280px 均通过上述成功/失败交互与横向溢出检查；两个页面脚本语法及 ESP32-C3 编译通过（程序 80%，动态内存 11%）。实机按键、传感器和浏览器连接中断仍需设备验证；本轮仅改页面、测试及说明，回退可恢复旧 `ScalePage.h`。

Code review passed: the spool readout shows remaining and gross mass, while ordinary mode retains its reading and tare. The raw-data row and separate Current spool section are removed. Browser creation opens the matching editor; successful save closes it, failure retains input; cancel, select, delete, and mode switching remain usable. Browser simulations at 390px and 1280px passed these success/failure paths and horizontal overflow checks. Both page scripts parsed and the ESP32-C3 compiled (80% program storage, 11% dynamic memory). Physical button, sensor, and browser connection-loss behavior still need device checks. Only the page, test, and documentation changed; restore the previous `ScalePage.h` to roll back.

## 最近使用排序与空台 / Recent Use And Empty Platform

级别：T2。目标：耗材记录按最近一次上秤或手动选用排序，最近在上；空台时无可选中耗材，读数区只显示“无耗材”。非目标：不增加时钟/NFC，不改变记录二进制格式或普通模式。设计锚点：`InventoryApi.ino` 的记录激活路径与 `ScalePage.h` 的读数状态。现有记录没有历史时间戳，旧记录首次使用前只能保留已有顺序；从本版开始每次使用移动到数组前端并持久化。约束：排序更新失败不能错选记录；最近原始读数不能代替最新传感器读数判断称台是否有重量。

任务树与批次：1. 加入记录移动与激活时的原子保存/失败回滚，覆盖新建、自动匹配、手动选用；2. 从最新称重读数导出有料状态，收紧选用/新建接口，空台隐藏读数并禁用选用；3. 原生顺序测试、浏览器状态/按钮测试、编译及双语说明。风险：移动数组后索引失效、NVS 写入失败、下料确认的短暂延迟。验收：新建或重新选用的记录排第一，未使用者顺序保持，重启后排序仍在；空台立即显示“无耗材”，记录均不可选，直接调用选用接口也被拒绝；有料及普通模式原行为保留。回退点为恢复本轮源文件；NVS 记录格式不变。

Level: T2. Goal: sort records by most recent load or manual selection, newest first; when the platform is empty, allow no selected spool and show only “无耗材” in the readout. Out of scope: a clock, NFC, binary record format changes, and ordinary mode. Anchor: record activation in `InventoryApi.ino` and readout state in `ScalePage.h`. Old records have no historical timestamps, so their existing order remains until used; future uses move a record to the front and persist that order. Invariants: a failed reorder must not select the wrong record; stale stable readings cannot prove the platform is loaded.

Batches: (1) move and persist records on creation, automatic match, and manual selection with failure rollback; (2) derive loaded state from the latest reading, reject empty-platform actions, and render an empty readout; (3) native order tests, browser state and button tests, compile, and bilingual documentation. Risks: index shifts, NVS write failure, and the removal confirmation delay. Acceptance: recent use rises to the top with older order preserved and survives restart; empty platform promptly shows “无耗材”, disables all Select buttons, and rejects direct Select API calls; loaded and ordinary mode remain functional. Restore this batch's sources to roll back; the NVS record format stays unchanged.

### 完成与验证 / Completion And Validation

实现审查通过：新建记录插入首位；自动匹配或手动选用旧记录时移动到首位并保存，写入失败恢复数组顺序及旧记录读数，自动匹配失败不会开放“新建”来制造重复记录。网页状态以最新读数区分有料、空台和过渡重量；空台只显示“无耗材”，选用及新建按钮不可用，接口也检查最新读数。原生测试覆盖多次使用后的排序、无效索引和“旧稳定读数有料但最新读数为空”的边界；浏览器模拟在 390px/1280px 覆盖排序、空台、过渡重量、编辑和普通模式；页面脚本语法及 ESP32-C3 编译通过（程序 80%，动态内存 11%）。尚未在实机验证 NVS 掉电恢复、HX711 噪声和取料时序。旧记录没有历史使用时间，首次重新使用前不能恢复真实先后；本轮保持原有二进制记录格式，可用旧版固件回退。

Code review passed: new records are inserted first; automatic matches and manual selections move old records to the front and persist, with array order and prior readings restored on write failure. A failed automatic reorder does not offer New for an already matched spool. The page distinguishes loaded, empty, and transitional weights from the latest reading. Empty shows only “无耗材”, disables Select and New, and the endpoint checks the latest reading too. Native tests cover repeated-use order, invalid indices, and a stale loaded stable reading with an empty latest reading. Browser simulations at 390px/1280px cover ordering, empty and transitional weight, editing, and ordinary mode. Both page scripts parsed and ESP32-C3 compilation passed (80% program storage, 11% dynamic memory). Physical NVS power-cycle recovery, HX711 noise, and removal timing remain untested. Old records lack usage timestamps, so their historical order cannot be reconstructed before reuse. The binary record format is unchanged, permitting rollback to older firmware.

## 网页响应速度 / Web Responsiveness

级别：T2。目标：降低设备网页连接和轮询延迟，保留称重与更新功能。非目标：不更换 HTTP/HX711 库，不修改称重算法和记录格式。复现：同网段 10 次 ICMP 均成功（平均约 70 ms），但首页 TCP 多次超时，`/api/reading` 曾建连约 3 秒后 15 秒内未返回数据；设备未接 USB，暂不能读取运行日志。设计锚点：`HX711WebScale.ino` 的 Wi-Fi 启动与两页 `refresh` 定时器。约束：ESP32-C3 单核，HTTP 服务由主循环调用；页面请求失败后仍须继续重试；OTA 重启轮询保持可用。

批次：1. 关闭 Wi-Fi 省电并在现有串口状态中加入信号强度以便实机诊断；2. 将两页固定间隔轮询改为上一轮完成后再安排下一轮，避免慢请求堆积；3. 编译、页面脚本/浏览器检查及双语说明。风险：关闭省电增加功耗，实际网络或 HX711 阻塞仍可能影响响应；避免在缺少串口证据时改动称重驱动。验收：浏览器首屏与按钮流程无回归，轮询不会自身重叠，Wi-Fi 连接成功后网页继续服务，串口可看到信号强度。实际设备上的延迟改善需刷写后复测。

Level: T2. Goal: reduce page connection and polling latency while preserving weighing and firmware update. Out of scope: replacing HTTP/HX711 libraries or changing measurement math and record storage. Reproduction: ten same-subnet ICMP probes succeeded (about 70 ms average), while the home TCP connection repeatedly timed out and `/api/reading` once connected after about 3 seconds but returned no bytes within 15 seconds. The device is not on USB, so runtime logs are unavailable. Anchor: Wi-Fi startup in `HX711WebScale.ino` and both pages' refresh timers. Constraints: the ESP32-C3 is single-core and the main loop services HTTP; failed page requests must retry; OTA reboot polling must continue.

Batches: (1) disable Wi-Fi power saving and add signal strength to the existing serial status; (2) schedule each page poll after its previous run completes to avoid accumulating slow requests; (3) compile, check scripts/browser behavior, and sync bilingual documentation. Risks: higher power draw and possible underlying network or HX711 blocking. Leave the sensor driver unchanged without serial evidence. Acceptance: no regression in page or button flows, polling cannot overlap itself, the server still starts on Wi-Fi connection, and Serial reports signal strength. Measure actual latency again after flashing the device.

### 完成与验证 / Completion And Validation

两页轮询均改为上一轮结束后再定时，避免慢请求按固定频率堆积；单轮超过 6 秒会中断并继续重试，设置页上传固件期间暂停状态轮询。ESP32-C3 连接 Wi-Fi 时关闭省电，串口每 5 秒报告 RSSI，单次网页处理超过 1 秒时报告耗时。代码复核通过：网页路由和称重接口未变。ESP32-C3 编译、耗材逻辑原生测试、耗材页 390px/1280px 交互检查通过；两页浏览器模拟均证明慢请求不重叠、超时后会再试。实机尚未刷写本轮固件，不能声称设备延迟已改善；若刷写后仍出现“ping 通但 HTTP 无响应”，需结合串口判断主循环是否停在 HX711 读取或 HTTP 处理。回退可恢复旧版草图；关闭省电会略增功耗。

Both pages now schedule their next poll only after the prior one finishes. A poll exceeding six seconds is aborted and retried; Settings pauses status polls during firmware upload. Wi-Fi power saving is disabled on the ESP32-C3; Serial reports RSSI every five seconds and logs a web handler taking over one second. Code review found routes and measurement APIs unchanged. ESP32-C3 compilation, native inventory tests, and scale-page interactions at 390px/1280px passed. Browser simulations of both pages confirmed that slow polls do not overlap and timed-out polls retry. This firmware has not yet been flashed to the device, so a latency improvement is unverified. If ping still works while HTTP stalls, Serial logs should distinguish an HX711 read stall from HTTP processing. Restore the prior sketch to roll back; disabling power saving increases consumption slightly.

## 耗材列表按变化刷新 / Event-Driven Spool List Refresh

级别：T2。目标：减少耗材页对 `/api/spools` 的请求，并在耗材持续使用时定期保存余量；`/api/reading` 仍周期读取。非目标：不改变记录格式、称重换算或设置页。设计锚点：`Measurement.ino` 的稳定读数与保存路径、`InventoryApi.ino` 的读数摘要和 `ScalePage.h` 的刷新流程。约束：物理按钮和另一浏览器编辑也应被发现；活动耗材余量随轻量读数更新；保存失败应保留旧值以便下次重试；记录请求失败后也须重试。

追加定期保存需求后，改用运行期列表版本号：只在新建、删除、编辑、换卷、下料或校准影响列表时递增；每 5 分钟且稳定重量比上次保存变化至少 2 g 时保存活动耗材原始值，但不递增列表版本。批次：1. 加入定期保存及失败回滚；2. 将列表版本随读数返回，页面只在初次进入或版本变化时获取列表，并本地更新活动 ID/余量；3. 浏览器与原生逻辑验证、编译及双语说明。风险：闪存磨损、版本重启、请求交错。处理：时间和重量双门槛、启动时随机版本、串行页面刷新；只有完整列表成功后记住版本。验收：静止称重时不重复请求 `/api/spools`；新建/删除/编辑/换卷触发一次，活动余量持续更新；定期保存不触发列表请求；保存失败可重试；下料仍立即保存。回退可恢复本轮文件，NVS 格式不变。

Level: T2. Goal: reduce `/api/spools` requests and periodically persist remaining mass during use, while continuing to poll `/api/reading`. Out of scope: binary record format, weight conversion, and Settings. Anchor: stable readings and persistence in `Measurement.ino`, the reading summary in `InventoryApi.ino`, and refresh logic in `ScalePage.h`. Invariants: physical-button and other-browser edits are detected; active remaining mass updates from lightweight readings; failed saves retain the prior value for a later retry; failed list requests retry.

The added periodic-save requirement calls for a runtime list revision instead of the stored checksum. Increment it on create, delete, edit, spool switch, removal, or calibration that changes list output. Save the active raw mass every five minutes only when its stable weight differs from the last saved mass by at least 2 g; do not increment the list revision for this checkpoint. Batches: (1) checkpoint with failure rollback; (2) expose the list revision and fetch the list only initially or on a revision change, updating active ID and mass locally; (3) browser and native checks, compile, and bilingual documentation. Risks: flash wear, revision reset, and interleaved requests. Mitigations: time and weight thresholds, a random initial revision on boot, serialized page refreshes, and remembering a revision only after a successful list fetch. Acceptance: stationary weighing does not repeatedly request `/api/spools`; create/delete/edit/switch requests it once; active remaining mass keeps updating; checkpoints do not request the list; failed saves retry; removal still saves immediately. Restore this batch's sources to roll back; NVS remains unchanged.

### 完成与验证 / Completion And Validation

代码复核通过：列表版本在新建、编辑、删除、换卷、下料和重新校准等可见变化时更新；当前耗材 ID 与余量由轻量读数本地更新。定期检查以 5 分钟和 2 g 为双门槛，只有稳定有料时写入，失败时恢复旧值并延后重试；此保存不触发完整列表请求。浏览器测试覆盖静止读数零次额外列表请求、事件触发一次请求、实体按钮新增、跨页面编辑、失败重试及当前余量更新；390px/1280px 交互通过。原生测试覆盖时间/重量边界与计时器回绕；ESP32-C3 编译通过（程序 80%，动态内存 11%）。未覆盖实机断电恢复和长期闪存磨损；本轮未向设备刷写固件。设备 MAC `b8:1f:3f:2b:ad:1c` 对应 `192.168.1.119`；最终复测首页、读数、列表均返回 200，分别约 0.25/0.21/0.03 秒。设备响应仍无新增的 `listRevision`，故不能把当前速度归因于本轮固件。

Code review passed: the list revision changes on visible create, edit, delete, spool switch, removal, or recalibration; active ID and remaining mass update locally from lightweight readings. A checkpoint requires both five minutes and a stable 2 g change, restores the prior value on save failure, and does not request the full list. Browser tests cover no extra list request during stationary weighing, one request per event, physical-button creation, another browser's edit, failed-request retry, and live remaining mass; interactions passed at 390px/1280px. Native tests cover timing, mass boundaries, and timer wraparound. ESP32-C3 compilation passed (80% program storage, 11% dynamic memory). Physical power-loss recovery and long-term flash wear remain untested; this firmware was not flashed to the device. MAC `b8:1f:3f:2b:ad:1c` maps to `192.168.1.119`. A final live check returned HTTP 200 for the home page, reading, and list in about 0.25/0.21/0.03 seconds. The device response still lacks `listRevision`, so its current speed cannot be attributed to this firmware.

## 自动保存参数与设置页简化 / Checkpoint Settings And One-Time Settings Page

级别：T2。目标：将自动保存间隔和重量变化阈值配置到设置页并持久化；设置页打开时各配置只读取一次，不再轮询；移除在线更新状态检查接口和版本/重连检查逻辑，保留上传功能。非目标：不改变耗材记录格式、不移除 OTA 上传、不改变称重换算。设计锚点：`Measurement.ino` 的自动保存门槛、`Storage.ino` 的 NVS 读取、`SettingsPage.h` 的一次性加载。旧 `spoolCfg` 必须继续可读，故新增独立 `checkpointCfg`；默认 5 分钟、2 g。约束：无效配置或写入失败不改变运行参数；设备不支持 OTA 时由上传接口返回错误；成功上传后仍只在写入完成时重启。

任务树与批次：1. 增加版本化自动保存配置、校验、GET/POST 接口和测量接入，原生测试/编译；2. 设置页增加两个输入并改为一次性读取，删除 `/api/update/status` 路由及处理、精简上传状态；3. 浏览器交互/请求次数检查、最终编译和双语说明。风险：旧 NVS 配置兼容、用户将间隔设得过短造成闪存写入增加、上传页失去预检查提示。采用独立键、范围限制与服务端上传错误提示。验收：默认值与有效修改持久化，非法值被拒且旧值保留；原有四项参数不丢失；设置页无周期请求；更新状态接口不存在，上传成功/失败处理仍可用；耗材页行为不回归。回退时旧版忽略新增 `checkpointCfg`。

Level: T2. Goal: expose and persist checkpoint interval and weight-change threshold on Settings; read each setting once when the page opens, with no polling; remove the firmware-update status endpoint and version/reconnection checks while retaining upload. Out of scope: spool record format, OTA upload removal, and weight conversion. Anchor: checkpoint gating in `Measurement.ino`, NVS loading in `Storage.ino`, and initial loading in `SettingsPage.h`. Preserve the existing `spoolCfg` by adding a separate versioned `checkpointCfg`, defaulting to five minutes and 2 g. Invalid or failed saves must leave active values unchanged. The upload endpoint still reports unsupported partitions and only reboots after a completed write.

Batches: (1) add validated checkpoint storage and GET/POST API, connect measurement, run native tests and compile; (2) add two controls, make Settings load once, remove `/api/update/status` route and handler, simplify upload state; (3) browser interaction/request-count checks, final compile, and bilingual documentation. Risks: compatibility with existing NVS settings, increased flash writes if interval is short, and loss of pre-upload availability messaging. Use an independent key, bounded values, and server upload errors. Acceptance: defaults and valid edits persist, invalid edits leave old values, the four existing settings survive, Settings has no periodic requests, the status endpoint is gone while upload success/failure still works, and the Scale page does not regress. Older firmware ignores `checkpointCfg` on rollback.

### 完成与验证 / Completion And Validation

三个批次已完成。代码复核通过：自动保存使用独立且带版本/校验的 `checkpointCfg`，读取无效数据时恢复默认值，写入失败不替换运行值；原 `spoolCfg` 格式未变。设置页打开时仅各读取一次校准状态、耗材参数和自动保存参数，无定时请求；`/api/update/status` 路由和页面调用均已移除。OTA 上传仍检查文件字段、`.bin` 后缀、可用分区、写入和结束结果，只有成功才安排重启。

原生逻辑测试验证自动保存参数边界、时间与重量门槛及计时器回绕；设置页浏览器测试验证一次性读取、保存成功/失败、上传反馈和 390px/1280px 布局；耗材页交互和延迟轮询测试通过。ESP32-C3 编译通过：程序空间 81%，动态内存 11%。中英文 README 已同步。尚未实机刷写或断电重启，NVS 跨重启保留及真实 OTA 上传仍待设备验证；缩短间隔会增加闪存写入。回退可用 USB 刷写旧固件，旧版忽略新增配置键。

All three batches are complete. Code review passed: checkpoint settings use a separate versioned, checksummed `checkpointCfg` key; invalid stored data falls back to defaults, and a failed write leaves active settings unchanged. The existing `spoolCfg` format is unchanged. Settings reads calibration, spool, and checkpoint data once on opening, with no timer. The `/api/update/status` route and page request are gone. OTA upload still validates the field, `.bin` extension, partition, writes, and finalization; only success schedules a reboot.

Native logic tests cover setting bounds, time and mass thresholds, and timer wraparound. Settings browser tests cover one-time reads, successful and failed saves, upload feedback, and 390px/1280px layout. Scale interactions and delayed polling tests pass. ESP32-C3 compilation passed at 81% program storage and 11% dynamic memory. The bilingual README is updated. Flashing, power-cycle persistence, and real OTA upload remain unverified on hardware; shorter intervals increase flash writes. Roll back with an earlier firmware over USB; it ignores the new configuration key.

## 统一设置与页面设计 / Unified Settings And Page Design

级别：T2。目标：设置页通过一次 GET 获取校准状态及全部六项配置，通过一个保存按钮一次 POST 提交六项配置，并改善视觉层次。非目标：改变称重换算或固件上传机制。设计锚点：设置页加载—编辑—保存流程。校准和归零需要现场放置/取下砝码，仍为独立即时操作；“保存设置”只提交配置参数。

任务树/模块边界：1. `Storage.ino` 与主程序：六项配置统一写入一个版本化 NVS 数据块，首次启动兼容旧 `spoolCfg`/`checkpointCfg`；2. `SettingsApi.ino`：GET/POST `/api/settings`，全部校验完成后才提交，失败不部分生效；3. `SettingsPage.h`：暖白与深绿仪表风格、编号分区、明确的参数单位及底部统一保存区；4. 测试及双语说明。按上述顺序实施，每批检查后继续。

风险与约束：两次独立存储可能造成部分保存，故改为单个 `settingsCfg` 数据块；保存成功前不更新运行参数。兼容读取旧键，旧固件回退时仍使用旧值。加载失败禁止提交，保存时禁止重复提交，出错保留输入。无周期请求；保存、校准、上传后均不追加 GET。验收：首次只有一次参数请求；六项一起保存/一起失败；旧配置可读取；手机/桌面无横向溢出且操作清晰；上传仍可使用。回退恢复本轮之前的文件或刷写旧版，新增键可保留。

方案复核：通过。单个 NVS 数据块可避免跨配置组部分提交；独立校准操作符合称台操作顺序。验证目的：原生测试检查兼容加载、校验与存储失败时状态；浏览器模拟检查请求数、编辑保存、加载/保存错误、校准及上传反馈；编译检查 ESP32-C3 的实际兼容性；截图检查视觉层级和响应式布局。

Level: T2. Goal: one GET for calibration status and all six settings, one Save action posting all six, and a clearer page design. Anchor: load/edit/save workflow. Calibration and zero remain explicit operations because they require physical weight handling. Weight conversion and upload transport are out of scope.

Batches and boundaries: (1) storage/main: a versioned combined NVS blob with legacy-key fallback; (2) settings API: GET/POST `/api/settings`, validate all values before committing; (3) page: warm paper, deep green, numbered sections, clear units, and one save area; (4) tests and bilingual documentation. Use one `settingsCfg` write to avoid partial saves; only update runtime values after success. Disable saves when loading fails and prevent duplicate submissions; retain edits on failure. No polling or follow-up GET after actions. Acceptance: one initial data request, all six values commit together or remain unchanged, legacy settings load, mobile/desktop remain usable, and upload works. Rollback uses earlier sources/firmware and its retained legacy settings.

Plan review: pass. Native tests will verify migration, validation and failed persistence; browser tests will verify request counts, edits, load/save errors and operation feedback; ESP32-C3 compilation verifies integration, and screenshots verify layout quality.

### 完成与验证 / Completion And Validation

本轮三项需求已实现：一次获取、一次统一保存和设置页重新设计。代码复核通过：六项先校验再单次写入 `settingsCfg`，写入失败不更新运行值；旧配置键保持可读取且不会被改写。当前耗材与上料阈值冲突时拒绝整组设置。页面加载失败禁用保存，保存时禁用表单避免重复提交，出错保留编辑内容；校准与上传作为即时操作，均不触发额外参数读取。旧的两个参数接口已由 `/api/settings` 取代。

原生测试使用实际存储/API 代码及模拟 NVS/HTTP，覆盖旧键迁移、统一配置重载、单次写入、写入失败、损坏配置回退、缺失/非法值、整数间隔、活动耗材冲突。浏览器通过 CDP 明确设置并断言实际 390px/1280px 视口，覆盖一次 GET、六字段一次 POST、重复提交、加载失败/数据不全、保存失败与成功、校准和上传反馈、无后续 GET/轮询和横向溢出。截图已检查；手机标题排版已调整。耗材页交互、慢请求与超时恢复、耗材原生逻辑回归均通过。最终 ESP32-C3 编译通过：1,071,579 bytes（81%），动态内存 39,304 bytes（11%）。中英文使用说明已同步。

未刷写本轮固件；真实 NVS 断电恢复、实机校准和 OTA 上传未验证。旧版固件回退会读取保留的旧配置值，不会读取本轮 `settingsCfg` 中的新值；校准与耗材记录格式未变。决策：统一单个配置块以保证整组保存，校准/归零保留为独立动作，页面无需任何外部字体或资源。

All requested changes are implemented: one read, one unified save, and a redesigned Settings page. Code review passed: all six values validate before one `settingsCfg` write; write failure preserves runtime state. Legacy keys remain readable and unchanged. A loaded-spool threshold conflict rejects the full submission. Failed page loads disable saves, saves lock the form against duplicate requests, and errors retain edits. Calibration and upload remain immediate operations without follow-up configuration reads. `/api/settings` replaces the two earlier parameter endpoints.

Native tests exercise the actual storage/API code with simulated NVS/HTTP: legacy migration, reload, single-write commit, write failure, corrupt-data fallback, missing/invalid values, integer intervals and active-spool conflicts. Browser tests use CDP to explicitly set and assert actual 390px/1280px viewports; they cover one GET, one six-field POST, duplicate submission, failed/incomplete loading, failed/successful saving, calibration/upload feedback, no follow-up reads or polling, and no horizontal overflow. Screenshots were inspected and mobile heading layout refined. Scale interaction, slow-request/timeout recovery and inventory logic regressions pass. Final ESP32-C3 compilation: 1,071,579 bytes (81%) and 39,304 bytes RAM (11%). Bilingual documentation is synchronized.

This revision has not been flashed. Physical power-loss persistence, calibration and OTA remain unverified. Rolling back reads retained legacy configuration values rather than this revision's combined settings; calibration and inventory formats are unchanged. Decisions: use one configuration blob for group consistency, retain independent calibration/zero actions, and load no external fonts or assets.

## 简洁设置页 / Simple Grouped Settings

T1。目标：按用户反馈恢复简洁页面，按耗材识别、自动保存、称台校准、固件更新聚类。设计锚点为 `SettingsPage.h`；移除横幅、编号、装饰图形和悬浮保存栏，统一保存按钮放在两组配置之后，校准及上传作为后续操作区。任务：简化样式与布局；复核 DOM 与交互；浏览器检查和编译；同步双语说明。验收：四组清楚、手机/桌面无溢出，一次 GET/一次保存及失败反馈仍通过现有测试。方案复核通过，接口和存储无需变更。

T1. Restore a simple page grouped into spool recognition, automatic saving, scale calibration and firmware update. Anchor: `SettingsPage.h`. Remove the banner, numbering, decorative graphics and floating save bar; place the unified Save button immediately after the two configuration groups, followed by calibration and upload actions. Tasks: simplify layout/styles, review DOM and interactions, run browser checks and compilation, synchronize bilingual docs. Acceptance: clear groups, no mobile/desktop overflow, existing one-read/one-save and failure-feedback tests pass. Plan review passed; API and storage changes are unnecessary.

完成：已恢复简洁布局，四组按功能排列；六项配置的保存按钮位于自动保存组之后，校准/更新独立分组。复核通过：表单 ID、事件处理与请求路径保留，装饰横幅、编号、卡片及悬浮栏已移除。390px/1280px 浏览器测试通过，覆盖读取/保存成功与失败、缺失数据、重复提交、校准/上传反馈、无轮询及无溢出；两种尺寸截图已检查。ESP32-C3 编译通过：1,066,579 bytes（81%），RAM 39,304 bytes（11%）。README 中英文说明已同步。本轮未刷写，实机页面与操作仍待验证。

Completed: simple functional groups restored, with the six-setting Save action after automatic saving and separate calibration/update sections. Review passed: form IDs, handlers and API paths are preserved; decorative banners, numbering, cards and the floating bar are removed. Browser tests at 390px/1280px pass for load/save success and failure, incomplete data, duplicate submission, calibration/upload feedback, no polling and no overflow. Both screenshots were inspected. ESP32-C3 compilation passed: 1,066,579 bytes (81%), RAM 39,304 bytes (11%). Bilingual README updated. Not flashed; live device verification remains outstanding.


## 移除参数小字 / Remove Parameter Helper Text

T0。验收：移除六项参数的范围/用途小字及自动保存说明，保留字段、单位与输入校验。已删除对应 HTML、无用样式及失效的 aria-describedby 引用。校准操作步骤保留。

T0. Acceptance: remove range/purpose captions beneath the six parameters and the automatic-save explanatory sentence while retaining labels, units and validation. Removed the HTML, unused style and dangling aria-describedby reference. Calibration instructions remain.

复核通过；390px/1280px 现有浏览器测试通过，覆盖保存成功/失败、加载异常及输入校验。ESP32-C3 编译通过（81% 程序空间、11% RAM）。双语 README 已同步；未刷写实机。

Review passed. Existing browser tests at 390px/1280px pass, including save success/failure, load errors and validation. ESP32-C3 compilation passed (81% program, 11% RAM). Bilingual README synchronized; not flashed to hardware.

## 两页视觉一致性 / Consistent Page Styling

T1。目标：沿用简洁样式，统一耗材页与设置页的背景、标题/导航位置、字号、间距、输入框和按钮。锚点：`ScalePage.h` 与 `SettingsPage.h` 的公共视觉元素。任务：对齐基础样式与设置页导航结构；浏览器对比实际样式/布局并回归交互；编译和双语说明同步。保留耗材读数的突出显示和设置分组，不新增小字说明，不改变请求或保存逻辑。验收：390px/1280px 两页的导航、页面边距和控件一致，无溢出，原有交互通过。方案复核通过：局部样式调整，不增加资源请求。

T1. Align the simple Scale and Settings pages: background, heading/navigation placement, typography, spacing, inputs and buttons. Anchor: common visual elements in both page headers. Tasks: align styles and Settings navigation structure, compare rendered styles/layout and run interaction regressions, compile and synchronize docs. Preserve the prominent scale readout and settings groups, add no helper captions, and leave request/save behavior unchanged. Acceptance: consistent margins, navigation and controls at 390px/1280px, no overflow, existing interactions pass. Plan review passed; local styling changes require no additional resource requests.

追加反馈：移除设置就绪/持久保存说明、固件上传说明和常驻校准状态，只保留操作反馈与错误提示；测试同时验证加载成功后无常驻说明。

Additional feedback: remove persistent ready/persistence text, the firmware-upload description and calibration badge; keep action/error feedback. Tests also verify no idle captions after successful loading.

完成与复核：两页背景、标题、导航、字号、边距、按钮及输入框已对齐；统一 42px 控件高度与桌面滚动条预留位置，避免不同页面高度导致导航偏移。指定的就绪/保存说明、上传说明、常驻校准状态均已删除，错误与操作结果仍显示。浏览器实际样式与几何尺寸对比在 390px/1280px 通过；设置页正常/加载失败/数据不完整、保存及校准上传交互通过；耗材页交互回归通过。截图检查通过。ESP32-C3 最终编译通过：1,066,091 bytes（81%），RAM 39,304 bytes（11%）。中英文文档同步完成；未刷写，实机检查待进行。

Completion/review: both pages now match backgrounds, headings, navigation, fonts, margins, buttons and inputs. Controls are 42px tall and stable scrollbar gutters prevent navigation shifts caused by different page heights. Requested idle/save descriptions, upload instructions and persistent calibration state are removed; errors and action results remain. Rendered-style and geometry comparisons pass at actual 390px/1280px viewports; Settings success/load-failure/incomplete-data and save/calibration/upload interactions pass, as do Scale interaction regressions. Screenshots inspected. Final ESP32-C3 build: 1,066,091 bytes (81%), RAM 39,304 bytes (11%). Bilingual docs synchronized; not flashed, hardware verification remains pending.


## 设置与返回按钮 / Settings And Back Buttons

T0。将两页 Tab 改为页头按钮：耗材秤右上角“设置”链接 `/settings`，设置页右上角“返回”链接 `/`。验收：Tab 不存在、两页跳转目标正确、页头及控件样式一致、手机布局无溢出。保留称重页连接状态与现有请求逻辑。

T0. Replace tabs with header navigation buttons: Settings links to `/settings` on the Scale page, and Back links to `/` on Settings. Acceptance: no tabs, correct destinations, consistent header/control styling and no mobile overflow. Keep connection status and request behavior.

复核通过：导航使用可直接访问的链接按钮，“返回”固定回到秤页，不依赖浏览器历史。390px/1280px 的按钮位置、样式、目标和无 Tab 断言通过；两页交互回归通过；手机截图已检查。ESP32-C3 编译通过（1,066,211 bytes，81%；RAM 11%），双语说明已同步。未刷写设备。

Review passed: native links provide button navigation; Back reliably opens the Scale page without depending on browser history. Button position/style, destinations and absence of tabs pass at 390px/1280px; both-page interaction regressions pass and mobile screenshots were inspected. ESP32-C3 compilation passed (1,066,211 bytes, 81%; RAM 11%). Bilingual docs synchronized. Not flashed.

## 页面压缩缓存与 Wi-Fi 恢复 / Compressed Pages, Cache And Wi-Fi Recovery

级别：T3（网络凭据及备用热点涉及访问边界；用户已明确授权实现页面压缩缓存、备用 AP、网页配置 Wi-Fi 及成功后关闭 AP）。目标：两页预压缩并用 ETag 条件缓存；Wi-Fi 连接失败时通过受密码保护的热点继续提供两页；设置页统一读取/保存 Wi-Fi 与称重配置，连接成功后关闭 AP。热点密码方式待用户偏好，其余工作可独立推进。非目标：互联网可用性检测、自动固件下载、改变称重逻辑、增加设置页轮询。

设计锚点：从网络失败到网页修复的端到端流程。任务树/边界：1. 资源生成工具、缓存协商与两页路由；2. 网络配置验证、兼容迁移与整体持久化；3. 独立 Network.ino 的非阻塞连接/AP状态与主循环接入；4. 设置页 Wi-Fi 分组；5. 原生/模拟网络/浏览器测试、编译与双语文档。分批依次实现、复核、验证。缓存可回退原始 HTML；旧配置键保留供旧固件读取。

约束：浏览器支持 gzip 才压缩发送，否则发送原文；ETag 随内容及编码变化，条件命中返回 304，数据接口不缓存；生成资源与源页面须有过期检查。网络变化先保存并返回响应，再开始连接；新网络错误时 AP 保持可用；持久化失败不部分更改参数。正常 Wi-Fi 密码不能返回给浏览器或串口；新密码空白仅在同 SSID 时保留，开放网络须显式选择。备用 AP 连接后提供 `/` 与 `/settings`，连接稳定取得 IP 后关闭。网页服务只在网络接口就绪时启动，不能因 STA 断线停止正在服务的 AP。OTA 上传期间暂缓网络切换。

风险：AP/STA 共用射频，连接时热点信道可能变化；缓存过期资源；密码错误锁出；NVS 部分写入；断电恢复。措施：稳定窗口、版本化单块设置、预生成 gzip 校验、独立错误/配置反馈、保持 AP 重试及明确的手动访问地址。验收覆盖 gzip/原文/304、缓存更新、原配置迁移、无效与失败保存、启动失败/运行中断网/AP恢复/成功关闭/计时器回绕、一次读取与保存、两页控件一致。方案复核通过；实现授权来自本次用户请求，不执行实机刷写。

Level: T3 because Wi-Fi credentials and fallback AP affect access boundaries. The user explicitly authorized compression/cache, fallback AP, web Wi-Fi configuration and AP shutdown after successful connection. Anchor: recover web access after Wi-Fi failure. Batches: (1) generated gzip assets, negotiation and conditional cache; (2) credential validation and atomic versioned storage with migration; (3) nonblocking network lifecycle; (4) Wi-Fi settings group; (5) native/network/browser verification, build and bilingual docs. AP password preference is pending; independent work proceeds. Exclude internet checks, firmware downloads, weighing changes and Settings polling.

Invariants: gzip only when accepted, identity otherwise; content/encoding-specific ETags and 304, no cached API data; generated-source freshness checks. Persist and respond before network changes, retain AP on failed connections, preserve runtime values on storage failure. Never return/log station passwords; blank passwords retain credentials only for the same SSID, with explicit open-network selection. Start HTTP only after an interface is ready and keep it available on AP when STA disconnects. Close AP only after a stable IP connection; defer network changes during OTA.

Risks include shared AP/STA channel changes, stale assets, credential lockout, failed storage and power loss. Mitigate with a stable connection window, one versioned settings blob, asset checks, clear save/error feedback and AP recovery. Validate encoded/identity/304 responses and invalidation, legacy migration, bad/failed saves, startup failure, disconnection, recovery/AP shutdown/timer wraparound, single-read/save and consistent pages. Plan review passed; implementation authorization is the user's request, without device flashing.

决策更新：用户明确要求备用热点不设密码，故 AP 使用开放模式，名称为设备唯一后缀的 `SpoolScale-xxxxxx`；不生成或存储 AP 密码。Wi-Fi 保存使用页面首次获取的会话令牌，防止无令牌跨站提交；普通网络密码不回显。称重与 Wi-Fi 改用 `deviceCfg` 单块持久化，兼容读取旧设置键。

Decision update: the user explicitly selected an open fallback AP. Use `SpoolScale-xxxxxx` with a device-specific suffix and no generated/stored AP password. Settings writes require the session token obtained by the initial same-origin read; station passwords are never returned. Weighing and Wi-Fi settings commit in one `deviceCfg` blob with legacy fallback.

### 完成、复核与验证 / Completion, Review And Validation

五批实现完成。复核通过：页面 gzip/原文按协商发送，ETag 按编码及内容隔离，304 不带页面正文；生成资源带源内容编译期校验。新配置先整组写入 `deviceCfg` 再更新运行值，旧键保持兼容；SSID 正确转义，正常 Wi-Fi 密码不回显/不记录，会话令牌防止无令牌配置提交。网络失败 30 秒后开启用户要求的开放 AP；无配置立即开启；修改网络先启动 AP，回应后延迟切换；必须关联目标 SSID、取得 IP 且稳定 10 秒才关 AP。AP/域名服务启动失败有退避，OTA 期间延后切换。设置页单次读取/统一保存保持不变，网络切换不新增轮询。

证据：原生测试验证实际页面处理函数的 gzip/原文/304/ETag 失效与缓存响应头，生成器回解 gzip 与源内容一致；网络策略覆盖阈值和计时器回绕，实际 Network.ino 模拟覆盖启动失败、DHCP 未完成、关联错误网络、掉线恢复、AP 启动失败、保存后的延迟切换、OTA 延后及 mDNS 重试退避。存储/API 测试覆盖旧格式迁移、重载、整组写入失败、非法参数、密码保留/开放网络、无配置仍可保存称重参数、令牌拒绝和密码不回显。390px/1280px 浏览器测试覆盖 Wi-Fi 修改、无密码网络、空初始配置、输入与失败反馈、一次 GET/POST、无轮询和两页样式一致；截图已检查。耗材页交互、轮询超时与原生逻辑回归通过。

最终 ESP32-C3 编译通过：1,131,573 bytes（86%），RAM 41,976 bytes（12%）。页面体积：耗材页 14,680 → 4,915 bytes，设置页 16,155 → 5,313 bytes。双语 README 已同步，提供 app `.bin` 供原有在线更新使用。本轮未向真实设备刷写；实际无线延迟、AP/STA 信道切换、mDNS 客户端兼容及断电持久化仍需实机验证。回退旧固件会使用保留的旧称重设置和旧固件内的 Wi-Fi 默认值，不读取 `deviceCfg`；校准及耗材数据格式未变。

All five batches are complete. Review passed: negotiated gzip/identity responses, content/encoding-specific ETags, empty 304 bodies and compile-time generated-source freshness checks. One `deviceCfg` commit precedes runtime changes, retaining legacy keys. SSIDs are escaped, station passwords are not returned/logged, and configuration writes require the session token. The user-requested open AP starts after 30 seconds offline or immediately without configuration. AP starts before credential changes, which are delayed until after the response. AP closes only after the target SSID has an IP for ten stable seconds. AP/mDNS startup failures back off; OTA defers switching. Settings retains one read/save and no connection polling.

Evidence: native tests of actual page handlers cover gzip/identity/304, ETag misses and response headers; generated gzip decompresses to source. Policy tests cover thresholds and wraparound; the actual Network.ino simulator covers failed startup, missing DHCP, wrong SSID, runtime disconnection, failed AP creation, deferred application, OTA deferral and mDNS retry backoff. Storage/API tests cover migration/reload, failed atomic writes, validation, password retention/open networks, saving weighing settings before network configuration, token rejection and password non-disclosure. Browser tests at 390px/1280px cover Wi-Fi changes/open/unconfigured cases, validation/failure feedback, one GET/POST, no polling and consistent styles. Screenshots inspected; scale interaction, timeout recovery and native inventory regressions pass.

Final ESP32-C3 build: 1,131,573 bytes (86%), RAM 41,976 bytes (12%). Scale HTML: 14,680 → 4,915 bytes; Settings: 16,155 → 5,313 bytes. Bilingual README synchronized and app .bin prepared for existing OTA upload. Not flashed to hardware: actual radio latency, AP/STA channel switching, client mDNS support and power-cycle persistence remain unverified. Rollback uses retained legacy weighing settings and the earlier firmware's Wi-Fi defaults, not deviceCfg; calibration/inventory formats are unchanged.

### 接口上传尝试（2026-11-22） / API Upload Attempt (2026-11-22)

用户明确授权跳过网页，向 `192.168.3.105` 通过接口上传本轮固件。目标 ARP MAC 已核对为 `b8:1f:3f:2b:ad:1c`；app 二进制 1,131,728 bytes，SHA-256 与交付校验文件一致。预检 `/api/settings` TCP 建连约 9 ms，但 15 秒内无 HTTP 返回。上传 `/api/update` 在 curl 累计发送 137,633 bytes 后连续 45 秒无进展而中止（总约 51 秒），未收到成功响应。之后 `/api/reading` 20 秒无响应，4 次 ping 均成功，平均约 23 ms。未确认部署成功；已请用户断电重启以恢复 HTTP 服务后继续接口上传。

The user explicitly authorized uploading the prepared firmware via API to 192.168.3.105. ARP matches b8:1f:3f:2b:ad:1c; the 1,131,728-byte app binary matches its SHA-256 sidecar. Settings connected in about 9 ms but returned no HTTP data within 15 seconds. The update request stalled after curl reported sending 137,633 bytes and aborted after 45 seconds without progress (about 51 seconds total); no success response arrived. A subsequent reading request timed out after 20 seconds, while all four pings succeeded at about 23 ms average. Deployment is not confirmed. Asked the user to power-cycle the board before continuing the already-authorized API upload.


## 保持行为的代码精简（2026-11-22） / Behavior-preserving simplification

### Intake / 接收
T2。目标：消除死代码、NVS 校验和接口查找重复，保持页面、HTTP 状态/正文、传感器行为、热点策略、旧配置迁移和存储格式。非目标：修复弱网、变更访问控制、删除兼容性、部署。验收：实际存储/接口的成功及失败场景测试通过；现有回归及 ESP32-C3 编译通过；报告精简量与剩余风险。
T2. Remove dead code and duplicated NVS validation/API lookup. Preserve pages, HTTP responses, sensor behavior, AP policy, legacy migration and storage layouts. Exclude weak-network fixes, access-control changes, compatibility removal and deployment. Acceptance: actual storage/API happy/edge tests, existing regressions and ESP32-C3 compilation pass; report reduction and residual risks.

### Design / 设计
锚点：配置和耗材请求从读取、校验到回应的原有路径。任务树/边界：主循环清理未使用变量；存储共用版本化数据读取和校验；库存 API 共用相同语义的 ID 查找；测试与双语文档同步。保留领域校验、长度/读取/版本/校验和的检查顺序，offsetof 保留原结构布局。选用的无效 ID 仍返回 409，编辑/删除仍返回 400/404。
Anchor: existing configuration/inventory read-validation-response flows. Boundaries: loop cleanup; shared versioned storage validation; shared identical inventory lookup; tests/docs. Preserve domain checks, length/read/version/checksum order and offsetof-based layout. Select retains invalid-ID 409; edit/delete retain 400/404.

### Batches, risks and rollback / 批次、风险与回退
1. 删除未使用 Wi-Fi 状态值；将网络测试热点名称期望对齐当前代码；运行网络回归。
2. 提取类型安全的存储校验辅助函数，替换五处重复读取校验；验证缺失、损坏和版本错误。
3. 合并编辑/删除的 ID 查找及相同错误响应；实际 API 验证成功、无效/不存在 ID、保存失败；最终编译、回归和文档同步。
1. Remove unused Wi-Fi local; align AP-name test with current behavior; network regressions.
2. Extract type-safe storage helpers for five reads; test missing/corrupt/wrong-version data.
3. Share edit/delete lookup/errors; test actual handlers for success, invalid/missing IDs and save failure; compile, regress and sync docs.
风险：Arduino 模板处理、校验和覆盖范围变化、错误响应语义漂移。措施：独立轻量头、结构/键/顺序不变、实际处理函数测试和目标板编译。三批均有本会话 work/simplify-before 源码快照作为回退点；无 Git reset、NVS 迁移或刷写。
Risks: Arduino template preprocessing, checksum coverage and response drift. Mitigate with a standalone header, unchanged structs/keys/order, actual handler tests and board compilation. Source snapshots in this chat's work/simplify-before provide rollback for all batches; no Git reset, NVS migration or flashing.

### Review-Plan / 方案复核
PASS。三批顺序实施，每批复核/测试通过后继续；保留 gzip 资源、旧配置迁移和失败回滚。当前用户请求授权本地重构，主代理完成复核与验证。
PASS. Three sequential batches with review/test gates. Keep gzip resources, legacy migration and failure rollback. User request authorizes local refactoring; main agent performs review/validation.

批次 1 复核/测试：PASS。仅删除无使用者的状态读取，未改变网络策略；热点名称测试对齐现有 Scale 前缀，实际 Network.ino 的正常连接、失败、断开、延迟应用和 OTA 延后模拟通过。允许进入批次 2。
Batch 1 review/test: PASS. Removed an unused status read without changing network policy; aligned the test with the existing Scale prefix. Actual Network.ino lifecycle happy/failure/disconnect/deferred-apply/OTA tests pass. Proceed to batch 2.

批次 2 复核/测试：PASS。五类 NVS 数据共用长度、读取、版本、offsetof 校验和验证，结构和领域校验保留。配置迁移/重载/整组写入失败，以及新增缺失、短读、长度/版本/校验和损坏和库存超限用例通过。ESP32-C3 检查点编译通过：1,131,195 bytes，RAM 41,976 bytes。允许进入批次 3。
Batch 2 review/test: PASS. Five NVS records share envelope validation; layouts and domain checks remain. Migration/reload/atomic-write failures and new missing/short-read/size/version/checksum/count rejection cases pass. ESP32-C3 checkpoint compile: 1,131,195 bytes; RAM 41,976 bytes. Proceed to batch 3.


### 批次 3、最终复核及关闭 / Batch 3, final review and close
PASS。编辑/删除只共用完全相同的 400/404 路径，选用仍保留 409；删除日志从已移除记录读取相同 ID。新增库存接口测试在重构前后均通过，验证实际处理函数的编辑/删除持久化、无效/缺失 ID、领域拒绝、保存失败回滚及活动卷拒绝。
PASS. Edit/delete share only identical 400/404 paths; Select retains 409. Delete logging reads the same ID from the removed record. New actual-handler inventory tests pass before and after refactoring, proving persistence, invalid/missing IDs, domain rejection, failed-save rollback and active-record protection.

最终验证全部通过：五项原生测试；统一设置/存储和实际库存 API 两组测试；页面压缩资源同步检查；延迟/超时轮询；两页 390px/1280px 交互/错误路径。ESP32-C3 相同环境编译：改前 1,131,229，改后 1,131,189 bytes（86%），RAM 同为 41,976 bytes。固件源码排除生成资源为 1,869 → 1,860 行，87,612 → 86,862 bytes；增加的测试/文档不计为固件精简。
Final validation passed: five native suites, unified settings/storage and actual inventory API suites, generated-page freshness, delayed/timeout polling and two-page interactions/error paths at 390/1280px. Identical ESP32-C3 builds use 1,131,229 before vs 1,131,189 bytes after (86%); RAM stays 41,976 bytes. Firmware source excluding generated assets: 1,869 → 1,860 lines; 87,612 → 86,862 bytes. Added tests/docs are excluded from source-reduction claims.

决策/文档同步：README 双语说明已更新，热点/主机名及 RSSI 说明与当前代码对齐；保留既往 PLAN 历史。未删除兼容迁移、缓存、压缩、备用 AP 或回滚保护，也未修改页面源码。三批完成，代码复核通过。残余风险：无硬件/断电/实际无线验证；旧有弱网 HTTP 阻塞和访问控制问题未在本轮修复。无需部署批准，本轮未部署。回退源码快照保存在当前会话 work/simplify-before；按批次恢复文件，不重置整个仓库。
Decision/doc sync: bilingual README updated, current names/log descriptions aligned; historical PLAN entries retained. Keep migration/cache/gzip/fallback AP/rollback protections; page sources unchanged. All three batches complete; code review passed. Residual risks: no hardware/power-loss/RF tests; existing weak-network HTTP blocking and access-control issues remain open. No deployment performed. Restore relevant files from this chat's work/simplify-before snapshots per batch, without resetting the repository.


## HTTP 慢连接与轮询退避（2026-11-22） / HTTP time bounds and polling backoff

### Intake / 接收
T2。目标：正常轮询仍为上一轮结束后 2 秒；连续读取失败逐步退避，成功恢复；写操作暂停轮询且不与读取重叠；普通 HTTP 慢连接限时释放。非目标：改成 5 秒、增加并发服务器、修改存储/称重/鉴权、部署。上传与普通请求分开限时，不能用短普通请求期限中断正常固件更新。
T2. Keep normal polling at two seconds after completion, back off failed reads and reset on success, pause/serialize reads during writes, and release slow ordinary HTTP connections. Exclude five-second polling, concurrent-server migration, storage/weighing/authentication changes and deployment. Treat firmware upload separately from short ordinary-request limits.

### Design / 设计
锚点：浏览器请求排队 → 设备接收完整请求 → 处理 → 限时发送/释放。任务树及边界：ScalePage 统一读写队列/单个定时器/4,8,16,30 秒失败退避/12 秒写超时；BoundedWebServer 本地适配当前 Arduino-ESP32 3.3.11，普通请求先无阻塞检查完整头/正文、4 秒总接收期限、2 KiB 头及 4 KiB 总请求限制，发送最多等待 2 秒；现有 WebServer 仍处理路由/参数/业务，OTA 流式解析保留并缩短停滞等待。生成资源、测试与双语文档同步。
Anchor: browser queue → complete device request → handler → bounded send/release. Boundaries: shared browser queue/single timer with 4/8/16/30-second backoff and 12-second write deadline; local BoundedWebServer adapter for core 3.3.11, nonblocking ordinary-request preflight with a four-second receive deadline, 2 KiB headers/4 KiB total request cap and two-second send budget; keep existing routing/parsing and streaming OTA with a separate idle wait. Regenerate assets and sync tests/docs.

不变量：只允许一个本页请求在途；写操作完成后先刷新状态，再恢复轮询；写超时不能自动重发非幂等操作。普通请求不完整时不得调用业务处理函数。首字节/慢头/慢正文均计入接收期限；响应分块共享发送期限。OTA 保留库流式解析，使用独立的字节停滞等待和在上传回调检查的总时限；不能把回调检查称为逐字节硬总期限。传感器业务执行时间不由网络发送超时中断。
Invariants: one request in flight per page; refresh after a successful write before polling resumes; never automatically retry uncertain mutations. Do not dispatch incomplete ordinary requests; receive deadline includes first-byte/header/body waits and response chunks share one send deadline. OTA retains library streaming with a separate byte-idle wait and total-time checks at upload callbacks, not a claimed per-byte hard total deadline. Network send deadlines do not interrupt sensor computation.

### Batches / 批次
1. 浏览器调度与退避；用真实页面脚本验证精确间隔、失败恢复、慢读/写互斥与写超时。
2. 设备适配与上传限制；用真实适配代码和原生 socket 验证无数据、慢头/体、滴流总期限、正常处理、失败后下一连接、发送不读客户端、OTA 路径；目标板编译。
3. 整体回归、压缩资源校验、代码复核、双语文档及交付说明。
1. Browser scheduling/backoff; actual-script tests for timings, recovery, read/write exclusion and write deadlines.
2. Device adapter/upload limits; native-socket tests of the actual adapter for empty/slow/dripping requests, success, next-client recovery, non-reading receivers and OTA path; board compile.
3. Full regressions, asset checks, code review and bilingual docs/delivery notes.

风险/回退：依赖核心受保护字段与 socket peek；严格限制可能拒绝非页面大请求；浏览器超时不代表设备未提交；弱网正常请求也可能被终止。使用最小本地子类不改安装库，保留接口响应与上传，记录丢弃原因；模拟+socket+编译验证，实机无线仍待测。各批源码快照在本会话 work/http-before；恢复本批文件及生成资源，不回退上一轮精简。
Risks/rollback: protected-core/socket-peek dependency, rejection of oversized external requests, uncertain writes and terminating legitimately slow RF transfers. Use a small local subclass without editing installed libraries, retain handlers/upload, log drop reasons and validate via mocks/sockets/board compilation. Real RF remains untested. Per-batch snapshots are in this chat's work/http-before; restore affected files/assets without undoing prior simplification.

### Review-Plan / 方案复核
PASS。当前用户已授权两项行为变更；三批顺序实施并逐批复核测试，无部署。上传库限制明确记录，不宣称解决所有业务阻塞。
PASS. User authorized the two behavior changes; proceed in three gated batches without deployment. Document streaming-library limits rather than claiming all handler blocking is solved.

批次 1 复核/测试 PASS：真实页面脚本的确定性时钟测试确认 4/8/16/30 秒退避、成功恢复 2 秒、已有读取完成后再写、写入期间无轮询、12 秒写超时后解除 busy 且不重发。Chrome 延迟/超时轮询及两种宽度的库存交互回归通过。进入批次 2。
Batch 1 review/test PASS: deterministic actual-script tests prove 4/8/16/30-second backoff, two-second recovery, read-before-write serialization, paused polling, and twelve-second write timeout without resend. Chrome delayed/timeout polling and inventory interactions at both widths pass. Proceed to batch 2.


### 批次 2 复核/测试 / Batch 2 review/test
PASS。普通 HTTP 先完整性检查再业务分派；接收滴流不重置期限；头/正文发送共用预算。原生 socket 检查空连接、慢头/体、滴流、超大/非法头、正常分段到达、坏连接后的正常连接、不读响应和分段发送；实际 OTA 处理函数检查独立空闲等待设置、回调期限、失败不重启和成功重启安排。适配层使用非阻塞 socket；未修改安装库。最终板型编译通过（1,135,749 bytes，RAM 46,096 bytes）。进入批次 3。
PASS. Ordinary requests dispatch only after preflight, trickles do not reset receive time and response chunks share a budget. Native sockets cover empty/slow/trickled/invalid requests, normal segmented arrival, next-client recovery and non-reading receivers. Actual OTA handlers cover timeout setup, callback budget, failure without reboot and scheduled reboot on success. Adapter sockets are nonblocking; installed libraries unchanged. Board compile passed: 1,135,749 bytes; RAM 46,096 bytes. Proceed to batch 3.

### 最终复核、测试、决策和文档同步 / Final review, test, decisions and doc sync
PASS。七项原生测试、两项实际 API 测试、确定性页面调度测试、Chrome 轮询与两页 390px/1280px 交互/失败路径、gzip 资源同步全部通过。写入响应头已收到而正文挂起也会在 12 秒超时；没有自动重发写操作。页面资源 15,456 → 5,173 bytes。代码复核确认无存储、称重和 Wi-Fi 策略变更，局部适配使用核心受保护字段，版本升级须回归。
PASS. Seven native suites, two actual-API suites, deterministic scheduling, Chrome polling/two-page interactions and failures at 390/1280px, and asset freshness passed. Write-body stalls also hit the twelve-second deadline without resend. Scale assets compress 15,456 → 5,173 bytes. Review confirms no storage/weighing/Wi-Fi policy change; the adapter uses protected core fields and needs regression on core upgrades.

决策：普通请求采用严格小请求预算；保留现有 OTA 流式库，不扩大为上传解析器重写。上传文件开始前的 multipart 元数据仍使用库等待；文件上传是字节空闲等待加回调总时间检查，不能宣称完整硬期限。传感器业务阻塞仍存在。README 中英文说明和本次交付说明已同步。未实机验证/刷写，无法承诺实际弱网改善幅度；回退点 work/http-before，不回退此前精简。三批完成。
Decision: strictly bound small ordinary requests while retaining the OTA streaming library; do not expand into a parser rewrite. Multipart metadata before file start retains core waits; upload idle/callback checks are not a full hard deadline. Sensor-handler blocking remains. Bilingual README/delivery notes synchronized. No RF hardware validation or flashing; no measured improvement claim. Roll back from work/http-before without undoing prior simplification. Three batches complete.


## API 固件更新结果（2026-11-22） / API firmware deployment result

用户明确授权持续通过 API 重试直至上传成功，且不请求页面。已向 192.168.3.105 的 POST /api/update 上传 HX711WebScale-http-recovery.esp32c3.bin（app 1,135,904 bytes；SHA-256 d83cda724544facd49cab416f7d7af2efb56c0b6f3c811b5e440b24a27c13db9）。设备 MAC b8:1f:3f:2b:ad:1c 已核对。多次断开后，最终一轮传输 multipart 1,136,151 bytes、耗时 28.338 秒，明确返回 HTTP 200「更新成功，设备即将重启」，重试循环自动结束。

User explicitly authorized persistent API retries and no page requests. Uploaded the verified ESP32-C3 app to POST /api/update at 192.168.3.105, matching MAC b8:1f:3f:2b:ad:1c. After repeated disconnects, the successful multipart transfer sent 1,136,151 bytes in 28.338 seconds and returned HTTP 200 with explicit update-success/reboot text; retries stopped automatically.

重启后只用 API 验证：/api/reading HTTP 200，0.101566 秒；/api/settings HTTP 200，0.041615 秒；/api/spools HTTP 200，0.049577 秒。启动 revision 改变，新版 Wi-Fi/会话字段和 Scale-1cad2b 热点标识存在。遵循用户追加约束后未请求网页。传输客户端发送字节数不作为成功依据，以最终明确成功响应和重启后接口为依据。

Post-reboot API-only validation: reading 200 in 0.101566s, settings 200 in 0.041615s, spools 200 in 0.049577s. Boot revision changed and new configuration/session fields plus Scale-1cad2b identity are present. No page requests after the user's API-only instruction. Success is based on the explicit device response and post-reboot APIs, not client-side sent-byte counts.

重要未解决项：上传前 /api/reading 报 calibrated=true、count=2；重启后 calibrated=false、count=0，/api/spools 为 []。新版本没有读取到旧校准和耗材记录；尚不能确认原 NVS 数据是否仍保留或旧固件格式/命名空间是否不同。未执行校准、设置保存、新建或删除，不宣称数据兼容性验证通过；需要单独核查/恢复，避免覆盖旧数据。设备上传目标已完成，该数据读取问题明确保留。

Open issue: pre-upload API reported calibrated=true and count=2; afterward calibrated=false/count=0 and spools=[]. The new version did not load the old calibration/inventory. Underlying NVS retention and old-format/namespace compatibility remain unverified. No calibration/settings-save/create/delete was performed; data-compatibility acceptance has not passed. Investigate/recover separately before overwriting old data. Firmware deployment is confirmed; this data-loading issue remains open.


# 页面修复 / Page repair

设置页移除字段类型、地址、阈值、密码、保存结果、砝码重量与固件扩展名的客户端校验；关闭表单原生校验。字段缺失不阻断其他设置显示。保留设备端校验、HTTP/JSON 错误提示、重复提交锁和文件选择要求。
Removed client field/type/address/threshold/password/save-result/calibration-input/firmware-extension validation and native form validation. Missing fields no longer block other settings. Server validation, HTTP/JSON errors, duplicate-submit locks and file selection requirements remain.

称重页面明确提示未校准、提供设置入口并禁用无效去皮；设置页显示校准状态。未将原始读数当克数，也未执行归零或校准。
The weighing page explains the uncalibrated state, links to settings and disables unavailable tare. Settings displays calibration status. Raw samples are never shown as grams; no tare/calibration was performed.

## Review-Code / 代码审查
通过：页面不再检查主机名；只修改页面与回归测试、生成资源，持久化格式与服务端校验未改。检查错误提示、忙碌状态、失败恢复及校准后恢复控件。
Pass: no hostname validation; changes limited to pages, tests and generated resources, with no persistence/server-validation changes. Reviewed errors, busy state, failure recovery and controls after calibration.

## Test / 验证
浏览器 390/1280px：设置正常加载、真实 scale-*.local 地址、辅助字段缺失、网络错误、损坏 JSON、保存失败、重复提交、Wi-Fi 保存与 OTA 界面通过；称重正常流程、未校准提示及恢复通过。
Browser 390/1280px: settings/current hostname/missing auxiliary fields/network errors/malformed JSON/save failures/duplicate submission/Wi-Fi and OTA UI passed; weighing flow and calibration-state recovery passed.
2 秒轮询、4/8/16/30 秒退避、写时串行及超时回归通过。实际设置/存储及耗材 API 原生测试通过。生成资源一致性通过。
Polling (2 seconds), 4/8/16/30-second backoff, serialized writes and timeout recovery passed. Actual settings/storage/inventory API tests and generated-resource consistency passed.
ESP32-C3 编译通过：程序 1,133,317 字节，RAM 46,096 字节。
ESP32-C3 build passed: program 1,133,317 bytes; globals 46,096 bytes.

## Decision / 决策与限制
reading 接口暂不删除字段：raw 可移至诊断接口，count 可由列表推导，loaded 不能由 state 推导（检测到上料但稳定确认前可能 loaded=true/state=0）；listRevision 保留以减少列表请求。该精简尚未实施。
Reading schema unchanged: raw could move to diagnostics, count derives from list, loaded cannot be derived from state (loaded=true/state=0 is possible before stable confirmation); retain listRevision to reduce requests. This reduction has not been implemented.
旧校准与两条耗材未加载的问题独立存在，本次页面修改不代表数据恢复。尚未进行可控弱信号实测。回退可恢复本次源码快照及上一版固件；回退不能保证恢复旧数据。
Prior calibration and two records are still not loaded; page fixes do not constitute data recovery. No controlled weak-signal test. Rollback uses source snapshot/prior firmware and does not guarantee data recovery.


# 四字段读数与耗材模型改造 / Four-field reading and spool model

## Intake · T3 · Authorized / 已授权
目标：reading 仅 raw/state/id/value；按稳定总重量唯一匹配耗材；自动保存仅降低已确认耗材总重量，余量在页面计算；文档、代码、测试完成后 API 更新设备。
Objective: reading contains only raw/state/id/value; uniquely match stable total weight, decrease confirmed spool totals only during automatic saves, calculate remaining in browser, document/test/build then deploy via API.
用户本轮明确要求实现并上传，满足 T3 实施授权。上一轮旧版上传循环已退出，无上传进程；不再上传旧版。
Explicit user instruction authorizes T3 implementation/deployment. Previous upload loop exited; obsolete firmware will not be uploaded.
非目标：重校准、清空存储、猜测恢复无法读取的旧数据、扩大网络架构改动。
Non-goals: recalibration, clearing storage, guessing inaccessible data, network architecture changes.

## Design anchor / 设计锚点
锚点：稳定上料→匹配/人工确认→使用递减→下料流程；InventoryLogic/Measurement/InventoryApi/ScalePage。
Anchor: stable load → matching/manual confirmation → consumption-only decrease → unload.
reading: raw 为最近 HX711 整数；state 优先 98 数据超过 2 秒未更新、99 未校准，否则 0 下料/1 上料；value 为相对空台总克数，98/99 时 null；id 仅在上料且当前稳定且确认时非零。
Reading: latest raw integer; state priority 98 for stale/missing sample (>2s), 99 uncalibrated, otherwise 0 unloaded/1 loaded. Value is gross grams relative to base zero, null for 98/99. Nonzero id requires loaded, currently stable and confirmed.
阈值之间采用迟滞：达到上料阈值变为上料，确认空台后下料；未确定初态保持下料。瞬时抖动使对外 id=0，内部身份保留直至下料；恢复稳定不会把使用中的耗材重新匹配。
Hysteresis between empty/load thresholds. Initial transition is unloaded. Unstable readings expose id=0 while keeping internal identity until unload; stabilization retains the same identity.
耗材记录使用独立于校准参数的克数：id/material/color/tare/total。新建空盘重量=稳定总重−设置初始净重，负数拒绝创建；仅迁移的未知旧空盘保留 tare=null。v1 原始读数记录只在有效校准时转换，旧键保留；新格式单独 recordsV2，迁移失败禁止写入覆盖。
Spool record: id/material/color/tare/total in grams, independent of calibration parameters. New tare=stable total−configured initial net weight; reject negative tare. Only legacy unknown tare remains null. Convert legacy raw v1 only with valid calibration; retain old key and write new recordsV2 separately. Failed migration blocks writes.
自动匹配必须唯一且在容差内；歧义和无匹配都 id=0。人工选取与新建必须当前稳定，原子保存当前稳定总重后立即确认；失败回滚，不发布成功。
Matching requires exactly one candidate within tolerance. Ambiguous/unmatched => id=0. Manual selection/creation requires stable load, persists current stable total before confirming, rolls back on failure.
使用更新只接受减少；id=0 或不稳定不写；定期检查最小变化与间隔，下料前保存最后稳定的更低重量。不会因为外部加重自动增大历史总重，人工选择允许重新同步总重。
Consumption writes only decreases with confirmed stable identity and configured interval/minimum change. Save last lower stable total on unload. Manual selection explicitly permits total resync upwards.
页面保留普通/耗材显示切换，普通去皮在浏览器本地偏移，总重接口不受影响。耗材列表独立请求，使用 ETag/304 与低频同步替代 reading 的列表版本字段，操作后立即刷新；请求始终串行，2 秒正常轮询及退避保留。
Browser retains display modes, ordinary tare uses local offset so reading remains gross. Separate inventory endpoint uses ETag/304 and periodic sync instead of reading revision fields; mutations force refresh. Serialized requests, 2s polling/backoff preserved.

## Task tree / 模块与批次
1. InventoryLogic + Storage：克数模型、只减规则、v1→v2只读迁移、存储失败保护。回退点：源码快照与旧键。
2. Measurement + InventoryApi + main + OTA：迟滞/稳定身份、四字段返回、人工确认、持久化事务。回退点：批次1验证结果。
3. ScalePage + SettingsPage + generated/tests：状态渲染、页面余量与去皮、列表缓存、保留取消校验改动。回退点：批次2测试通过。
4. 全面审查/测试/双语文档/编译/API部署，收到成功响应并验证四字段与列表 API。
1 model/storage; 2 measurement/API/main/OTA; 3 browser/generated/tests; 4 review/test/bilingual docs/build/API deploy. One verified batch at a time.
依赖：1→2→3→4。存储格式和页面接口需成套升级；不混用旧固件。风险：重量歧义不自动识别；RF 差导致 OTA 中断；已存在旧数据未读取异常仍未恢复；无实物操作不能验证真实上料/卸料。
Dependencies: 1→2→3→4. Firmware/UI/storage evolve together. Risks: ambiguous weights, OTA interruptions, prior unreadable data, no physical live load/unload test.

## Review-Plan / 计划审查
通过：主流程与用户规则逐项对应；仅识别/库存/页面范围重构；保留原始持久化数据；不自行校准；API上传已授权。用户已明确新建空盘重量按总重减设置初始重量计算，其他参数全部保留。
Pass: scope matches requested flow, preserves legacy bytes, no automatic calibration, deployment authorized. User confirmed tare calculation from configured initial net weight; retain all settings parameters.

## Acceptance and purpose-driven validation / 验收
- 精确四个字段，覆盖98优先99、0/1迟滞、稳定/抖动身份、有效/无效克数。
- 唯一匹配、边界容差、双候选、未匹配、重上料，人工选取后立即同步总重与id。
- id0及抖动无自动写；消耗下降保存、增加不写、最小变化/时间边界、下料保存失败回滚。
- v1迁移、缺校准保护、v2重启、损坏/短读/写失败、旧blob不改动。
- 两页移动/桌面浏览器流程、页面自行余量、98/99/0/1、普通本地去皮、列表304与串行退避。
- ESP32-C3编译、生成一致；API上传200并重启后reading精确字段确认。不通过页面GET验证。
- Exact schema/status/identity; matching and ambiguity; decreasing writes and rollback; safe migration; browser remaining/status/local tare/cache/serialization; target build and API-only OTA verification.


# 四字段改造验证与交付 / Four-field implementation verification

## Review-Code / 代码审查
独立执行模式，自审通过：reading 精确四字段；状态98优先99；确认ID受稳定性保护；自动更新只减不增；人工选取原子同步；新建空盘由配置净重推算；失败回滚；新旧存储键分离；列表失败不伪报称重断线。审查中修正了 ESP32 32位 strtoul 的 ID 溢出边界，改用64位解析后检查。
Solo self-review passed: exact schema, state priority, stable identity, decrease-only consumption, transactional manual selection, configured tare inference, rollback, separate storage keys, independent inventory errors. Corrected 32-bit ID overflow by parsing with strtoull and checking bounds.

## Test / 验证证据
- 七项原生测试通过，包括实际 OTA 处理函数、真实本地 socket 的 HTTP 期限验证。
- 实际 Storage/Settings 代码测试通过：v1→v2克数迁移、无校准时保护、损坏/短读/版本检查、写失败、原始blob保留、v2克数不随校准变化。
- 实际 Measurement/InventoryApi 工作流测试通过：精确四字段与五字段列表、状态98/99/0/1、饱和/过期传感器、稳定/抖动、唯一/双候选/未知、人工选取立即同步、初始重量推算、只减不增、间隔阈值、下料失败重试、ETag304、编辑/删除回滚。
- 浏览器390/1280px通过：余量本地计算、四种状态、新建/编辑/选择、列表失败保留称重、本地普通去皮、设置全参数、无前端校验、加载/保存/上传反馈。
- 调度通过：2秒正常间隔，4/8/16/30秒退避，读写串行、写时暂停、写超时不重发。
- 页面生成一致性通过；ESP32-C3编译通过：程序1,133,885字节(86%)，全局RAM45,464字节(13%)。
Seven native suites, actual storage/settings/measurement/inventory workflow, 390/1280px browsers, scheduling and generated-resource checks passed. Target build: 1,133,885 program bytes, 45,464 global RAM bytes.

## Doc-Sync / 决策与变更
以最新用户逻辑替代旧 reading 协议；固件/页面同步交付。所有设置保留。新建时空盘=稳定总重−nominalG，负数拒绝；自动用量更新只下降，人工选取允许同步增加；旧blob不覆盖。reading不含列表版本，独立列表通过ETag和约15秒同步保持一致。普通模式的去皮转为页面本地偏移，以保证value始终为总重。
Latest user specification replaces old reading contract. Preserve all settings. Infer new tare from stable gross−nominalG, reject negative tare. Automatic updates decrease only; explicit selection may increase. Preserve legacy blobs. Use inventory ETag/periodic sync instead of reading revision. Ordinary tare is browser-local to preserve gross value semantics.

## 未覆盖和回退 / Limits and rollback
无实体操作，未进行真实上料/卸料/砝码及可控弱信号试验；模拟测试不能替代这些验证。已存在的原校准和两条记录未读取异常仍未恢复。新建净余量精度取决于配置初始净重。回退为本次源码快照/原固件；旧固件仅看旧records，不能读取新recordsV2后续改动。弱网OTA可能多次失败，必须有成功响应或明确新协议实机证据再确认。
No physical load/unload/calibration or controlled weak-signal experiment. Earlier missing calibration/two records remain unresolved. Inferred net accuracy depends on initial net setting. Rollback uses source snapshot/prior firmware; old firmware cannot read later recordsV2 changes. OTA may need retries; no success claim without response or verified new live contract.

固件 / Firmware: HX711WebScale-four-field.esp32c3.bin
SHA-256: b1066d99118aca1b4e5cc22b47516cb72f5430859517651ebaed5bc69290ce4a
文件大小 / Image bytes: 1134032

部署状态 / Deployment status: prepared; API upload pending.

## OTA 打包兼容尝试 / OTA packaging compatibility attempt
由于完整传输后设备多次返回 Flash Read Failed，尝试保留应用内容、仅添加560字节0xFF至4KiB页边界的应用镜像，大小1,134,592字节。esptool验证ESP32-C3型号、校验和和镜像哈希有效。此为兼容性尝试，尚无证据证明可修复设备Flash问题；原始固件保留。
After repeated Flash Read Failed responses, try the identical application with 560 trailing FF bytes to a 4KiB boundary (1,134,592 bytes). Esptool validates target, checksum and image hash. This is an unproven compatibility attempt; the original image is retained.

页对齐实测结果：完整传输后设备仍返回 Flash Read Failed，对齐尝试未解决问题。原始和对齐镜像均通过本地校验，API 自动重试继续；截至此记录尚未确认切换。
Page-aligned live result: a complete upload still returned Flash Read Failed. Alignment did not resolve the issue. Both images validate locally; API retries continue with deployment unconfirmed.


# 允许总重不足时添加耗材 / Allow low-total spool creation

Intake: T1。目标：总重小于初始净重时也成功新增，仅提示并持续标记问题。非目标：放宽稳定、容量、存储检查；改变四字段reading或五字段记录结构；修改校准、已有记录或用户工具。
T1. Create successfully below nominal net weight, show and persist a warning. Preserve stability/capacity/storage checks, API schemas, calibration, existing records and user tools.

Design anchor: InventoryApi.ino:createNewSpool → handleCreate → ScalePage.h。
负空盘计算结果保存为既有的未知标记NaN（API为tare:null）；新增仍确认ID并保存总重量。创建返回HTTP200与提示，页面保留成功提示及记录警告；编辑有效空盘重量后恢复余量并去除标记。不增加NVS字段或版本。
Negative inferred tare becomes existing unknown marker NaN (tare:null in API). Still persist total and confirm ID. Return HTTP200 with warning; keep visible record marker. Valid tare edit clears marker and restores remaining. No storage schema/version change.

Plan: 1 最小设备端/页面修改；2 实际API与浏览器回归；3 生成资源、目标编译与双语文档。
Plan: minimal backend/UI patch, actual API/browser regression, generated resources/build/bilingual docs.

Acceptance: 800g<1000g可新增、HTTP200、ID有效、total=800、tare=null，重启加载后标记保留；等于/大于初始重量行为正确；手动填空盘解除标记；不稳定/满容量/保存失败依旧不成功；读写串行不回退。
Acceptance: 800g<1000g creates with HTTP200, confirmed ID, total800, null tare, retained after reload. Equal/above nominal, manual correction, unstable/capacity/write-failure rejection and serialization tested.

Review-Plan: 自审通过。NaN/null为现有受支持的标记，小且可回退；源码快照已保存。当前项目tests目录已不存在，使用本地历史回归脚本在work内验证，不恢复已删除目录。
Self-review passed. Existing null marker avoids migration. Snapshot saved. Project tests directory is absent; reconstruct regression tools in work without restoring the deleted project directory.


# 允许初始重量异常时添加 / Allow creation below nominal weight

总重低于初始净重时允许新建并立即确认ID。无法推算的空盘重量保存为现有未知标记NaN，列表API为tare:null。创建成功提示原因，列表持续标记“空盘重量待确认”；填写有效空盘重量后去除警告并显示余量。等于/高于初始净重仍正常推算。稳定性、容量与存储失败约束保留。
Creation below nominal net weight succeeds and immediately confirms ID. Store unknown tare using the existing NaN marker (tare:null in list API), return a success warning and retain a visible record marker. Editing valid tare clears the marker and restores remaining mass. Equal/above nominal still infers tare normally. Stability, capacity and storage-failure constraints remain.

## Review-Code / 代码自审
通过：不存负空盘、不虚构余量；NaN标记已受v2校验和序列化支持，无格式变更；警告保持HTTP200并允许打开编辑器；纠正后清除旧提示；reading四字段和记录五字段不变。
Pass: no negative/invented tare, existing v2 sentinel and serialization, HTTP200 warnings do not block editor, corrections clear stale notices, API schemas unchanged.

## Test / 验证证据
实际Measurement/Inventory/Storage回归通过：800g对1000g可新增/确认ID、标记重载后保留、API输出null、有效空盘编辑、等于初始重量边界、不稳定/容量满/写失败、其他识别与回滚路径。
Actual measurement/inventory/storage regression passed: 800g vs1000g creation/ID, sentinel survives reload, null API, valid correction, equality boundary, unstable/capacity/write-failure guards and existing matching/rollback flows.
390/1280px真实浏览器通过：创建警告保留、记录标记、编辑后解除、余量恢复；设置页、读写串行/退避回归与生成资源检查通过。
390/1280px browsers passed warning visibility, persistent marker, correction and remaining; settings, serialized scheduling/backoff and generated-resource checks passed.
项目tests目录已被删除，本次回归工具保存在work/allow-small-spool-validation，不恢复用户删除的目录。
The project tests directory is absent; regression tools are in work/allow-small-spool-validation without restoring the deleted directory.
XIAO ESP32-C3按用户缓存固件相同配置编译通过：程序1,111,803字节(84%)，全局RAM44,868字节(13%)。
Built using the selected cache firmware's XIAO ESP32-C3 configuration: 1,111,803 program bytes (84%), 44,868 global RAM bytes (13%).

## Doc-Sync / 文档与回退
README中英文规则已同步。恢复本次源码快照/旧固件可回退，无NVS版本升级。已创建的待确认记录仍使用兼容的空盘未知标记。
Bilingual README updated. Roll back with this source snapshot/prior firmware; no NVS version upgrade. Newly flagged records retain the compatible unknown-tare marker.
实机验证范围为API读数及固件更新；没有通过API虚构上料或创建测试耗材。
Live validation covers API readings and firmware upload; no fabricated physical load or test inventory mutation.

固件 / Firmware: HX711WebScale-allow-small-spool.xiao-esp32c3.bin
文件大小 / Image bytes: 1111952
SHA-256: 33ae637dde19468315d3fece16b04aab4ea8477a20ff0367dbac6cc45f0f16aa
部署状态 / Deployment: upload pending.


# 普通称重浏览器记录 / Browser weighing log

Intake: T1，普通模式单页面功能。目标：记录日期、当前显示重量（含本地去皮）、可编辑说明；浏览器持久化；CSV导出；去掉重复普通称重/普通模式文字。保留模式切换和去皮按钮。
T1 single-page feature. Record timestamp, displayed/tared weight and editable notes, persist in browser and export CSV. Remove duplicate ordinary weighing/mode captions; retain mode switch and tare.

Design anchor: ScalePage.h ordinary view. Storage key hx711.ordinary-log.v1, newest first, ISO timestamps stored and browser-local dates displayed/exported, weight fixed to displayed 0.1g. No device API writes or extra polling. Polling never rebuilds note editors.
存储读取/写入异常时明确提示，保留当前内存记录供导出；损坏的旧存储不自动覆盖。CSV使用UTF-8 BOM、CRLF和字段引号转义；说明中的公式前缀按文本导出。
Storage failures preserve in-memory entries for export; unreadable stored data is not overwritten. CSV uses UTF-8 BOM, CRLF, quoted escaping and formula-prefix neutralization for notes.

Plan: 1 页面记录与显示精简；2 浏览器持久化/编辑/导出/异常测试和轮询回归；3 资源生成、相同XIAO配置编译、双语文档与API部署。
Implement page; validate persistence/edit/export/failure paths and polling; regenerate/build with same XIAO config, document bilingually, deploy via API.

Acceptance: 三处重复标题/状态消失但普通模式按钮保留；有效读数可记录显示值；去皮和之后读数变化不改变已记录重量；说明编辑立即保存且轮询不抢焦点；刷新恢复记录；CSV中文/引号/逗号/换行/公式文本正确；断线/98/99不可记录；本地存储失败可导出；不增加设备请求。
Remove three captions, keep mode button; record valid displayed value; retain snapshots across tare/read changes; persist edits without polling focus loss; reload; correct CSV text/escaping; block recording on disconnected/98/99; export drafts on storage failure; no extra device requests.

Review-Plan: 自审通过，源文件快照可回退；不变更设备数据结构及API。
Self-review passed, snapshot available; no device schema/API change.

用户追加：删除设置页“砝码重量”可见标签，保留输入/校准能力并给输入保留无障碍名称。T0文本子项，验收为可见标签消失、输入与校准按钮仍在。
Additional T0 text change: remove visible calibration weight label while preserving input/button and accessible name.


# 普通模式禁止设备记录 / No device measurement records in ordinary mode

Intake: T2，联动设备模式、后台耗材处理和页面；用户明确要求普通模式设备不产生任何称重/耗材记录。普通记录、日期和说明仅浏览器存储。
T2 cross-module mode enforcement. Ordinary measurements/dates/notes stay browser-local; ordinary mode must neither create nor update device spool records.

Read-only impact map: 当前普通模式仅为页面显示选项，设备processStableRaw/checkpoint/saveInventory仍可写库存；已有POST /api/mode和持久模式配置可复用。Reading JSON保持四字段。
Current browser-only mode allows background matching/checkpoints. Reuse existing mode endpoint/configuration; keep the four-field reading JSON.

Design anchor: 设备runMode是唯一权威。reading通过X-Scale-Mode响应头同步页面，不加JSON字段或额外轮询。切换模式POST /api/mode，读回确认后才启用页面操作；超时后以响应头恢复实际模式，不盲目重发。模式配置仅改变时保存，以使设备重启仍停留普通模式。
Device runMode is authoritative; X-Scale-Mode reading header syncs the page without new JSON fields/extra polls. Mode changes use existing POST; controls await acknowledgement/readback and recover uncertain outcomes via later headers. Save mode config only on actual change to retain ordinary mode after reboot.

切入普通模式前完成上一耗材的最终保存；失败则不宣称已切换。普通模式期间匹配、新建、选用、编辑、删除、周期更新和底层库存保存全部禁止。采样和实时重量保持；id始终0。重新切回耗材模式清空旧稳定状态，重新稳定识别。
Finish the previous spool before entering ordinary mode; failure leaves mode unchanged. While ordinary, disable matching, create/select/edit/delete, checkpoints and underlying inventory writes. Sampling continues, id=0. Re-entering spool mode resets stability/identity before matching.

Task tree/batches: 1 设备模式/写入边界；2 页面同步/不上传浏览器记录；3 实际API与浏览器/模式失败/轮询测试；4 双语文档、生成、同XIAO配置编译及API部署。
Batches: device guards; browser sync; actual workflow/browser failure/scheduling verification; docs/generation/build/deploy. One writer, one verified batch at a time. Dependencies 1→2→3→4.
风险：模式配置保存失败、切换请求结果未知、多页面切换、重启；由持久模式、响应头每次同步和写入底层防护处理。源码快照可回退，不变更NVS记录结构。
Risks: failed/uncertain switches, multiple tabs and reboot; mitigate with persisted mode, per-reading header sync and storage guards. Snapshot rollback, no record schema migration.

Acceptance: 普通模式持续采样/稳定/上下料/按钮/API均不写recordsV2；已有库存字节不变，id0；记录/编辑说明/CSV只有本地存储；仅mode参数的显式模式请求可发送，日期/重量/说明不提交设备。耗材模式原行为、失败回滚、首次加载模式、切换失败/恢复、4字段JSON、原轮询串行/退避通过。
Acceptance: zero inventory writes across ordinary-mode sampling/stability/load/button/API; unchanged existing bytes/id0; local logging only, mode-only explicit request; preserve spool behavior, rollback, initial/switch/uncertain recovery, four-field JSON, serialized backoff.
Review-Plan: 自审通过。采用响应头复用已有读数请求，避免新增轮询；模式切换完成后不存在耗材写入路径。
Self-review passed. Response header reuses reading request; all inventory write paths are guarded after mode transition.


# 普通模式本地记录与设备隔离 / Ordinary-mode browser log and device isolation

## 用户可见行为 / User behavior
普通模式新增记录按钮、日期/重量/可编辑说明列表及CSV导出。重量快照与当时显示值一致，包含浏览器本地去皮，精度0.1g；最新在前。说明输入即保存，轮询不重建编辑器。记录仅存于当前浏览器和访问地址的localStorage，刷新恢复。CSV使用UTF8 BOM、标准引号/换行转义，中文及常见公式前缀按文本处理。
Ordinary mode adds Record, dated weight snapshots, editable notes and CSV export. Snapshot displayed/tared 0.1g weight, newest first. Notes persist on input and survive polling without focus loss. Records stay in browser/origin localStorage and restore after reload. CSV uses UTF8 BOM, standard quote/newline escaping and common note-formula prefix protection.
已去掉普通模式读数上方“普通称重”、重量下“普通模式”、去皮上方标题，以及设置页可见“砝码重量”标签；保留模式按钮和校准输入/无障碍名称。
Removed duplicate ordinary captions and visible calibration-weight label; retained mode controls and accessible calibration input.

普通模式设备只提供实时采样，id为0；匹配、新建、选用、编辑、删除、定期保存以及底层库存写入都禁止。浏览器记录、日期、说明绝不发送设备。页面切换只提交mode，读数响应头X-Scale-Mode确认实际模式，JSON仍是raw/state/id/value，不增加轮询。模式配置仅切换时保存，重启后保留。进入普通模式前保存上一卷已有消耗；保存失败保持原模式，不误报切换成功。
In ordinary mode the device samples live weight with id0; matching, create/select/edit/delete, checkpoints and underlying inventory writes are blocked. Browser records/dates/notes never reach device. Explicit mode changes submit mode only; X-Scale-Mode confirms actual mode in existing readings without extra JSON fields/polls. Mode configuration survives reboot. Final prior-spool consumption is saved before transition; failure leaves mode unchanged.

## Review-Code / 自审
通过：本地存储不可读时不覆盖原数据；写失败保留可导出内存记录；失效读数不可记录；CSV与记录分离；模式切换确认后启用操作，未知结果由读回确认，不重发；中央saveInventory防护覆盖所有库存写路径。
Pass: preserve unreadable stored data/exportable drafts, block invalid captures, isolate CSV/logging, acknowledge/read back mode changes without blind resend, central inventory guard covers write paths.

## Validation / 验证
390/1280px真实浏览器：正常记录、去皮快照、负重量、编辑/焦点、跨页面重载、本地存储容量失败/损坏/空数据、CSV解析/文件名/中文/引号/换行/BOM、断线与98/99、初始普通模式、外部模式头同步全部通过。
Actual browsers passed logging/tare/negative values, editing/focus, reopen persistence, quota/corruption/empty storage, parsed CSV content/name/Unicode/quotes/newlines/BOM, disconnected/98/99 states, restored ordinary mode and external header synchronization.
实际Measurement/Inventory/Storage测试验证普通模式采样、上/下料、按钮函数、写接口和周期函数对recordsV2写入次数为0，已有库存字节不变，id0；模式失败回滚及耗材模式原行为通过。页面模式失败/丢回复不误切换、不自动重发。设置校准与手机/桌面样式、2s轮询/退避/读写串行及生成资源一致性通过。
Actual device workflow tests prove zero recordsV2 writes in ordinary mode across sampling/load/button/API/checkpoint paths with unchanged inventory bytes/id0. Mode failure rollback and existing spool behavior passed. Browsers verify failed/lost switch readback without resend. Settings/calibration/styles, serialized 2s polling/backoff and generated-resource checks passed.
XIAO ESP32-C3编译：程序1,119,975字节(85%)，全局RAM44,868字节(13%)。
XIAO ESP32-C3 build: 1,119,975 program bytes (85%), 44,868 global RAM bytes (13%).

## Doc-Sync and rollback / 文档与回退
README中英文已同步，流程和快照记录于work/ordinary-log-plan.md及work/ordinary-device-mode-plan.md。回退源码快照与前一固件；浏览器记录独立保存，不清空。跨浏览器/访问地址的记录独立；实体操作仍需现场确认。
Bilingual README, plans and snapshots updated. Roll back source/firmware snapshot without clearing independent browser storage. Browser/origin records remain separate; physical interaction still needs on-device confirmation.

Firmware: HX711WebScale-browser-log.xiao-esp32c3.bin
Image bytes: 1120128
SHA256: d3cd5d1f3c518b9ff6131baf1c8009ae36795dd2cdd84886baf305e8c96c6c12
Deployment / 部署: success / 已成功。

## Deployment complete / 部署完成
API首轮上传HTTP200“更新成功，设备即将重启”，1,120,128字节，17.408秒，自动重试已停止。重启后reading为精确四字段，X-Scale-Mode=scale，id=0，库存1条。验证仅访问API。
First API upload succeeded with explicit HTTP200 acknowledgement (1,120,128 bytes,17.408s); retries stopped. After reboot the exact four-field reading and X-Scale-Mode=scale were verified, id=0, inventory count=1. API-only verification.


# 新浏览器空连接与模式切换 / New-browser idle connections and mode switching

Intake: T2。目标：删除指定本地存储提示语，修复新浏览器空TCP连接阻塞正常API及首次读取失败后模式按钮锁住。保持普通模式零库存写、浏览器本地记录、四字段reading。
T2. Remove requested caption; prevent empty TCP connections from blocking APIs and allow explicit mode retry after initial read failure. Preserve ordinary-mode inventory isolation, local logs and reading schema.

Read-only map/reproduction: API baseline3.003s；预先建立无数据TCP连接后reading3.828s。当前适配器接受空连接并独占等待4s；浏览器模式按钮在modeReady=false时禁用。Arduino-ESP32 3.3.11 NetworkServer.hasClient()非阻塞检查并缓存下一个连接，accept()接回它。
Reproduced baseline3.003s, reading3.828s behind an empty TCP connection. Adapter reserves one empty client for4s; modeReady=false disables mode choices. Core3.3.11 hasClient() nonblockingly caches the next accepted socket for accept().

Design anchor: BoundedWebServer::handleClient and ScalePage mode controls. No data yet + pending client => release empty client immediately; partial HTTP requests retain absolute4s budget. Quietly retire unused speculative connections; keep logs for actual partial-request deadline failures. Do not shorten legitimate partial-request limits. Mode choices are disabled only while busy; explicit click may recover unknown initial mode. Browser/device actual state still confirmed by reading header; record/mutation guards remain locked until confirmed.
空连接遇到排队连接立即让位；已有请求数据仍保留4s绝对期限。正常空闲/预连接回收静默处理，真实部分请求超时仍记录。模式未知时允许用户显式选择，只有收到读回确认才启用记录/库存操作。

Batches: 1 提示语删除/模式手动恢复；2 空连接公平性/真实socket测试；3 双语文档/生成/相同XIAO编译/API部署与实机复测。
Batches: caption/mode recovery; idle fairness/socket tests; docs/generation/XIAO build/API-only deploy/retest. Snapshots provide rollback points.
Risk: speculative versus delayed legitimate sockets, queued incomplete clients, noisy expected idle cleanup, uncertain mode response. Pending-client-only eviction and unchanged absolute partial deadline limit impact. No installed library changes.
风险：延迟首包与预连接区分、排队的不完整请求；仅有新连接排队时释放零数据连接，保留部分请求期限，库文件不改动。

Acceptance: 空连接前置时完整API请求无需等待4s；部分/trickle/body超时仍释放；无排队连接的慢首包仍可在原期限内服务。冷启动或读取失败后模式可点击，丢回复后读回确认、不自动重发；普通记录不提交设备。指定文案消失。真实本地socket/浏览器/目标编译及实机API对比。
Review-Plan: 自审通过。使用已核实的核心公开hasClient方法；改动范围限定适配层与控件恢复，保留既有业务约束。
Self-review passed; verified public core method, limited adapter/control changes, preserve business constraints.


# 新浏览器连接与模式恢复 / New-browser connection and mode recovery

无HTTP数据的预连接此前会占用唯一处理槽最多4秒，导致后续API排队。现在遇到新连接排队时立即让位；无竞争的延迟首包仍允许原期限。真实部分请求保持绝对4秒期限及日志，普通预连接回收静默。首次模式未知时允许用户显式选择，忙碌时防重复；失败/未知结果保留手动重试能力，实际模式仍由响应头读回确认，记录/库存操作在确认前受保护。指定“记录保存在当前浏览器，可导出 CSV 备份。”可见文本已删除。
Empty speculative sockets previously held the single slot for up to4s. They now yield when another client queues. Delayed first packets without competition retain the grace period. Real partial requests keep the absolute4s deadline/log; unused preconnections retire quietly. Unknown initial mode permits explicit choice; busy operations block duplicates. Failed/uncertain mode changes remain retryable and are confirmed by response headers before enabling logs/inventory. Requested visible caption removed.

## Review-Code / 自审
通过：公开NetworkServer.hasClient在Core3.3.11检查并缓存下一连接；只释放从未收到HTTP字节的连接，不中断已有请求；原读写预算保留；设备普通模式库存隔离和浏览器本地记录逻辑不变。源码快照可回退。
Pass: verified core public API, evict only sockets with zero request bytes, preserve partial-request/send budgets and ordinary-mode protections. Snapshot rollback available.

## Test / 验证
实机修复前：API基线3.003秒；前置空TCP连接时3.828秒。仅请求API，无网页GET。
Before live reproduction: baseline3.003s, reading behind one idle TCP3.828s; APIs only.
真实本地socket新增回归在旧代码失败，修改后通过：一个/多个预连接立即让位；无竞争首包延迟仍成功；部分请求/慢滴流/不完整请求体保留期限；发送期限和OTA预检不回退。
Native real-socket regression failed on old code and passed after fix: one/multiple preconnections yield, delayed first packet works, partial/trickle/body deadlines retained, send/OTA checks preserved.
390/1280px浏览器：冷启动首次读取失败、第一次模式请求也失败后仍可显式重试，模式丢回复读回且不自动重发；正常切换/库存操作、本地记录/说明/CSV、数据损坏/容量异常恢复通过。2秒轮询、4/8/16/30退避、读写串行通过。生成资源一致性通过。
Browsers passed cold-read plus first-mode failure recovery, uncertain mode readback without resend, normal switching/inventory, local logging/notes/CSV and storage failure cases. Polling/backoff/serialization and generated resources passed.
XIAO ESP32-C3编译通过：程序1,120,079字节(85%)，RAM44,868字节(13%)。
XIAO ESP32-C3 build passed: 1,120,079 program bytes (85%),44,868 global RAM bytes(13%).

## Doc-Sync / 文档
中英文README已同步。保持同步HTTP模型与部分请求4秒期限；本次修复不保证任意弱信号请求不再失败。
Bilingual README updated. Synchronous HTTP/4s partial-request limit remain; arbitrary weak-signal failure is not ruled out.

Firmware: HX711WebScale-http-idle-mode.xiao-esp32c3.bin
Image bytes: 1120224
SHA256: f4b42fc1d1f3c017e4890fd9c9a015c9354fefc0673c4e8cde471eddf2c41383
Deployment / 部署: pending / 待确认。


# 分段POST收包修复 / Segmented POST receive repair

Intake T2: 模式切换全局变量逻辑保留；修复收包保护造成的4秒等待。实机稳定复现同一mode请求：合并发送0.01秒200，头与参数间隔150ms分发则4.02秒reset。先前空连接修复独立有效（API0.017–0.029秒），但未解决此POST问题。
Keep global mode logic. Reproduced identical mode POST: combined transmission0.01s/200, split header/body150ms apart resets at4.02s. Prior idle fix works independently but misses this case.

Design anchor BoundedWebServer/HttpRequestGate：不再依赖MSG_PEEK展示完整TCP流；非阻塞recv按实际到达累计到已有4KiB缓冲。普通请求完整后通过NetworkClient派生的预读视图交回原WebServer解析/路由，不重写参数或HTTP业务。普通视图只暴露当前完整请求，multipart上传则先提供预读前缀再读原连接，保持流式与超时继承。保留4秒绝对收包、2秒发送、容量限制与空连接让位。
Accumulate nonblocking recv into existing4KiB buffer rather than require MSG_PEEK to expose a whole stream. Replay through a NetworkClient view into original WebServer parser/router; no custom argument parser. Limit ordinary input to complete request; stream multipart after replayed prefix and inherit owner timeouts. Preserve receive/send/cap/idle guards.

Batches: 1 gate返回请求长度与stream标记、预读client；2 适配接收/原解析分发、真实socket与页面回归；3 相同XIAO编译、API部署、合并/分段POST与模式读回、无Flash写的无效后缀multipart流式验证。
Batches: framing/view; adapter/parser and socket/UI tests; build/deploy and live split/combined/mode readback plus rejected-filename streaming upload without flash mutation.

Risks: virtualStream读取、前缀与原连接交接、上传回调修改timeout、响应清理和中间件/SSE。复用原解析/路由，视图同步原client timeout，作用域结束后关闭连接；源码快照回退。JSON和模式行为不变。
Risks: virtual reads, replay/socket handoff, callback timeouts, response cleanup/middleware/SSE. Reuse core flow, synchronize timeout and scope replay before cleanup; snapshot rollback. No JSON/mode semantics changes.

Acceptance: 多次接收正确拼接headers/body；真实分段POST快速200；绝对超时仍生效；multipart越过4KiB前缀继续流式；正常GET、参数、模式控制、普通零库存写不回退；读取和上传仍只有API。
Review-Plan: 自审通过，核心3.3.11公开virtual读取与protected解析入口已核实。部署已在本会话授权。
Self-review passed; verified core3.3.11 virtual input/protected parser hooks. Deployment already authorized.

## User steering / 用户调整
用户明确要求删除自定义前置收包检查，接受单连接顺序服务，旧连接释放后新浏览器刷新即可；各浏览器功能一致。此前预读视图方案仅本地编译，未部署，现放弃并删除。
User requests removal of preflight, accepts sequential single-client service and refresh after release. Prefetched-view prototype compiled locally only and is now superseded before deployment.
最终方案：直接使用标准WebServer分段解析，适配层仅为空连接让位、响应发送超时；无4KiB缓冲、无自定义HTTP头/长度/包完整性预检。接收采用核心默认空闲等待（约5秒），不再承诺4秒绝对接收期限。保留模式全局变量、普通模式零库存写和本地记录。
Final: native WebServer parsing with only idle-client handoff and bounded response writes. Remove4KiB buffer and custom framing/size/preflight parsing. Core idle waits (~5s), no custom4s absolute receive guarantee. Preserve global mode, ordinary inventory isolation and local logs.
验收追加：合并及分开发送mode参数均快速成功；同一模式请求来自多个独立TCP连接行为一致；旧空连接释放后新连接正常；实际模式双向切换后恢复原值且库存不变；multipart无效文件名流式请求完整返回400而不写Flash。
Acceptance: combined/split mode requests succeed, independent clients behave consistently, idle handoff works, mode roundtrip restores original/inventory, rejected-filename streaming multipart returns400 without flash writes.


# 原生单连接HTTP / Native single-client HTTP

用户最终要求：去掉自定义前置收包，单连接顺序服务即可，不同浏览器功能一致。
User requests native single-client sequential handling without custom preflight; functionality must remain browser-independent.

已删除HttpRequestGate、自定义分段检查与4KiB缓冲，未部署的预读视图原型也已移除。由ESP32原生WebServer解析HTTP头和POST数据，适配层只保留空连接让位及2秒响应发送预算。接收使用核心默认空闲等待（约5秒），不再使用或宣称4秒绝对接收上限。
Removed gate, custom packet checks and4KiB buffer, including the undeployed replay prototype. Native WebServer parses HTTP/POST. Adapter retains idle handoff and2s response-send budget. Core receive idle waits (~5s), no custom4s absolute limit.

模式仍由全局runMode分支执行；普通模式不匹配或写入耗材记录，日期/重量快照/说明只在浏览器。新浏览器读取设备实际模式，无浏览器绑定或专属连接锁。
Global runMode controls behavior. Ordinary mode never matches/writes inventory; dated snapshots/notes stay browser-local. New browsers read actual mode; no browser ownership/session lock.

Review-Code: 自审通过。复用原生解析路径，未修改安装库；无自定义HTTP完整性预检；响应发送有界，旧空连接可释放；接口JSON与业务模式保持。
Self-review passed: native parser, no installed library edits/preflight, bounded responses/idle handoff, unchanged JSON/business semantics.
Test: 原生socket适配层（单连接让位、延迟首包、发送期限）通过；390/1280px本地记录、CSV、冷启动失败后的模式手动恢复通过；2秒轮询/退避与读写串行通过。HTTP分段解析由核心负责，以实机API做最终验证。
Native socket adapter, 390/1280px local logs/CSV/cold-start mode recovery and serialized polling/backoff passed. Actual split parsing is delegated to core and verified on device after upload.
XIAO ESP32-C3编译通过：程序1,117,997字节(85%)，RAM40,756字节(12%)，较前版减少4,112字节RAM。
Target build:1,117,997 program bytes (85%),40,756 global RAM bytes(12%),4,112 RAM bytes saved.
中英文README已同步；源码快照可回退，不迁移或清空库存/浏览器记录。普通小POST不再由自定义层等包；原生空闲超时不等于整个慢流请求的硬总期限。
Bilingual README updated; snapshot rollback, no record migration/clearing. Native idle waits are not a hard whole-request deadline.

Firmware: HX711WebScale-native-http.xiao-esp32c3.bin
Image bytes: 1118144
SHA256: f39586fa2f0c3198d6c6e4761cf7bc073e0052831e319db08e30b2accdaa5da3
Deployment: success / 已成功。

## Deployment and live verification / 部署与实机验证
首轮API上传HTTP200“更新成功”，1,118,144字节，17.299秒；已停止重试。普通POST整包0.012秒200；头和参数间隔150ms时0.1728秒200，间隔500ms时0.5137秒200；头也分段时0.1968秒200。多个独立新TCP连接和空连接让位后请求均成功。
First API upload succeeded (HTTP200,1,118,144 bytes,17.299s); retries stopped. Combined POST0.012s; split header/body150ms apart0.1728s;500ms apart0.5137s; split headers0.1968s, all200. Independent clients/idle handoff passed.
实机切换到普通模式、读回确认成功，再恢复测试前耗材模式；已有库存完全一致。8KiB multipart无效.txt文件正确返回400“请选择.bin固件文件”，验证原生流式解析，未开始写Flash。reading仍为精确四字段。全程仅API，不请求设备网页。
Actual mode roundtrip/readback passed and original spool mode was restored; inventory unchanged.8KiB multipart with rejected.txt filename returned400 before flash update, validating native streaming. Reading retains exactly four fields. API-only; no device webpage request.


## T1 传感器异常保留显示 / Preserve display on sensor errors
Intake: 98响应不覆盖同模式最近有效重量，明确告警；不改变设备采样、HTTP、API或库存写入。
Goal: retain last valid same-mode display on98 with warning, without sampling/HTTP/API/persistence changes.
Plan/Design: ScalePage.h单独缓存有效样本；操作继续使用当前状态；模式变更和99失效缓存。先快照，再页面与边界测试，生成资源，XIAO编译，API部署。
Separate display-only sample cache; actions use current state. Invalidate on mode change/99. Snapshot, page/edge regression, regenerate, XIAO build, API deploy.
Review-Plan: 自审通过，小范围页面修改，部署已有授权。Self-review passed; scoped page change, deployment already authorized.
Review-Code: 自审通过，缓存不修改实时state；余量以缓存总重与记录空盘重计算；缓存身份不开放选用/新增；恢复清除实时警告；不恢复已删除测试目录。
Self-review passed: cached sample never replaces current state; remaining uses cached gross/tare; stale ID does not enable creation/selection; recovery clears warning immediately; deleted project tests remain deleted.
Test: 390/1280px真实Chrome测试覆盖耗材余量/总重保持、模式切换缓存隔离、99后无样本、普通去皮值保持、操作禁用、恢复、本地记录/CSV；读写串行、2秒轮询与4/8/16/30秒退避回归。
Real Chrome390/1280px covers retained gross/remaining/tared value, mode cache isolation, no valid sample after99, guarded actions, recovery and local logs/CSV. Scheduling regression verifies serialized reads/writes and polling/backoff.
风险/回退: 显示历史值须留意警告，不能替代当前值；快照及前版固件可回退，无存储迁移。实机传感器故障无法远程强制注入，以浏览器可控输入验证。
Risk/rollback: retained values are historical and visibly warned; snapshot/previous firmware provide rollback, no storage migration. Sensor faults cannot be forced remotely; browser fixtures verify fault behavior.

Deployment / 部署：192.168.3.105，仅POST /api/update及重启后GET /api/reading；第6次上传HTTP200“更新成功，设备即将重启”，1,119,168字节，19.342秒。停止重试，重启后API已恢复：raw672699/state1/id0/value436.7。未请求网页或修改库存/校准。
API-only deployment succeeded on attempt6 (HTTP200),1,119,168 bytes,19.342s; retries stopped. Post-reboot reading recovered: raw672699/state1/id0/value436.7. No webpage requests or inventory/calibration mutations.
SHA256: 8e719499f2c32def1c9333f123d998dcb30a6b3cd0239d788c1de6d7f35202b7


## T2 总重显示与确认身份 / Total display and confirmed identity
Intake/Plan: 用户明确确认“已确认id在波动及传感器异常时保留，异常只由state表达”；首次匹配仍要求稳定。耗材记录在余量左侧显示总重。计划见会话work/confirmed-id-plan.md，包括模块边界、风险及分批回退。
User explicitly chose retaining confirmed ID through fluctuations AND sensor faults, using state for faults. Initial matching still requires stability. Total precedes remaining in rows. Session plan documents boundaries/risks/batches.
Design: confirmedId保留模式/校准/上料/卸料保存约束，去掉瞬时稳定和传感器新鲜度；周期保存显式检查平台有效与稳定。acceptRaw供主循环与测试共用，采样超时只清稳定窗口，饱和值不进入上下料检测，均不清确认身份。processStableRaw仍唯一处理稳定使用量且只减少。
Identity keeps mode/calibration/load/unload-save guards without live freshness/stability. Checkpoints explicitly require fresh stable platform. Shared acceptRaw preserves identity across sample gaps and saturation, clears stability, and excludes saturated samples from unload detection. Stable processing remains decrease-only.
Review-Code: 自审通过：首次未确认仍0，异常ID不开放新增/选用，定期保存独立稳定/新鲜度约束；主循环恢复不会清身份；传感器失效不会凭满量程假卸料；普通模式不写库存。无存储迁移、API字段不变。
Self-review passed: initial ID stays0; retained fault ID does not permit create/select; checkpoints independently require freshness/stability; recovery/saturation cannot erase identity; ordinary mode remains inventory-free. No schema/storage migration.
Test: 真实C++工作流覆盖530g首次稳定确认，525–534g交替50次保留ID且不扣重，超时/正负饱和仍保留ID且无周期写入，恢复稳定后下降至528g、卸料/未校准/普通模式归零。390/1280px Chrome覆盖活动/历史总重顺序与数值、空盘未知提示、失效缓存、普通本地记录/CSV与请求串行退避。首次全套原生检查与资源生成并行时因旧资源哈希失败，生成后重跑验证；测试目录保持用户删除状态，测试仅在会话work中。
Actual C++ workflow validates initial530g stable confirmation,50 alternating525–534g samples without ID loss/consumption, timeout/both saturation bounds retaining ID without checkpoints, stable recovery to528g and unload/uncalibrated/ordinary ID reset. Chrome390/1280px verifies active/saved totals/order/unknown-tare warning and stale/local-log/CSV/scheduling regressions. Initial native asset-hash check raced regeneration; rerun after generation. User-deleted project tests remain deleted; session tests stay in work.
Doc-Sync: 中英文README同步新契约；历史计划条目保留，以上为当前规则。前版stale-reading固件和本批源码快照可回退。
Bilingual README updated; historical plan entries retained, this section supersedes identity rules. Previous stale-reading firmware/source snapshots provide rollback.
Residual risks: 物理卸料仅按连续有效空台样本确认；故障期间无法知道物理上是否已换卷，保留既有身份符合本次用户规则。未远程注入真实故障，使用实际代码受控采样验证。
Physical unload requires consecutive valid empty samples; a spool swap during an outage cannot be inferred, so identity retention follows the user's chosen rule. Real faults were not remotely injected; controlled samples exercise actual source.

Deployment / 部署：API第3次上传HTTP200“更新成功，设备即将重启”，1,119,360字节，17.515秒；停止重试，重启后reading与spools均HTTP200。全程未请求网页、未修改库存/校准/模式。
API upload succeeded on attempt3 (HTTP200),1,119,360 bytes,17.515s. Retries stopped; post-reboot reading and spools returned200. No device webpage requests or inventory/calibration/mode mutations.
实机读回：耗材模式，value520.1、id0；已有记录#1保存total497.5、tare57.0。重启不保留内存确认身份，当前未自动匹配；需首次稳定匹配或人工选用后才进入“已确认后保留ID”的路径。未用人工选用测试污染记录。
Live readback: spool mode,value520.1,id0; existing record1 total497.5/tare57.0. Reboot clears RAM confirmation; current load remains unmatched. Initial stable match or explicit manual selection is required before confirmed-ID retention applies. No manual-selection mutation was performed for testing.


T0 文案调整 / Notice wording: “选用”未满足条件的提示改为“请先稳定耗材,持续3s.”；仅更改提示，稳定检测窗口/阈值/计时保持不变。
Selection precondition notice now reads “请先稳定耗材,持续3s.”. Wording only; stability window, thresholds and timer unchanged.

Deployment / 部署：调整传输速率后，128KiB/s第32次API上传HTTP200“更新成功”，1,119,360字节，12.582秒；已停止重试。重启后reading恢复：raw707564,state1,id0,value527.8。全程仅API，无网页请求；3s为提示文案，未修改稳定判断计时。
At128KiB/s attempt32 succeeded via API (HTTP200),1,119,360 bytes,12.582s. Retries stopped; post-reboot reading recovered. API-only, no webpage requests. The3s text is guidance; stability timing is unchanged.


## T1 页面请求节奏 / Page cadence
Intake/Design: spools成功或失败后60秒再定期读取，保留初始/耗材写后强制同步，去掉ID变化触发；reading请求结束后2秒，覆盖以前4/8/16/30秒退避。连续3次reading失败才离线，前2次保留显示但禁用记录/去皮/新增/选用，成功重置计数和离线提示。
Inventory background interval/retry60s, preserving initial/post-inventory-write sync and removing ID-change triggers. Reading completion+2s supersedes prior backoff. Offline only after3 consecutive failures; first2 retain display with fresh-reading actions disabled; success resets counters/notice.
Review-Plan/Code: 自审通过，ScalePage.h单模块；列表失败不触发reading失败计数；写操作仍暂停轮询/串行，传感器98仍按原显示缓存规则；无设备业务/存储变更。详细计划保存在会话work/polling-minute-plan.md。
Self-review passed: single page module, separate list/reading failures, serialized writes/poll pause, preserved sensor98 cache, no business/storage changes. Session plan records acceptance/rollback.
Test: 受控时钟验证每轮reading完成+2s、冷启动第3次失败才离线/成功恢复、spools成功和失败都0/60/120秒且ID变化无额外请求、写操作暂停及超时释放。390/1280px Chrome验证原值保留/成功打断连续失败/第三次清空/即时恢复，以及模式重试、本地记录/CSV与失效缓存。首轮计时测试因模拟DOM未建立online节点报错，修正测试读取后通过。生成资源检查通过。
Controlled clock proves completion+2s reads, cold3-failure threshold/recovery,0/60/120s inventory success/failure schedule despite changing IDs, write pause/timeouts. Chrome390/1280px proves retained display/consecutive-failure reset/third-failure clearing/immediate recovery and mode/local logs/CSV/stale cache. Initial clock assertion accessed an uncreated mock DOM node; corrected test access then passed. Generated assets check passed.
Doc-Sync/Risks: 中英文README同步。普通模式不定期拉取耗材列表；库存读取随reading成功循环调度，因此60秒是最小间隔，慢请求可延后；前两次失败显示的是旧重量。可回退上一版select-notice固件或页面快照重生成，不迁移数据。
Bilingual README synced. Ordinary mode does not poll inventory.60s is a minimum interval gated by successful reads; slow requests can delay it. First2 failed reads display historical values. Rollback with prior select-notice firmware or page snapshot/regeneration; no migration.

Deployment / 部署：第36次API上传HTTP200“更新成功”，1,119,264字节，44.619秒；已停止重试。重启后reading恢复：raw707227,state1,id0,value527.0。仅POST更新API及成功后GET reading，无网页请求、无业务配置/库存/校准更改。
Attempt36 API upload succeeded (HTTP200),1,119,264 bytes,44.619s; retries stopped. Post-reboot reading recovered. Only update POST and post-success reading GET; no webpage requests or business configuration/inventory/calibration mutations.


T0 / reading间隔改为请求结束后3秒，其余规则不变。验收：计时检查完成+3秒、列表60秒、连续3次失败和串行不回退；编译后API部署。
T0: reading interval is now completion+3s; other rules unchanged. Acceptance: timed cadence/60s inventory/3-failure threshold/serialization preserved, then build/API deployment.

部署/Deployment: 首次API上传成功，HTTP200，17.664秒，1,119,264字节；重启后reading恢复value515.3/state1/id0。停止重试，未请求网页。
First API attempt succeeded (HTTP200),17.664s,1,119,264 bytes; post-reboot API recovered. Retries stopped, no webpage requests.


# T1 / 缓存读数与传感器故障 / Cached reading and sensor fault
目标：reading仅用缓存；HTTP阻塞不让缓存失效；有raw且已校准必须计算value。state98仅表示尚无读数或实际传感器等待持续失败。确认ID跨异常保留；网页请求结束后3秒保持。
Goal: cached-only API, HTTP blocking cannot invalidate cached raw; calculate value whenever a raw sample and calibration exist. State98 indicates no sample or confirmed sensor-read failure. Preserve confirmed ID and completion+3s page cadence.
Anchor: Measurement.ino acquisition status, InventoryApi.ino response, HX711WebScale.ino service call. No threading, preflight, schema, calibration/storage migration or page redesign.
设计：用实际读取结果维护状态，不用缓存年龄。每轮传感器服务最多等待2ms，仅累计这些实际等待时间，累计3000ms无数据标记故障。成功读取立即清计数/故障。HTTP时间完全不进入等待累计。校准同步读取超时同样标记故障。98时已有缓存仍返回计算value；尚无缓存或未校准返回null。数字满量程不再自动作为98。长采样间隔只重建稳定窗口，不清身份或缓存。
Design: acquisition-result state, not cache age. Sensor service waits at most2ms per loop, accumulating only real sensor wait (3000ms marks failure). Successful reads reset failure/wait. HTTP time excluded. Synchronous calibration timeout also marks failure. Cached calibrated value remains numeric even in98; no cache or missing calibration stays null. Numeric ADC endpoints alone no longer imply98. Long gaps restart stability only, retaining ID/cache.
任务：快照/计划 -> 实际C++故障/缓存边界测试 -> 修改 -> 自审/回归 -> 文档双语 -> XIAO构建 -> API上传/只读验证。
Tasks: snapshot/plan -> actual C++ cache/failure tests -> implementation -> self-review/regression -> bilingual docs -> XIAO build -> API upload/read-only verification.
验收：HTTP模拟延迟10s后raw/value及state保持；实际连续等待阈值才98；98保留id和缓存计算值、不扣重/不周期保存；重新读到raw恢复；首次无数据98/null、未校准99/null；零/负值/两端数字可计算；接口不触发ADC读取；页面3s和60s列表/三次失败规则通过。
Acceptance:10s HTTP gap preserves state/raw/value; only accumulated actual wait triggers fault;98 retains ID/cache and suppresses consumption/checkpoints; read recovery; initial98/null and uncalibrated99/null; zero/negative/endpoints compute; API makes no ADC calls; page cadence preserved.
Review-Plan: 自审通过。风险：故障判定以累计实际等待计时，网络繁忙会推迟发现真实断线；98缓存value为历史数据，页面仍告警并显示最近正常值。快照/上一polling-3s固件可回退。
Self-review passed. Risk: real disconnect detection may be delayed by network work;98 cached value is historical, page retains warning/last healthy display. Rollback via snapshots/prior polling-3s firmware.

Review-Code: 自审通过：reading不访问ADC、不依赖采样年龄；已校准缓存即使98也有value；实际读取失败暂停周期保存且保留ID；等待累计饱和至3000避免溢出；读取成功清故障，长间隔仅清稳定窗口。同步校准读失败不改校准参数。
Self-review passed: cached-only API independent of age; calibrated cache computes even in98; real failures suppress checkpoints and preserve ID; wait saturates at3000; read success resets fault and long gaps clear stability only. Failed calibration acquisition leaves calibration intact.
Test: 实际C++代码验证10秒HTTP模拟等待不触发98、2998/3000ms实际等待边界、98缓存重量/身份保持、失败禁止周期写入、读取恢复、同步读取超时、无样本/未校准null、零/负/满量程可计算；计时测试确认页面完成后3s、列表60s与三次失败规则。
Actual C++ verifies10s simulated HTTP wait,2998/3000ms actual-wait boundary,98 cached value/ID, checkpoint suppression/recovery, synchronous timeout, no-data/uncalibrated null, zero/negative/ADC endpoints. Timer tests preserve completion+3s reads,60s lists and3-failure notice.
Doc-Sync: 中英文README已同步新状态语义；历史条目由本次规则取代。物理断线以实际代码模拟验证，不在实机人为断开。
Bilingual README reflects current semantics, superseding historical entries. Actual-code fault simulation used; device not physically disconnected.
