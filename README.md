# Embedded Watchdog Framework

一个嵌入式看门狗框架，提供任务心跳监控和超时处理功能。该框架设计简洁，易于集成到嵌入式系统中。

## 特性

- **多通道支持**: 支持最多16个独立的看门狗通道
- **灵活超时**: 每个通道可设置不同的超时时间
- **回调机制**: 超时时可执行自定义回调函数
- **线程安全**: 使用互斥锁保护共享数据
- **电源管理**: 支持暂停和恢复功能
- **平台抽象**: 易于移植到不同平台

## 文件结构

```
watchdog/
├── z_wdt.h             # 公共API头文件 (37行，精简设计)
├── z_wdt.c             # 核心实现 (平台无关，可直接用于嵌入式)
├── watchdog_os.c       # 平台层实现 (需根据目标平台修改)
├── watchdog_test.c     # 测试程序
├── Makefile            # 构建文件
└── README.md           # 说明文档
```

### 架构特点

- **z_wdt.h**: 仅 37 行，极度精简，只包含必要的公共 API
- **z_wdt.c**: 核心逻辑与平台无关，可直接移植到嵌入式环境
- **watchdog_os.c**: 平台相关实现，根据目标平台修改（Linux/FreeRTOS/裸机等）

## 快速开始

### 编译

```bash
# 编译所有目标
make all

# 仅编译库
make libwatchdog.a

# 编译测试程序
make watchdog_test

# 编译示例程序
make example
```

### 运行测试

```bash
# 运行测试
make test

# 使用valgrind检查内存泄漏
make test-valgrind

# 运行示例
make run-example
```

## API 参考

### 初始化

```c
int z_wdt_init(void);
```

初始化看门狗系统。必须在其他函数调用前调用。

### 添加通道

```c
int z_wdt_add(uint32_t reload_period, watchdog_callback_t callback, void *user_data);
```

- `reload_period`: 超时时间（毫秒）
- `callback`: 超时回调函数
- `user_data`: 用户数据指针
- 返回值: 通道ID（成功）或-1（失败）

### 删除通道

```c
int z_wdt_delete(int channel_id);
```

删除指定的看门狗通道。

### 喂狗

```c
int z_wdt_feed(int channel_id);
```

重置指定通道的超时时间。

### 暂停/恢复

```c
void z_wdt_suspend(void);
void z_wdt_resume(void);
```

暂停或恢复看门狗系统（用于电源管理）。

### 处理函数

```c
void z_wdt_process(void);
```

处理看门狗逻辑（由内部定时器线程调用）。

## 使用示例

### 基本使用

```c
#include "z_wdt.h"

// 超时回调函数
void timeout_callback(int channel_id, void *user_data) {
    printf("Channel %d timeout!\n", channel_id);
    // 处理超时情况
}

int main() {
    // 初始化看门狗
    z_wdt_init();
    
    // 添加通道
    int channel = z_wdt_add(2000, timeout_callback, NULL);
    
    // 在主循环中喂狗
    while (running) {
        do_work();
        z_wdt_feed(channel);
        sleep(1);
    }
    
    // 清理
    z_wdt_delete(channel);
    z_wdt_cleanup();
    return 0;
}
```

### 多任务使用

```c
#include "z_wdt.h"
#include <pthread.h>

// 任务数据
typedef struct {
    int task_id;
    int channel_id;
    bool should_feed;
} task_data_t;

// 超时回调
void task_timeout_callback(int channel_id, void *user_data) {
    task_data_t *data = (task_data_t *)user_data;
    printf("Task %d (channel %d) timeout!\n", data->task_id, channel_id);
}

// 任务函数
void* task_function(void *arg) {
    task_data_t *data = (task_data_t *)arg;
    
    while (running) {
        if (data->should_feed) {
            z_wdt_feed(data->channel_id);
        }
        
        // 执行任务工作
        do_task_work();
        usleep(100000); // 100ms
    }
    
    return NULL;
}

int main() {
    z_wdt_init();
    
    // 创建任务数据
    task_data_t task1 = {1, 0, true};
    task_data_t task2 = {2, 0, true};
    
    // 添加看门狗通道
    task1.channel_id = z_wdt_add(2000, task_timeout_callback, &task1);
    task2.channel_id = z_wdt_add(3000, task_timeout_callback, &task2);
    
    // 创建任务线程
    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, task_function, &task1);
    pthread_create(&thread2, NULL, task_function, &task2);
    
    // 主循环
    while (running) {
        sleep(1);
    }
    
    // 清理
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    z_wdt_delete(task1.channel_id);
    z_wdt_delete(task2.channel_id);
    z_wdt_cleanup();
    
    return 0;
}
```

## 配置选项

### 最大通道数

在 `z_wdt.h` 中定义：

```c
#define WATCHDOG_MAX_CHANNELS 16
```

### 平台抽象

框架使用平台抽象层，需要实现以下函数：

```c
// 获取当前时间戳（毫秒）
int64_t watchdog_get_ticks(void);

// 启动定时器
void watchdog_timer_start(int64_t timeout_ticks);

// 停止定时器
void watchdog_timer_stop(void);

// 日志输出
void watchdog_log(const char *level, const char *format, ...);
```

## 测试

### 测试覆盖

测试程序包含以下测试用例：

1. **基本功能测试**: 初始化、添加通道、喂狗、删除通道
2. **超时功能测试**: 验证超时检测和回调执行
3. **多通道测试**: 同时管理多个通道
4. **暂停/恢复测试**: 验证电源管理功能
5. **错误条件测试**: 验证错误处理
6. **最大通道测试**: 验证通道数量限制

### 运行测试

```bash
# 运行所有测试
make test

# 使用valgrind检查内存泄漏
make test-valgrind
```

## 移植指南

框架采用分层设计，核心代码（`z_wdt.h` 和 `z_wdt.c`）与平台无关，可以直接用于嵌入式环境。

### 快速移植步骤

1. **保留核心文件**: 将 `z_wdt.h` 和 `z_wdt.c` 拷贝到目标项目，无需修改
2. **实现平台层**: 参考 `watchdog_os.c`，为目标平台实现以下函数：
   - `watchdog_get_ticks()` - 获取毫秒级时间戳
   - `watchdog_log()` - 日志输出
   - `watchdog_os_init()` - OS初始化
   - `watchdog_os_cleanup()` - OS清理
   - `watchdog_mutex_lock/unlock()` - 互斥锁操作
3. **定期调用**: 在定时器/任务中每 100ms 调用一次 `z_wdt_process()`

### 支持的平台

框架已在以下平台验证可用：
- Linux (pthread)
- Windows (Win32 API)
- 理论支持：FreeRTOS, Zephyr, RT-Thread, 裸机等

详细的移植指南、不同平台实现示例和注意事项，请参考 **[PORTING.md](PORTING.md)**。

## 注意事项

1. **线程安全**: 所有API都是线程安全的
2. **内存管理**: 框架不分配动态内存
3. **时间精度**: 依赖平台时间API的精度
4. **回调执行**: 超时回调在定时器线程中执行
5. **资源清理**: 程序退出前应调用 `z_wdt_cleanup()`

## 许可证

本项目采用 MIT 许可证。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。
