<div align="center">

# 🚂 TrainClient

### Railway Freight Wagon Loading Status Dynamic Detection System

**An industrial-grade intelligent visual inspection system for railway freight operations**

[![Qt](https://img.shields.io/badge/Qt-6.9-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![OpenCV](https://img.shields.io/badge/OpenCV-4.12-5C3EE8?style=flat-square&logo=opencv)](https://opencv.org/)
[![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)](LICENSE)

---

</div>

> **Core Focus: Desktop Client Development**
>
> This project encompasses the full-stack system — from PLC communication and camera hardware integration to web platform deployment. The primary development emphasis lies in the **Qt desktop client**, which serves as the main inspection interface for railway freight operators.

## Overview

TrainClient replaces manual freight inspection workflows with an automated, AI-assisted detection system. It captures high-resolution images of passing freight wagons from three perspectives (left, right, top), stitches them into complete views, and provides an intelligent inspection interface for operators to identify loading anomalies.

### Key Features

- **Multi-camera synchronized capture** — 3 Hikvision line-scan cameras acquire left, right, and top views simultaneously
- **Real-time PLC communication** — Binary protocol integration with trackside sensors and PLC controllers
- **High-precision image stitching** — OpenCV-powered seamless image composition at speeds up to 120 km/h
- **Intelligent detection** — AI-assisted anomaly identification (cargo residue, door status, over-limit detection)
- **Desktop & Web clients** — Full-featured Qt desktop app + browser-based remote access
- **REST API backend** — JWT-authenticated HTTP API for system integration

## System Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                    Outdoor Data Acquisition                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────────┐  │
│  │ Left Cam │  │ Top Cam  │  │ Right Cam│  │ PLC + AEI +    │  │
│  │ (Line)   │  │ (Line)   │  │ (Line)   │  │ Magnetic Steel │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └───────┬────────┘  │
│       └──────────────┼──────────────┘               │           │
│                      ▼                              ▼           │
├──────────────────────────────────────────────────────────────────┤
│                     Machine Room (Indoor)                        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                   Qt6 Application Server                   │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │  │
│  │  │ CTrainTcp   │  │ WebServer    │  │ ImageServer      │  │  │
│  │  │ Server      │  │ (REST API)   │  │ (Port 9091)      │  │  │
│  │  │ (Port 8989) │  │ (Port 8080)  │  │                  │  │  │
│  │  └──────┬──────┘  └──────┬───────┘  └────────┬─────────┘  │  │
│  │         │                │                    │            │  │
│  │  ┌──────┴──────────────┴────────────────────┴─────────┐   │  │
│  │  │              Core Processing Pipeline               │   │  │
│  │  │  Image Stitching → DB Storage → Smart Detection    │   │  │
│  │  └────────────────────────────────────────────────────┘   │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│                    Client Access Layer                            │
│  ┌─────────────────────────┐  ┌───────────────────────────────┐  │
│  │   Qt Desktop Client     │  │   Web Browser Client          │  │
│  │   (Primary Interface)   │  │   (Vue.js + Element UI)       │  │
│  └─────────────────────────┘  └───────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

## Project Structure

```
TrainClient/
├── Backend/                    # Qt6 Server Application (C++20)
│   ├── main.cpp               # Entry point
│   ├── mainwindow.cpp/h       # GUI main window
│   ├── csystemconfig.cpp/h    # Configuration management
│   ├── cdatabasemanager.cpp/h # SQLite database layer
│   ├── imageuploader.cpp/h    # Remote image uploader
│   ├── logger.cpp/h           # Logging system
│   ├── Net/                   # Network modules
│   │   ├── ctraintcpserver.*  # PLC TCP server (port 8989)
│   │   ├── cdataprotocol.*    # Binary protocol parser
│   │   ├── webserver.*        # HTTP REST API (port 8080)
│   │   ├── imageserver.*      # Image distribution (port 9091)
│   │   └── clienthandler.*    # Client connection handler
│   ├── MVS/                   # Camera modules
│   │   ├── MvCamera.*         # Hikvision MVS SDK wrapper
│   │   ├── clinecameramanager.* # Line-scan camera manager
│   │   ├── clinecameraitem.*  # Single camera capture thread
│   │   ├── cimagestitching.*  # Image stitching engine
│   │   └── cconfigdialog.*    # Camera configuration UI
│   ├── common/                # Shared definitions
│   │   ├── constants.h        # Constants (chunk size, etc.)
│   │   └── protocol.*         # Client binary protocol
│   └── Utils/                 # Utility classes
│       ├── imageutil.*        # OpenCV image utilities
│       └── stringutil.*       # String manipulation
│
├── Client/                    # Qt5 Desktop Client
│   ├── main.cpp
│   ├── mainwindow.*
│   ├── login/                 # Authentication module
│   ├── dashboard/             # Home page with statistics
│   ├── inspection/            # Train inspection (core workflow)
│   ├── history/               # Historical query & export
│   ├── annotation/            # Car type annotation for AI training
│   └── admin/                 # System administration
│
├── Web/                       # Vue.js Frontend (dist only)
│   └── dist/                  # Production build artifacts
│
├── ApiTest/                   # API testing utilities
├── .copilot_bridge/           # Development task documentation
└── README.md
```

## Tech Stack

| Layer | Technology |
|-------|-----------|
| **Backend Framework** | Qt 6.9.2 + C++20 |
| **GUI** | Qt Widgets (Desktop), Vue.js + Element UI (Web) |
| **Image Processing** | OpenCV 4.12 (MinGW build) |
| **Camera SDK** | Hikvision MVS (win64) |
| **Database** | SQLite (appdata.db) + QSql |
| **HTTP Server** | Qt HttpServer (built-in) |
| **Compiler** | MinGW |
| **Build System** | CMake (Backend), qmake (Server) |

## Communication Protocols

| Interface | Protocol | Port | Description |
|-----------|----------|------|-------------|
| PLC ↔ Server | TCP (binary, big-endian) | 8989 | Train arrival/departure, wagon data |
| AEI ↔ Server | RS232 (9600 bps) | Serial | Car identification |
| Web API | HTTP REST (JWT auth) | 8080 | System integration |
| Image Transfer | TCP (binary, 64KB chunks) | 9091 | Client image delivery |

## Database Schema

| Table | Description |
|-------|-------------|
| `TrainInformation` | Train records (number, time, direction, images) |
| `CarriageTable` | Wagon details (ID, model, images, inspection status) |
| `Users` | User accounts (auth, roles, permissions) |
| `user_tokens` | JWT tokens (24h expiry) |

## Building

### Prerequisites

- Qt 6.9.2+ (MinGW kit)
- OpenCV 4.12 (MinGW build)
- Hikvision MVS SDK
- CMake 3.29+

### Backend

```bash
cd Backend
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Client

Open `Client/Client.pro` in Qt Creator and build with Qt 5.x kit.

## Configuration

Edit `config.ini` in the application directory:

```ini
[Server]
ListenPort=9090
WebServerPort=8080
PLCAddress=192.168.0.1

[Camera]
LineCameraSamplingHz=20000
ExposureTime=0.05
ImageHeight=2048

[Storage]
TrainImagesSavePath=c:\trainImage
LineCameraGrabbingFilePath=c:\grabbing
```

## Usage

1. Launch `Backend` application on the server machine
2. Open Qt Desktop Client or navigate to `http://server:8080`
3. Login with credentials (default: `admin` / `admin`)
4. View train inspection dashboard, historical records, or manage system settings

## Performance Metrics

| Metric | Target |
|--------|--------|
| Car identification accuracy | > 99.9% |
| Single wagon judgment accuracy | > 98% |
| Image stitching success rate | > 98% |
| Supported train speed | 0–120 km/h |
| Detection accuracy | > 80% |
| Efficiency improvement | 70% over manual inspection |

---

<div align="center">

---

</div>

<div align="center">

# 🚂 TrainClient

### 铁路货车装载状态动态检测系统

**工业级铁路货运智能视觉检测系统**

---

</div>

> **核心开发重心：桌面客户端**
>
> 本项目涵盖完整的技术栈 — 从 PLC 通信和相机硬件集成到 Web 平台部署。主要开发重心在于 **Qt 桌面客户端**，作为铁路货检员的核心作业界面。

## 项目概述

TrainClient 用自动化、AI 辅助的检测系统替代人工货检作业。系统从三个视角（左侧、右侧、顶部）捕获通过的货运车厢高分辨率图像，拼接成完整视图，并为操作员提供智能检测界面，以识别装载异常。

### 核心特性

- **多相机同步采集** — 3 台海康线阵相机同时获取左、顶、右视角图像
- **实时 PLC 通信** — 与轨旁传感器和 PLC 控制器的二进制协议集成
- **高精度图像拼接** — 基于 OpenCV 的无缝图像合成，支持 120 km/h 车速
- **智能检测** — AI 辅助异常识别（货物残留、车门状态、超限检测）
- **桌面 + Web 双客户端** — 功能完整的 Qt 桌面应用 + 浏览器远程访问
- **REST API 后端** — JWT 认证的 HTTP API，支持系统集成

## 系统架构

```
┌──────────────────────────────────────────────────────────────────┐
│                      室外数据采集子系统                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────────┐  │
│  │ 左侧相机 │  │ 顶部相机 │  │ 右侧相机 │  │ PLC + AEI +    │  │
│  │ (线阵)   │  │ (线阵)   │  │ (线阵)   │  │ 磁钢传感器     │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └───────┬────────┘  │
│       └──────────────┼──────────────┘               │           │
│                      ▼                              ▼           │
├──────────────────────────────────────────────────────────────────┤
│                       机房子系统（室内）                          │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  Qt6 应用服务器                              │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐  │  │
│  │  │ CTrainTcp   │  │ WebServer    │  │ ImageServer      │  │  │
│  │  │ Server      │  │ (REST API)   │  │ (端口 9091)      │  │  │
│  │  │ (端口 8989) │  │ (端口 8080)  │  │                  │  │  │
│  │  └──────┬──────┘  └──────┬───────┘  └────────┬─────────┘  │  │
│  │         │                │                    │            │  │
│  │  ┌──────┴──────────────┴────────────────────┴─────────┐   │  │
│  │  │              核心处理流水线                           │   │  │
│  │  │  图像拼接 → 数据库存储 → 智能识别                    │   │  │
│  │  └────────────────────────────────────────────────────┘   │  │
│  └────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│                      客户端访问层                                 │
│  ┌─────────────────────────┐  ┌───────────────────────────────┐  │
│  │   Qt 桌面客户端          │  │   Web 浏览器客户端             │  │
│  │   （主要交互界面）        │  │   （Vue.js + Element UI）     │  │
│  └─────────────────────────┘  └───────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

## 项目结构

```
TrainClient/
├── Backend/                    # Qt6 服务端应用（C++20）
│   ├── main.cpp               # 入口点
│   ├── mainwindow.cpp/h       # GUI 主窗口
│   ├── csystemconfig.cpp/h    # 配置管理
│   ├── cdatabasemanager.cpp/h # SQLite 数据库层
│   ├── imageuploader.cpp/h    # 远程图片上传器
│   ├── logger.cpp/h           # 日志系统
│   ├── Net/                   # 网络模块
│   │   ├── ctraintcpserver.*  # PLC TCP 服务器（端口 8989）
│   │   ├── cdataprotocol.*    # 二进制协议解析器
│   │   ├── webserver.*        # HTTP REST API（端口 8080）
│   │   ├── imageserver.*      # 图片分发（端口 9091）
│   │   └── clienthandler.*    # 客户端连接处理器
│   ├── MVS/                   # 相机模块
│   │   ├── MvCamera.*         # 海康 MVS SDK 封装
│   │   ├── clinecameramanager.* # 线阵相机管理器
│   │   ├── clinecameraitem.*  # 单相机采集线程
│   │   ├── cimagestitching.*  # 图像拼接引擎
│   │   └── cconfigdialog.*    # 相机配置 UI
│   ├── common/                # 共享定义
│   │   ├── constants.h        # 常量（chunk 大小等）
│   │   └── protocol.*         # 客户端二进制协议
│   └── Utils/                 # 工具类
│       ├── imageutil.*        # OpenCV 图像工具
│       └── stringutil.*       # 字符串处理
│
├── Client/                    # Qt5 桌面客户端
│   ├── main.cpp
│   ├── mainwindow.*
│   ├── login/                 # 登录认证模块
│   ├── dashboard/             # 首页仪表盘
│   ├── inspection/            # 过车检车（核心工作流）
│   ├── history/               # 历史查询与导出
│   ├── annotation/            # 车型标注（AI 训练数据）
│   └── admin/                 # 系统管理
│
├── Web/                       # Vue.js 前端（仅 dist 产物）
│   └── dist/                  # 生产构建产物
│
├── ApiTest/                   # API 测试工具
├── .copilot_bridge/           # 开发任务文档
└── README.md
```

## 技术栈

| 层级 | 技术 |
|------|------|
| **后端框架** | Qt 6.9.2 + C++20 |
| **GUI** | Qt Widgets（桌面端）、Vue.js + Element UI（Web 端） |
| **图像处理** | OpenCV 4.12（MinGW 编译） |
| **相机 SDK** | 海康威视 MVS（win64） |
| **数据库** | SQLite（appdata.db）+ QSql |
| **HTTP 服务器** | Qt HttpServer（内嵌） |
| **编译器** | MinGW |
| **构建系统** | CMake（Backend）、qmake（Server） |

## 通信协议

| 接口 | 协议 | 端口 | 说明 |
|------|------|------|------|
| PLC ↔ 服务端 | TCP（二进制，大端序） | 8989 | 列车到达/离开、车辆数据 |
| AEI ↔ 服务端 | RS232（9600 bps） | 串口 | 车号识别 |
| Web API | HTTP REST（JWT 认证） | 8080 | 系统集成接口 |
| 图片传输 | TCP（二进制，64KB 分块） | 9091 | 客户端图片分发 |

## 数据库设计

| 表名 | 说明 |
|------|------|
| `TrainInformation` | 列车记录（车号、时间、方向、图片） |
| `CarriageTable` | 车厢详情（车号、车型、图片、检视状态） |
| `Users` | 用户账户（认证、角色、权限） |
| `user_tokens` | JWT 令牌（24 小时过期） |

## 构建说明

### 环境要求

- Qt 6.9.2+（MinGW 套件）
- OpenCV 4.12（MinGW 编译版）
- 海康威视 MVS SDK
- CMake 3.29+

### Backend 构建

```bash
cd Backend
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Client 构建

在 Qt Creator 中打开 `Client/Client.pro`，使用 Qt 5.x 套件构建。

## 配置说明

编辑应用目录下的 `config.ini`：

```ini
[Server]
ListenPort=9090
WebServerPort=8080
PLCAddress=192.168.0.1

[Camera]
LineCameraSamplingHz=20000
ExposureTime=0.05
ImageHeight=2048

[Storage]
TrainImagesSavePath=c:\trainImage
LineCameraGrabbingFilePath=c:\grabbing
```

## 使用方式

1. 在服务器上启动 `Backend` 应用程序
2. 打开 Qt 桌面客户端或访问 `http://server:8080`
3. 登录系统（默认账户：`admin` / `admin`）
4. 查看列车检车仪表盘、历史记录，或管理系统设置

## 性能指标

| 指标 | 目标值 |
|------|--------|
| 车号识别准确率 | > 99.9% |
| 单节车辆判断准确率 | > 98% |
| 图像成图率 | > 98% |
| 适应车速 | 0–120 km/h |
| 智能识别准确率 | > 80% |
| 检车效率提升 | 较人工提升 70% |

---

<div align="center">

**Built with ❤️ for railway safety**

**为铁路安全而生**

</div>
