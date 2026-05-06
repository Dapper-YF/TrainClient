# TASK_CURRENT.md — Phase 8：检视页面"视频回放"功能

## Context
- **项目根目录**：`E:\Study\AI_Workspace\TrainClient`
- **客户端路径**：`E:\Study\AI_Workspace\TrainClient\Client`
- **关键文件**：
  - `Client\windows\InspectionWindow.cpp/h` — 检视页面（目标位置）
  - `Client\Client.pro` — 已有 `multimedia multimediawidgets`
  - `Client\pages\HomePage.cpp/h` — 首页（已读过）
  - `E:\Study\AI_Workspace\TrainClient\MVSvideos\` — 视频文件目录
- **编译命令**：
  ```bash
  cd E:\Study\AI_Workspace\TrainClient\Client
  D:\Qt\5.15.2\mingw81_64\bin\qmake.exe
  D:\Qt\Tools\mingw810_64\bin\mingw32-make.exe -j4
  ```

## Goal
在 `InspectionWindow`（检视页面）的播放控制栏添加"▶ 视频"按钮，点击弹出小型视频回放窗口，播放列车三侧（顶/左/右）过车视频，三路同步播放。

## 视频源

### 本地目录（与 MVSimages 同级）
```
E:\Study\AI_Workspace\TrainClient\MVSvideos\
├── left-test.mp4     ← 已有测试视频（2.7MB）
├── right-test.mp4
└── top-test.mp4
```
目标目录结构（与 MVSimages 完全对应）：
```
E:\Study\AI_Workspace\TrainClient\MVSvideos\
└── <date>\
    ├── left.mp4
    ├── right.mp4
    └── top.mp4
```

### 后端视频接口
- 服务器基础：`http://localhost:8080`
- HTTP 路由：`/trainvideos/<arg>` → `<m_strAreaCameraVideoFilePath>/<arg>`
- 数据库字段（TrainInformation 表）：
  - `strLeftAreaCameraPath` — e.g., `2025-11-13/left.mp4`
  - `strRightAreaCameraPath`
  - `strTopAreaCameraPath`
- 最终 URL 格式：`http://localhost:8080/trainvideos/<date>/left.mp4`

### 客户端 URL 构造
```cpp
// reachDatetime 格式：QDateTime::fromSecsSinceEpoch(reachDatetime).toString("yyyy-MM-dd")
// strLeftAreaCameraPath = "2025-11-13/left.mp4"
QString leftUrl = "http://localhost:8080/trainvideos/" + dateStr + "/left.mp4";
// → http://localhost:8080/trainvideos/2025-11-13/left.mp4
```

### ⚠️ 后端需手动确认
1. `csystemconfig.cpp` 的 `m_strAreaCameraVideoFilePath` 默认值需指向 `E:/Study/AI_Workspace/TrainClient/MVSvideos`（或等效绝对路径）
2. 数据库 TrainInformation 表中 `strLeftAreaCameraPath` 等字段需有正确值

## Constraints
- 弹出窗口必须小于检视页面（约 900×340 px）
- 三路视频横向排列（顶/左/右 三等分）
- 三路同步播放（同时开始）
- 使用 Qt5 Multimedia（已有 `multimedia multimediawidgets` 模块）
- 视频暂不接通，UI 和路径指向正确即可

---

## Steps

### Step 1：在 Client.pro 中添加视频路径宏

在 `DEFINES` 行添加（参考已有的 `MVSIMAGES_PATH`）：
```
DEFINES += QT_DEPRECATED_WARNINGS MVSIMAGES_PATH=..\\mvsimages MVSVIDEOS_PATH=..\\MVSvideos
```

### Step 2：创建 VideoPlaybackDialog 类

**新建文件**：`Client/windows/VideoPlaybackDialog.h` + `.cpp`

设计要点：
- 继承 `QDialog`，非模态（`Qt::Window`）
- 尺寸：900×340 px（固定）
- 标题栏文字："▶ 过车视频"
- 样式：跟随 InspectionWindow 深色主题（背景 #1a1a2e）

