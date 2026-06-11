#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <type_traits>
#include <vector>

namespace reactor::base
{

/// @brief 非连续内存高性能网络缓冲区
///
/// 设计要点：
///   - 前置 8 字节预留（kCheapPrepend），为二进制协议头部预留空间
///   - readIndex_ / writeIndex_ 双指针管理读写位置
///   - readFd 使用 readv + 栈上 extraBuf 避免频繁扩容
///   - 扩容优先整理碎片（数据前挪），减少重新分配
///
/// @thread 非线程安全，单线程环境下使用
class Buffer
{
  public:
    static const size_t kCheapPrepend = 8; // 为2进制协议预留请求头
    static const size_t kInitialSize = 1024;

    explicit Buffer();
    ~Buffer() = default;

    /// @brief 当前可读字节数
    size_t readableBytes() const;

    /// @brief 当前可写字节数
    size_t writeableBytes() const;

    /// @brief 前置预留空间大小
    size_t prependableBytes() const;

    /// @brief 指向可读区起始位置
    const char *peek() const;

    /// @brief 在可读区中查找子串
    /// @return 相对于可读区起始的偏移量，未找到返回 npos
    std::string::size_type find(const std::string_view substr) const;

    /// @brief 在可读区中从 offset 开始查找子串
    /// @return 相对于可读区起始的偏移量，未找到返回 npos
    std::string::size_type find(const std::string_view substr, size_t offset) const;

    /// @brief 追加数据到缓冲区
    void append(const std::string &str);

    /// @brief 追加原始数据到缓冲区
    void append(const char *data, size_t len);

    /// @brief 消费 len 字节（仅移动 readIndex_，不释放内存）
    void retrieve(size_t len);

    /// @brief 重置读写位置到初始状态（kCheapPrepend）
    void retrieveAll();

    /// @brief 获取可读区只读视图（不消费）
    std::string_view getStringView(size_t len) const;

    /// @brief 获取可读区从 offset 开始 len 字节的只读视图（不消费）
    std::string_view getStringView(size_t offset, size_t len) const;

    /// @brief 取出指定长度字符串并消费
    std::string retrieveAsString(size_t len);

    /// @brief 取出所有可读字符串并消费
    std::string retrieveAllString();

    /// @brief 从 fd 读取数据到缓冲区（readv 双缓冲区）
    ///
    /// 策略：writeable 区 + 32KB 栈上 extraBuf 作为 iovec，
    /// 减少数据拷贝和 buffer 扩容次数。
    ///
    /// @param fd         源 fd
    /// @param saved_errno 输出参数，出错时保存 errno
    /// @return 读取的字节数，-1 表示出错
    ssize_t readFd(int fd, int *saved_errno);

    /// @brief 将可读区数据写入 fd
    /// @param fd 目标 fd
    /// @return 写入的字节数
    ssize_t writeFd(int fd);

  private:
    char *begin_();
    const char *begin_() const;
    char *beginWrite_();

    void ensureWriteableBytes_(size_t len);
    void makeSpace_(size_t len);

  private:
    std::vector<char> buffer_;
    uint32_t readIndex_;
    uint32_t writeIndex_;
};

} // namespace reactor::base
