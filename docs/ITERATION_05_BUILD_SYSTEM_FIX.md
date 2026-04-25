# Iteration 05 - 全量构建链路修复

## 本轮目标
- 解决全量构建阻塞问题，恢复“从源码到可执行”链路。
- 解释 `spdlog/common.h` 报错根因并修正构建配置。

## 问题根因与修复

### 1) `spdlog/common.h` 报缺失的真实原因
- 根因：
  - `spdlog` 目录实际完整，但 `Makefile` 对 `.cpp` 的编译规则配置错误。
  - 原规则仅定义了 `%.o: %.c`，导致 `.cpp` 编译没有稳定套用 `-I.` 头文件路径。
  - 同时对象列表替换规则写成 `.c -> .o`，对 `.cpp` 源文件不成立。
- 修复：
  - 对象规则改为 `.cpp -> .o`。
  - 显式使用 `CPPFLAGS` 传递 `-I. -I./include`。
- 代码位置：
  - `Makefile`：`OBJ` 规则、`%.o: %.cpp` 规则。

### 2) `std::latch` 编译失败
- 根因：
  - 工程未显式开启 C++20。
- 修复：
  - `CXXFLAGS` 增加 `-std=c++20`。
- 代码位置：
  - `Makefile`：`CXXFLAGS`。

### 3) 线程相关链接参数缺失风险
- 修复：
  - 链接参数加入 `-pthread`，保证线程库符号稳定链接。
- 代码位置：
  - `Makefile`：`LDLIBS`。

### 4) `HttpResponse` 头部容器写入类型错误
- 根因：
  - `headers_` 类型为 `vector<pair<string,string>>`，但 `emplace_back` 传入了单字符串。
- 修复：
  - 改为 `emplace_back(key, value)`。
- 代码位置：
  - `HttpResponse.cpp`：`addHeader()`。

### 5) Dispatcher 编译单元缺少 `EventLoop` 完整定义
- 根因：
  - `Poll/Epoll/Select` 的 `.cpp` 文件调用了 `evLoop_->active(...)`，但仅有前向声明。
- 修复：
  - 在三个 `.cpp` 中补充 `#include "EventLoop.hpp"`。
- 代码位置：
  - `PollDispatcher.cpp`、`EpollDispatcher.cpp`、`SelectDispatcher.cpp`。

### 6) Buffer 视图接口 `const` 声明位置错误
- 根因：
  - `getStringView` 使用尾置返回类型时 `const` 放置位置不当，导致方法未被识别为常量成员函数。
- 修复：
  - 统一改为标准声明：`std::string_view getStringView(...) const`。
- 代码位置：
  - `Buffer.hpp`、`Buffer.cpp`。

## 本轮收益
- 构建链路参数语义清晰：预处理、编译、链接职责分离。
- `spdlog` 头文件可按标准包含路径解析，不再误报“缺失”。
- 为后续迭代提供可重复执行的全量构建基础。

## 相比上一版本解决的问题
- 相比 Iteration 04，本轮解决了“逻辑可改但工程无法全量构建验证”的阻塞。
- 将多个分散编译问题一次性收敛到构建系统层，恢复了每轮迭代可回归的工程节奏。
