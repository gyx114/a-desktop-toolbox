# a-desktop-toolbox

基于 VS MFC 实现的 Windows 桌面工具箱程序，集成进程管理、启动项管理、剪贴板历史、窗口处理、文件管理、Git 工具箱等功能。

## 标签页功能

| 标签页 | 功能描述 |
|--------|----------|
| 进程管理 | 枚举系统进程，支持结束进程、定位程序文件 |
| 启动项管理 | 查看/添加/删除注册表启动项 |
| 剪贴板 | 记录最近 10 条剪贴板文本，双击复制 |
| 窗口处理 | 窗口定位、置顶、透明度调节、强制结束进程、截图 |
| 文件管理 | 拖入文件生成副本、重命名、删除、复制 |
| Git 工具箱 | 常用 Git 命令列表，双击复制命令，右键编辑 |

## 控件功能表

### 全局控件

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 标签页 | IDC_TAB1 | Tab Control | 切换 6 个功能标签页 |
| 关机/重启 | IDC_BUTTON1 | Button | 执行关机/重启（根据 Combo 选择） |
| 取消关机 | IDC_BUTTON2 | Button | 取消已计划的关机 |
| 关机选项 | IDC_COMBO1 | Combo Box | 选择关机模式（重启/关机/定时） |
| 关机时间 | IDC_EDIT1 | Edit Control | 输入定时关机秒数 |
| 开机自启 | IDC_CHECK1 | Check Box | 设置程序开机自启动 |
| 最小化到托盘 | IDC_CHECK2 | Check Box | 关闭时最小化到系统托盘 |
| 防锁屏 | IDC_CHECK5 | Check Box | 阻止系统自动锁屏/休眠 |
| 非管理员 PowerShell | IDC_CHECK6 | Check Box | 以非管理员权限启动 PowerShell |

### 进程管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 进程列表 | IDC_LIST1 | List Control | 显示进程名/PID/路径/内存，右键菜单支持结束进程和定位文件 |

### 启动项管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 启动项列表 | IDC_LIST2 | List Control | 显示启动项名和命令，右键菜单支持添加/删除 |

### 剪贴板标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 剪贴板历史 | IDC_LIST3 | List Control | 显示最近 10 条剪贴板文本，双击复制到剪贴板 |

### 窗口处理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 窗口信息 | IDC_LIST5 | List Control | 显示选中窗口的句柄/进程名/PID/路径/标题 |
| 置顶窗口列表 | IDC_LIST6 | List Control | 显示所有已置顶窗口，右键取消置顶 |
| 定位窗口 | IDC_BUTTON19 | Button | 进入定位模式，点击目标窗口后可选择置顶或关闭 |
| 透明度标签 | IDC_STATIC18 | Static Text | 显示当前透明度百分比 |
| 透明度滑块 | IDC_SLIDER2 | Slider Control | 调节选中窗口透明度（10~255） |
| 强制结束进程 | IDC_BUTTON15 | Button | 强制终止选中窗口的进程 |
| 截图到剪贴板 | IDC_BUTTON16 | Button | 截取选中窗口画面并复制到剪贴板 |

### 文件管理标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 拖放路径 | IDC_STATIC_PATH | Static Text | 显示拖入的文件路径 |
| 路径显示 | IDC_EDIT4 | Edit Control | 显示/编辑拖入文件路径 |
| 生成副本 | IDC_BUTTON3 | Button | 生成拖入文件副本 |
| 重命名原名 | IDC_EDIT7 | Edit Control | 输入原文件名 |
| 重命名目标 | IDC_EDIT8 | Edit Control | 输入目标文件名 |
| 重命名 | IDC_BUTTON23 | Button | 执行文件重命名 |
| 删除文件 | IDC_BUTTON24 | Button | 删除选中文件 |
| 打开文件夹 | IDC_BUTTON25 | Button | 在资源管理器中打开文件夹 |
| 复制文件 | IDC_BUTTON26 | Button | 复制文件到指定目录 |
| 目标路径 | IDC_MFCEDITBROWSE2 | MFC Edit Browse | 选择复制目标路径 |