**布局**（水平三等分）：
```
┌──────────────────────────────────────────────────────────────┐
│  ▶ 过车视频                                            [×]  │
├──────────────────────────────────────────────────────────────┤
│   顶部视图              左侧视图              右侧视图        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │QVideoWidget │  │QVideoWidget │  │QVideoWidget │      │
│  │  300×220   │  │  300×220   │  │  300×220   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│   ▶ 顶部 300×220      ▶ 左侧 300×220      ▶ 右侧 300×220    │
├──────────────────────────────────────────────────────────────┤
│  [◀上一帧] [▶播放/■暂停] [下一帧▶]           当前车厢 0/12   │
└──────────────────────────────────────────────────────────────┘
```

**三路同步播放实现**：
```cpp
void VideoPlaybackDialog::playAll()
{
    if (m_leftPlayer && m_topPlayer && m_rightPlayer) {
        m_leftPlayer->play();
        m_topPlayer->play();
        m_rightPlayer->play();
    }
}
```

**使用 QMediaPlayer + QVideoWidget**（已启用 multimedia 模块）：
```cpp
// 构造函数中
m_leftPlayer = new QMediaPlayer(this);
m_leftVideoWidget = new QVideoWidget(this);
m_leftVideoWidget->setMinimumSize(280, 200);
m_leftPlayer->setVideoOutput(m_leftVideoWidget);
m_leftPlayer->setMedia(QUrl::fromLocalFile(localPath));
connect(m_leftPlayer, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state){
    if (state == QMediaPlayer::PlayingState) {
        // 同步其他两路
        if (m_topPlayer->state() != QMediaPlayer::PlayingState) m_topPlayer->play();
        if (m_rightPlayer->state() != QMediaPlayer::PlayingState) m_rightPlayer->play();
    }
});
```

**降级处理**（视频文件不存在时）：用 QLabel 显示路径文本

### Step 3：在 InspectionWindow 中添加按钮

**位置**：`InspectionWindow::setupUi()` 的播放控制栏（playBar）中，在 `m_speedCombo` 之后添加：

```cpp
// 视频回放按钮
QPushButton* videoBtn = new QPushButton("▶ 视频", playBar);
videoBtn->setFixedSize(60, 30);
videoBtn->setObjectName("videoPlaybackBtn");
videoBtn->setStyleSheet(R"(
    QPushButton {
        background-color: #1890ff; color: white; border: none;
        border-radius: 4px; font-size: 11px; font-weight: bold;
    }
    QPushButton:hover { background-color: #40a9ff; }
)");
playLayout->addWidget(videoBtn);
connect(videoBtn, &QPushButton::clicked, this, [=]() {
    // 构造视频 URL
    QString dateStr = QDateTime::fromSecsSinceEpoch(m_currentReachDatetime)
                     .toString("yyyy-MM-dd");
    QString leftUrl  = QString("http://localhost:8080/trainvideos/%1/left.mp4").arg(dateStr);
    QString topUrl   = QString("http://localhost:8080/trainvideos/%1/top.mp4").arg(dateStr);
    QString rightUrl = QString("http://localhost:8080/trainvideos/%1/right.mp4").arg(dateStr);

    static VideoPlaybackDialog* dialog = nullptr;
    if (!dialog) dialog = new VideoPlaybackDialog(this);
    dialog->loadVideos(leftUrl, topUrl, rightUrl);
    dialog->show();
    dialog->activateWindow();
});
```

**同时更新头文件**：`windows/InspectionWindow.h` 顶部添加：
```cpp
#include "VideoPlaybackDialog.h"
```

### Step 4：处理视频路径（从本地文件读取）

由于后端可能不返回视频路径，优先使用本地 MVSvideos 目录：

```cpp
QString localBase = QCoreApplication::applicationDirPath()
                  + "/../../MVSvideos/";  // 相对于 Client.exe 的路径

// 本地优先，HTTP 兜底
QString leftPath = localBase + dateStr + "/left.mp4";
if (!QFile::exists(leftPath)) {
    leftPath = leftUrl;  // 用 HTTP URL
}
```

### Step 5：编译验证

```bash
cd E:\Study\AI_Workspace\TrainClient\Client
D:\Qt\5.15.2\mingw81_64\bin\qmake.exe
D:\Qt\Tools\mingw810_64\bin\mingw32-make.exe -j4
```

预期：无编译错误

---

## 验收条件
- [ ] "▶ 视频"按钮出现在检视控制栏
- [ ] 点击按钮弹出约 900×340 的小窗口
- [ ] 窗口内三路视频横向排列（顶/左/右）
- [ ] 窗口标题显示"▶ 过车视频"
- [ ] 窗口可关闭（×按钮）
- [ ] 编译无错误
