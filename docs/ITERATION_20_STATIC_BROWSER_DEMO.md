# Iteration 20 - 本机静态目录访问示例与测试扩展

## 本轮目标
- 提供可直接在本机浏览器访问的目录浏览与文件查看示例。
- 保持 Reactor 核心主链路不变，仅通过可插拔 handler 扩展功能。
- 补充更全面的验证脚本，覆盖目录索引、文件读取、404、路径越界防护和基线压测。

## 技术点与代码位置
- 连接层响应能力增强（支持 body 与 content-type）：
  - `TcpConnection.hpp`：`HandlerResult` 增加 `body/contentType`；新增 `pendingBody_/pendingContentType_`
  - `TcpConnection.cpp`：`appendSimpleResponse_()` 支持动态 `Content-Length` 与 body 写入
- 静态目录示例 handler：
  - `main.cpp`：新增 URL 处理、HTML 转义、MIME 推断、目录 HTML 渲染、路径越界检查
  - `main.cpp`：`server.setRequestHandler(...)` 默认挂载本地静态目录浏览器
  - 行为：
    - `/` 显示目录索引并可点击跳转
    - 目录返回 HTML 列表
    - 文件返回原始内容与对应 content-type
    - `/healthz` 返回健康检查文本
    - 路径穿越请求返回 `403`
- 测试扩展：
  - 新增 `scripts/test_static_browser.sh`：
    - 校验目录索引页面
    - 校验指定文档内容
    - 校验 404
    - 校验穿越防护（编码路径）
  - 复用 `scripts/test_smoke.sh` 与 `scripts/test_baseline_once.sh` 做完整链路验证

## 结果与验证
- 编译：`make -j4` 通过。
- 冒烟：`test_smoke.sh` 通过（200）。
- 静态浏览专项：`test_static_browser.sh` 通过。
- 基线（5s/80c/4t）：`Requests/sec 11452.17`，`Transfer/sec 32.22MB`。

## 相比上一版本解决的问题
- 相比 Iteration 19，本轮从“可插拔接口”进一步落到“可直接使用的本机目录服务示例”。
- 在不侵入 Reactor 核心的前提下，实现了可见、可点击、可验证的基础框架演示能力。