### Git 工具箱标签页

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 命令列表 | IDC_LIST4 | List Control | 显示 Git 命令说明和命令，双击复制，右键编辑 |
| 打开 GitHub | IDC_BUTTON30 | Button | 在浏览器中打开当前目录的 GitHub 仓库 |
| 启动 Git Bash | IDC_BUTTON31 | Button | 在当前目录打开 Git Bash |
| 清除路径 | IDC_BUTTON32 | Button | 清除拖入的 Git 仓库路径 |

### 工具箱按钮

| 控件 | ID | 类型 | 功能 |
|------|-----|------|------|
| 启动微信 | IDC_BUTTON4 | Button | 启动微信 |
| 启动 QQ | IDC_BUTTON5 | Button | 启动 QQ |
| 启动 VS Code | IDC_BUTTON6 | Button | 启动 VS Code |
| 启动 Visual Studio | IDC_BUTTON7 | Button | 启动 Visual Studio |
| 启动哔哩哔哩 | IDC_BUTTON8 | Button | 启动哔哩哔哩客户端 |
| 学习文件夹 | IDC_BUTTON9 | Button | 打开学习文件夹 |
| 本地服务器 | IDC_BUTTON10 | Button | 打开本地服务器 URL |
| 中国大学 MOOC | IDC_BUTTON11 | Button | 打开中国大学 MOOC |
| 音量应用 | IDC_BUTTON12 | Button | 应用编辑框中的音量值 |
| 音量设为 0 | IDC_BUTTON13 | Button | 设置系统音量为 0 |
| 音量设为 60 | IDC_BUTTON14 | Button | 设置系统音量为 60 |
| 音量滑块 | IDC_SLIDER1 | Slider Control | 调节系统音量（0~100） |
| 音量显示 | IDC_EDIT5 | Edit Control | 显示当前音量值 |
| 运行命令 | IDC_BUTTON17 | Button | 运行输入的命令 |
| 清空命令 | IDC_BUTTON18 | Button | 清空命令输入框 |
| 命令输入 | IDC_EDIT6 | Edit Control | 输入命令 |
| 任务管理器 | IDC_BUTTON20 | Button | 打开 Windows 任务管理器 |
| 下载文件夹 | IDC_BUTTON21 | Button | 打开下载文件夹 |
| 启动元宝 | IDC_BUTTON22 | Button | 启动元宝 |
| 启动 PowerShell | IDC_BUTTON27 | Button | 启动 PowerShell |
| 启动 WSL | IDC_BUTTON28 | Button | 启动 WSL |
| 打开 LeetCode | IDC_BUTTON29 | Button | 打开 LeetCode |
| 哔哩哔哩下一首 | IDC_BUTTON33 | Button | 哔哩哔哩播放下一首 |

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| Ctrl+Alt+Space | 显示/隐藏工具箱主窗口 |
| F3 | 连点器开关 |
| 右键 → 置顶 | 在定位模式下右键目标窗口置顶 |
| 右键 → 关闭 | 在定位模式下右键目标窗口关闭 |

## 菜单栏

| 菜单 | 菜单项 | 功能 |
|------|--------|------|
| 文件 | 设置 | 打开设置对话框（配置应用路径） |
| 文件 | 退出 | 退出程序 |
| 视图 | 进程管理/启动项管理/剪贴板/窗口处理/文件管理/Git 工具箱 | 快速切换到对应标签页 |
| 工具 | 微信/QQ/VS Code/VS/哔哩哔哩/学习/下载/PowerShell/WSL/Git Bash | 快速启动对应工具 |
| 窗口 | 窗口定位/取消全部置顶/关闭选中窗口 | 窗口管理快捷操作 |
| 帮助 | 关于/快捷键列表/GitHub | 帮助信息 |