# Soft-UE ns-3 项目代码格式化规范

本文档定义了Soft-UE ns-3项目的代码格式化规范，确保代码库的一致性和可读性。

## 概述

本规范基于ns-3项目的标准编码风格，并结合了现代C++最佳实践。所有贡献者必须遵循此规范。

## 工具配置

### clang-format配置

项目使用`.clang-format`文件进行代码格式化，配置基于Google C++风格，但做了以下调整：

- **缩进**: 2个空格
- **列限制**: 100字符
- **指针对齐**: 左对齐 (`Type* ptr`)
- **大括号风格**: K&R风格 (Attach)
- **空行**: 最大保留1个空行

## 基本规范

### 1. 缩进和空格

```cpp
// 正确的缩进 - 使用2个空格
if (condition)
{
  DoSomething();
  if (anotherCondition)
  {
    DoMore();
  }
}

// 错误的缩进 - 使用4个空格或Tab
if (condition)
{
    DoSomething(); // 错误
}
```

### 2. 大括号风格

```cpp
// K&R风格 - 左大括号在同一行
if (condition)
{
  // code here
}

// 错误的Allman风格
if (condition)
{
  // code here
}
```

### 3. 函数声明和定义

```cpp
// 函数返回类型和函数名分行
void
ClassName::MethodName (void)
{
  // implementation
}

// 单行函数可以放一行
bool IsEmpty (void) const { return m_empty; }
```

### 4. 类定义

```cpp
class MyClass : public BaseClass
{
public:
  // Public members first
  MyClass ();
  virtual ~MyClass ();

  void DoSomething (void);

private:
  // Private members last
  int m_member;

  // Helper methods
  void HelperMethod (void);
};
```

### 5. 命名约定

#### 类名
```cpp
// 使用PascalCase
class SesManager;
class PdsStatistics;
class SoftUeNetDevice;
```

#### 成员变量
```cpp
// 使用m_前缀 + camelCase
int m_memberVariable;
std::string m_name;
Ptr<SomeType> m_pointer;
```

#### 常量
```cpp
// 全局常量使用k前缀
const int kMaxSize = 100;

// 静态常量
static const uint32_t DEFAULT_PORT = 8080;
```

#### 函数名
```cpp
// 公共方法使用PascalCase
void ProcessPacket (void);
Ptr<Manager> GetManager (void) const;

// 私有方法可以使用camelCase
void privateMethod (void);
```

### 6. 注释格式

```cpp
/**
 * @brief 类的简短描述
 * @details 详细描述（可选）
 */
class MyClass
{
public:
  /**
   * @brief 方法的简短描述
   * @param param1 参数1的描述
   * @param param2 参数2的描述
   * @return 返回值描述
   */
  int SomeMethod (int param1, std::string param2);

private:
  std::string m_member; ///< 成员变量的描述
};
```

### 7. 包含头文件

```cpp
// 顺序：系统头文件 -> ns-3头文件 -> 项目头文件
#include <algorithm>
#include <iostream>

#include "ns3/object.h"
#include "ns3/log.h"

#include "my-class.h"
#include "helper/util.h"
```

### 8. 条件语句

```cpp
// if语句格式
if (condition)
{
  // code
}
else if (anotherCondition)
{
  // code
}
else
{
  // code
}

// switch语句格式
switch (value)
{
case CONSTANT1:
  // code
  break;
case CONSTANT2:
  // code
  break;
default:
  // code
  break;
}
```

### 9. 循环语句

```cpp
// for循环
for (uint32_t i = 0; i < count; ++i)
{
  // code
}

// while循环
while (condition)
{
  // code
}

// do-while循环
do
{
  // code
} while (condition);
```

### 10. 模板和泛型

```cpp
// 模板参数使用描述性名称
template <typename T>
class Container
{
  // ...
};

// 使用模板时保持一行格式
std::vector<Ptr<SesManager>> managers;
std::map<std::string, uint32_t> nameToId;
```

## ns-3特定规范

### 1. ns-3类型系统

```cpp
// 使用ns-3类型
Time delay = Seconds (1.0);
DataRate rate = DataRate ("1Gbps");

// ns-3智能指针
Ptr<MyClass> obj = CreateObject<MyClass> ();
```

### 2. 日志记录

