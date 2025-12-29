The OpenWSN firmware is documented using [http://www.stack.nl/~dimitri/doxygen/](doxygen) syntax. This folder contains the configuration for Doxygen to build HTML-based documentation.

Note that this documentation is built nightly and published at http://openwsn-berkeley.github.io/firmware/ by the OpenWSN build servers.


## Additional experiment ideas

### Channel reuse with interference（中文说明）

当你和同学在相同的信道上同时做实验时，可以通过以下方式模拟“恶劣环境”：

1. **保持相同的 `CHANNEL`**：所有小组都使用相同的信道编号，例如 11。这样会产生同频干扰。
2. **区分报文内容**：虽然信道相同，但每个发送端可以在序列号之后追加不同的字节（例如 0xAA、0xBB 等），以便在日志中判断报文来源。
3. **同步实验步骤**：约定同一时间开始发送，同一时间开始记录指定距离的日志，这样便于比较各组数据。
4. **观察现象**：
   - `log_serial_packets.py` 记录的 `len seq crc rssi` 中，`crc` 可能从 1 变成 0，意味着碰撞导致 CRC 错误；
   - `plot_pdr_rssi.py` 生成的 PDR 曲线会下降，RSSI 的波动也会增大；
   - 在严重干扰下，你还可能看到序列号出现“跳跃”更频繁（表示丢包）。
5. **记录说明**：在实验报告中注明：实验人数、是否同步起始、各自使用的 payload 模式，以及干扰环境（例如是否有人在移动、是否有金属遮挡）。

通过比较无干扰与有干扰时的 PDR/RSSI 曲线差异，即可直观说明同频干扰的影响。建议在数据分析脚本之外，额外记录每次实验的开始/结束时间以及现场环境备注，便于后续复盘。

## Troubleshooting

- 如果串口脚本报错“pyserial is required”，请确认已经安装依赖。
- 如果日志中出现无法解析的行，可以加 `--show` 参数查看并调试。
- Matplotlib 报错通常是因为运行环境缺少 GUI 支持；使用 `--save` 写入 PNG 就能在无显示的环境下完成分析。
