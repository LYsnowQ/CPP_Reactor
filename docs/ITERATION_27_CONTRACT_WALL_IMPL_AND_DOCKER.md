# Iteration 27 - 代码注释全面落地与 Docker 部署

## 本轮目标
- 给所有模块的头文件和源文件补上完整的注释
- 补充 Docker 一键部署能力

## 代码注释落实

这次给全部 8 个模块的代码都加了三类注释：

| 层级 | 位置 | 写什么 |
|------|------|--------|
| 公开契约 | `.hpp` 里 public 方法上 | 这个函数干什么、参数是什么、返回什么、线程安全吗 |
| 算法说明 | `.cpp` 里函数/段落开头 | 这段代码整体是怎么工作的，设计思路 |
| 实现注释 | `.cpp` 里关键行前面 | 为什么这么写，边界条件，竞态处理 |

### 改了哪些文件

| 模块 | 改了多少文件 | 主要涉及 |
|------|-------------|----------|
| Core | 8 个 | EventLoop、3 种 Dispatcher、Buffer、CoreStatus |
| Handler | 5 个 | IHttpHandler、StaticFileHandler、SqlHandler |
| Net | 8 个 | Channel、IOThreadPool、TcpConnection、TcpServer |
| Observability | 2 个 | Metrics |
| Protocol | 6 个 | HttpRequest、HttpResponse、HttpRouter |
| Repository | 3 个 | SqlRepository、SqlTask |
| Utils | 3 个 | StringUtils、JsonConfigLoader |
| Persistence | 11 个 | MySqlConnection、SqlExecutor、ThreadLocalSqlConn、SqlConfig、SqlTransaction |
| **合计** | **46 个** | 所有 hpp 和 cpp 全覆盖 |

### 编译验证

```
make clean && make -j$(nproc) → 零错误零警告
```

## Docker 部署

新增了 4 个文件让项目可以用 Docker 一键跑起来：

| 文件 | 干嘛的 |
|------|--------|
| [Dockerfile](file:///project/CPPReactor/Dockerfile) | 多阶段构建，编译环境和运行环境分开 |
| [docker-compose.yml](file:///project/CPPReactor/docker-compose.yml) | 编排 MySQL 8.0 和 app 两个服务 |
| [docker-entrypoint.sh](file:///project/CPPReactor/docker-entrypoint.sh) | 启动前根据环境变量动态生成 SQLConfig.json |
| [.dockerignore](file:///project/CPPReactor/.dockerignore) | 排除不需要进镜像的文件 |

### 怎么用

```bash
docker compose up --build -d
curl http://localhost:8080/healthz
```

MySQL 连接用环境变量配：`MYSQL_HOST`、`MYSQL_PORT`、`MYSQL_USER`、`MYSQL_PASSWORD`、`MYSQL_DATABASE`。

## 相比上一版本解决了什么

- **代码注释**：46 个源文件都加了注释，后面的人看代码能更快理解
- **Docker 部署**：不用手动装依赖，一条命令就能跑
- **编译验证**：从头编译零错误零警告，启动正常