```cpp
// 在文件开始定义日志组件
NS_LOG_COMPONENT_DEFINE ("MyClass");

// 在方法中使用
NS_LOG_FUNCTION (this);
NS_LOG_INFO ("Information message");
NS_LOG_DEBUG ("Debug message");
NS_LOG_ERROR ("Error message");
NS_LOG_WARN ("Warning message");
```

### 3. 对象系统

```cpp
// 标准的TypeId实现
TypeId
MyClass::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyClass")
    .SetParent<Object> ()
    .SetGroupName ("Soft-Ue")
    .AddConstructor<MyClass> ()
    .AddAttribute ("AttributeName",
                   "Attribute description",
                   TypeValue::GetDefault (),
                   MakeTypeAccessor (&MyClass::GetAttribute, &MyClass::SetAttribute),
                   MakeTypeChecker ());
  return tid;
}
```

### 4. 回调和追踪

```cpp
// 回调定义
Callback<void, int, std::string> m_callback;

// 追踪源
TracedCallback<Ptr<Packet>, Time> m_transmitTrace;
```

## 文件组织

### 1. 头文件结构

```cpp
// 版权信息 (C/C++风格)
/*******************************************************************************
 * Copyright notice...
 ******************************************************************************/

// 文件文档 (Doxygen风格)
/**
 * @file             filename.h
 * @brief            Brief description
 * @author           author@example.com
 * @version          1.0.0
 * @date             2025-XX-XX
 * @copyright        License information
 */

// 包含保护
#ifndef FILENAME_H
#define FILENAME_H

// 包含文件 (系统 -> ns-3 -> 项目)
#include <system_headers>

#include "ns3/headers.h"

#include "project_headers.h"

// 前向声明
namespace ns3 {
class ForwardDeclaredClass;
}

// 类定义和实现
// ...

#endif /* FILENAME_H */
```

### 2. 源文件结构

```cpp
#include "filename.h"
#include "other_headers.h"
#include <system_headers>

// 命名空间开始
namespace ns3 {

// 日志组件定义
NS_LOG_COMPONENT_DEFINE ("ClassName");
NS_OBJECT_ENSURE_REGISTERED (ClassName);

// 静态成员定义 (如果需要)
// ...

// 类型ID实现
TypeId
ClassName::GetTypeId (void)
{
  // implementation
}

// 其他方法实现
// ...

// 命名空间结束
} // namespace ns3
```

## 常见错误和修正

### 1. 缩进不一致

```cpp
// 错误
if (condition)
{
  DoSomething();
    DoMore(); // 错误的缩进
}

// 正确
if (condition)
{
  DoSomething();
  DoMore();
}
```

### 2. 大括号位置

```cpp
// 错误
if (condition)
{
  DoSomething();
}

// 正确
if (condition)
{
  DoSomething();
}
```

### 3. 行长度

```cpp
// 错误 - 行太长
VeryLongMethodName (veryLongParameter1, veryLongParameter2, veryLongParameter3, veryLongParameter4);

// 正确 - 适当换行
VeryLongMethodName (veryLongParameter1,
                    veryLongParameter2,
                    veryLongParameter3,
                    veryLongParameter4);
```

## 工具使用

### 1. 使用clang-format

```bash
# 格式化单个文件
clang-format -i src/soft-ue/model/ses/ses-manager.cc

# 格式化整个目录
find src/soft-ue -name "*.cc" -o -name "*.h" | xargs clang-format -i
```

### 2. IDE配置

推荐的IDE插件：
- VS Code: C/C++ Extension Pack
- CLion: 内置clang-format支持
- Vim: vim-clang-format

### 3. Git钩子

建议设置pre-commit钩子自动格式化代码：

```bash
#!/bin/sh
# .git/hooks/pre-commit
find src/soft-ue -name "*.cc" -o -name "*.h" | xargs clang-format -i
git add src/soft-ue/
```

## 检查清单

提交代码前，请确认：

- [ ] 所有文件使用2空格缩进
- [ ] 大括号使用K&R风格
- [ ] 行长度不超过100字符
- [ ] 变量命名遵循项目约定
- [ ] 包含适当的Doxygen注释
- [ ] 头文件包含保护正确
- [ ] ns-3特定模式正确使用
- [ ] 运行`clang-format -i`确认格式

## 维护

本规范文档应随项目发展持续更新。如有疑问或建议，请联系项目维护者。

---

**版本**: 1.0
**最后更新**: 2025-12-08
**维护者**: Soft-UE Project Team