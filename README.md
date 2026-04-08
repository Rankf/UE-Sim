<div align="center"> 

## **UE-Sim: End-to-End Ultra Ethernet Simulation Platform**

<div align="center">

[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![ns--3-3.44](https://img.shields.io/badge/ns--3-3.44-green.svg)](VERSION)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu%2020.04+-lightgrey.svg)](#environment-requirements)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-orange.svg)](https://isocpp.org/)

[Project Website](https://kaima2022.github.io/UE-Sim/) | [GitHub Repository](https://github.com/kaima2022/UE-Sim)

UE-Sim is an Ultra Ethernet network simulator based on ns-3 for AI and HPC data center network research, protocol validation, and performance benchmarking.


<div align="center">
  <table>
    <tr>
      <td align="center" width="50%">
        <img src="images/logo/xgs_logo_.png" alt="信工所Logo" width="180"/><br/>
        <strong>Institute of Information Engineering</strong><br/>
        Chinese Academy of Sciences<br/>
        <em>State Key Laboratory of Cyberspace Security Defense</em>
      </td>
      <td align="center" width="50%">
        <img src="images/logo/wdzs_logo.png" alt="微电子所Logo" width="180"/><br/>
        <strong>Institute of Microelectronics</strong><br/>
        Chinese Academy of Sciences<br/>
        <em>Artificial Intelligence Chip and System Research and Development Center</em>
      </td>
    </tr>
  </table>
</div>

</div>


</div>

## Table of Contents

- [UE-Sim Overview](#ue-sim-overview)
- [System Architecture](#system-architecture)
  - [Core Components](#core-components)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
  - [Environment Requirements](#environment-requirements)
  - [Installation](#installation)
  - [Usage](#usage)
- [Changelog](#changelog)
- [Contributing](#contributing)
- [Contact us](#contact-us)
- [Citation](#citation)
- [Acknowledgments](#acknowledgments)
- [License](#license)

---

## UE-Sim Overview

UE-Sim is an end-to-end network simulation platform for the [Ultra Ethernet(UE) Specification](https://ultraethernet.org/). 

Ultra Ethernet is a specification of new protocols for use over Ethernet networks and optional enhancements to existing Ethernet protocols that improve performance, function, and interoperability of AI and HPC applications. The Ultra Ethernet specification covers a broad range of software and hardware relevant to AI and HPC workloads: from the API supported by UE-compliant devices to the services offered by the transport, link, and physical layers, as well as management, interoperability, benchmarks, and compliance requirements. 

UE-Sim serves two primary objectives:

- **Network Configuration and Performance Evaluation**: UE-Sim provides an Ultra Ethernet simulation platform for GPU manufacturers, AI data center operators, and other users. It supports constructing topologies of various scales, configuring parameters, and evaluating network performance under different workloads.

- **Ultra Ethernet (UE) Specification Optimization**: The platform enables researchers to optimize the Ultra Ethernet (UE) specification through algorithm innovation and protocol validation.

### Current Soft-UE Validation Status

`src/soft-ue` currently provides a regression-backed semantic model for core RUD behavior, including `SEND expected/unexpected`, `RC_NO_MATCH -> retry`, late duplicate replay, basic `READ/WRITE`, receive-side resource tracking, validation failure classification, and control-pressure style profiles.

What is validated now:

- `rud-semantic-negative-test`
- `rud-soak-smoke-test`
- `rud-control-pressure-test --profile=baseline|credit_pressure|lossy|mixed`
- `soft-ue-bridge-export-test`
- `soft-ue-observer-test`

What is not yet claimed as complete:

- broader Phase 4 system-level integration beyond the current read-only observer/logging bridge
- full external trace surface for every protocol-internal event class
- broader control-plane generalization beyond the currently validated semantic/runtime model

Current system-level bridge status:

- `SoftUeNetDevice` exports read-only protocol/runtime/failure snapshots.
- `sue-sim-module` can observe those snapshots through `SoftUeObserverHelper` and write protocol CSV logs.
- The legacy `PointToPointSueNetDevice` path remains separate and is not merged with `soft-ue` protocol semantics in this phase.

System scenario modes:

- `--systemScenarioMode` is required for `SUE-Sim`
  - there is no implicit default mode
  - runs that omit it abort immediately instead of silently falling back to legacy mode
- `scratch/SUE-Sim/SUE-Sim.cc --systemScenarioMode=legacy_sue`
  - legacy system/link experiment path
  - not labeled as `soft-ue truth-backed`
- `scratch/SUE-Sim/SUE-Sim.cc --systemScenarioMode=soft_ue_truth`
  - truth-backed system scenario path that instantiates one `SoftUeNetDevice` per `(xpu, port)`
  - auto-installs `SoftUeObserverHelper` by default
  - writes protocol/failure/diagnostic CSV only when the truth path is active
  - this is the only system entry that is labeled as `soft-ue truth-backed`

Truth-backed experiment classes:

- `--truthExperimentClass=semantic_demo`
  - deterministic demo path
  - covers `SEND`, `WRITE`, `READ`, and an intentional validation failure
- `--truthExperimentClass=system_smoke`
  - multi-node, multi-port, multi-flow truth-backed system smoke path
  - supports `uniform`, `trace`, and `fine-grained` planning
- `--truthExperimentClass=system_pressure`
  - truth-backed system pressure path
  - focuses on traffic plus receive-side resource pressure

Truth-backed system experiment matrix:

- `soft_ue_truth + semantic_demo`
  - labeled as truth-backed semantic demo
- `soft_ue_truth + system_smoke`
  - labeled as truth-backed system smoke
- `soft_ue_truth + system_pressure`
  - labeled as truth-backed system pressure
- any `legacy_sue` run
  - legacy system scenario only; not truth-backed

Truth-backed benchmark matrix:

- `python3 performance/run-soft-ue-truth-benchmarks.py`
  - benchmark entry for `soft_ue_truth + system_pressure`
  - runner hard-enforces `systemScenarioMode=soft_ue_truth` and `truthExperimentClass=system_pressure`
  - expands `truthPressureProfile x truthFlowCount x truthPayloadBytes x repetition`
  - consumes protocol/failure/diagnostic CSV from `PerformanceLogger`
  - writes `benchmark-summary.csv`, `profile-comparison.csv`, and `benchmark-report.md`
  - `--enforce-long-run-gate` runs `soft-ue-truth-pressure-system-test --long` first and refuses to emit an internal report if any profile fails
- `python3 performance/run-soft-ue-truth-benchmarks.py --config performance/config/soft-ue-truth-benchmark-matrix-jumbo.json`
  - focused jumbo benchmark entry for large-payload truth-backed SEND validation
  - keeps the default `2500`-byte device MTU matrix unchanged
  - runs with `Mtu=9000` and `PayloadMtu=8192`
- `python3 performance/run-soft-ue-truth-benchmarks.py --config performance/config/soft-ue-truth-benchmark-matrix-payload4096.json`
  - focused truth-backed benchmark entry for the UEC `PayloadMtu=4096` profile
  - uses a legal device MTU large enough to carry the configured payload MTU
- `python3 performance/run-soft-ue-truth-benchmarks.py --config performance/config/soft-ue-truth-throughput-matrix.json`
  - throughput-oriented truth-backed matrix
  - expands `baseline/credit_pressure x payload2048/payload4096/payload8192 x send_only/write_only/read_only`
  - intended for internal payload-MTU throughput comparison, not for long-run gate enforcement
- `python3 performance/run-soft-ue-internal-report.py`
  - standard internal report entry
  - runs correctness gate (`SEND/WRITE/READ` jumbo tests), long-run gate, then the throughput matrix
  - emits `internal-performance-report.md`, `benchmark-summary.csv`, `profile-comparison.csv`, `correctness-gate.json`, and `report-metadata.json`
- `legacy_sue`
  - excluded from truth-backed benchmark conclusions
  - not part of the Phase 4.8 KPI/profile comparison path

MTU semantics:

- `Mtu`
  - device/link MTU in bytes, including headers
  - this is the value applied to `PointToPointSueNetDevice` and `SoftUeNetDevice`
- `PayloadMtu`
  - UEC semantic payload MTU for `soft_ue_truth`
  - supported values in this phase: `2048`, `4096`, `8192`
  - used for semantic fragmentation, reassembly, and truth planner multi-chunk decisions
- semantic header overhead
  - fixed at `128` bytes for the current model
  - `PayloadMtu + 128` must fit within device `Mtu`
- the current default keeps `Mtu=2500`
- the current default keeps `PayloadMtu=2048`
- jumbo experiments use `Mtu=9000` with `PayloadMtu=8192`
- `legacy_sue` ignores `PayloadMtu` in this phase

Benchmark KPI semantics:

- `semantic_goodput_mbps`
  - internal headline KPI for truth-backed pressure runs
  - derived from requester-side successful completion payload bytes divided by simulation time
- `semantic_completion_rate_pct`
  - requester-side successful completions divided by started operations
- `terminalization_rate_pct`
  - requester-side terminalized operations divided by started operations
- `success_latency_p50_us / p95_us / p99_us`
  - computed from requester-side successful completion records only
- `device_tx_egress_mbps`
  - limited performance conclusion only
  - derived from sender-side `SoftUeNetDevice.totalBytesTransmitted / simulationTime`
  - transport-efficiency supporting metric, not semantic goodput
- `device_rx_observed_mbps`
  - diagnostic-only
  - may be zero or under-reported because semantic receive placement can complete in `PDS/SES` without passing through `DeliverReceivedPacket()`
- control-plane/resource/packet-reliability/failure-domain counters
  - pressure evidence, not throughput KPIs
- Internal performance reports additionally require:
  - `soft_ue_truth + system_pressure`
  - passing long-run gate across `baseline`, `credit_pressure`, `lossy`, and `mixed`
- Still not claimed in this phase:
  - receiver-side observed Mbps as a headline KPI
  - `legacy_sue` benchmark conclusions
  - external NIC performance

Example:

```bash
python3 performance/run-soft-ue-truth-benchmarks.py \
  --config performance/config/soft-ue-truth-benchmark-matrix.json \
  --enforce-long-run-gate

python3 performance/run-soft-ue-truth-benchmarks.py \
  --config performance/config/soft-ue-truth-benchmark-matrix-payload4096.json \
  --enforce-long-run-gate

python3 performance/run-soft-ue-truth-benchmarks.py \
  --config performance/config/soft-ue-truth-benchmark-matrix-jumbo.json \
  --enforce-long-run-gate
```

---

## System Architecture

<p align="center">
  <img src="attachment/SUETArchitecture.png" alt="UE-Sim Architecture" width="90%"/>
</p>

### Core Components

<p align="center">
  <img src="attachment/CoreComponents.png" alt="UE-Sim Core Components" width="90%"/>
</p>

- **SES (Semantic Sub-layer)**
  - Responsible for semantic processing of transaction requests, including endpoint addressing, authorization verification, and metadata management.
  - Implements **packet slicing (fragmentation)** functionality, breaking down large transactions into multiple packets suitable for network transmission (e.g., based on MTU).
  - Manages Message Sequence Numbers (MSN) for message boundary identification (SOM/EOM) and reassembly.

- **PDS (Packet Delivery Sub-layer)**
  - Acts as the central dispatcher for packet delivery.
  - Handles **PDC allocation and management**, assigning SES packets to specific Packet Delivery Contexts.
  - Performs packet routing, classification, and error handling for events not associated with a specific PDC.
  - Coordinates congestion control and traffic management policies.

- **PDC (Packet Delivery Context)**
  - Represents the transport context for a specific flow or transaction.
  - **IPDC (Immediate PDC)**: Provides unreliable, low-latency transmission for delay-sensitive traffic (no ACK/retransmission).
  - **TPDC (Transactional PDC)**: Provides reliable transmission with acknowledgment mechanisms and **Retransmission Timeout (RTO)** logic to ensure guaranteed delivery and ordering.

---

## Repository Structure

The directory tree below highlights core UE-Sim-related paths and is not exhaustive.

```
UE-Sim/
├── src/soft-ue/                      # UEC protocol stack implementation
│   ├── model/
│   │   ├── ses/                      # SES (Semantic Sub-layer)
│   │   │   ├── ses-manager.h/cc     # SES manager and coordination
│   │   │   ├── operation-metadata.h/cc  # Message sequence number (MSN) management
│   │   │   └── msn-entry.h/cc       # MSN entry tracking
│   │   ├── pds/                      # PDS (Packet Delivery Sub-layer)
│   │   │   ├── pds-manager.h/cc     # Central packet dispatcher
│   │   │   ├── pds-header.cc        # PDS header definitions
│   │   │   ├── pds-common.h         # Common PDS definitions
│   │   │   └── pds-statistics.cc    # PDS statistics tracking
│   │   ├── pdc/                      # PDC (Packet Delivery Context)
│   │   │   ├── pdc-base.h/cc        # Base PDC interface
│   │   │   ├── ipdc.h/cc            # IPDC: Unreliable, low-latency transport
│   │   │   ├── tpdc.h/cc            # TPDC: Reliable transport with ACK
│   │   │   └── rto-timer/           # Retransmission timeout logic
│   │   │       └── rto-timer.h/cc   # RTO timer implementation
│   │   ├── network/                  # ns-3 integration layer
│   │   │   ├── soft-ue-net-device.h/cc    # Network device implementation
│   │   │   └── soft-ue-channel.h/cc       # Channel model
│   │   └── common/                   # Shared utilities
│   │       ├── soft-ue-packet-tag.h/cc    # Packet tags
│   │       └── transport-layer.h    # Transport layer definitions
│   ├── helper/                       # Helper classes for simulation setup
│   │   └── soft-ue-helper.h/cc      # Main helper API
│   └── test/                         # Unit and integration tests
│       ├── ses-manager-test.cc      # SES layer tests
│       ├── pds-manager-test.cc      # PDS layer tests
│       └── pds-full-test.cc         # Full PDS integration tests
│
├── scratch/                          # Example programs and experiments
│   ├── Soft-UE/                      # Throughput stress test
│   │   └── Soft-UE.cc               # High-load performance testing
│   └── Soft-UE-E2E-Concepts/         # End-to-end walkthrough
│       └── uec-e2e-concepts.cc      # Protocol flow demonstration
│
├── examples/                         # Usage examples and tutorials
│   ├── first-soft-ue.cc             # Getting started example
│   ├── performance-benchmark.cc     # Performance testing suite
│   └── ai-network-simulation.cc     # AI workload simulation
│
├── benchmarks/                       # Performance benchmark suite
│   ├── soft-ue-performance-benchmark.cc   # Comprehensive benchmarks
│   └── e2e-demo-optimized.cc        # Real-world workload demos
│
├── docs/                             # Documentation
├── attachment/                       # Architecture diagrams
│   ├── SUETArchitecture.png
│   └── CoreComponents.png
├── CMakeLists.txt                    # Build configuration
├── ns3                               # ns-3 build script
├── README.md                         # This file
└── VERSION                           # Version: 1.0.0
```

**Key Directories:**
- **`src/soft-ue/model/`**: Core protocol implementation (SES, PDS, PDC layers)
- **`scratch/`**: Runnable test programs demonstrating key features
- **`examples/`**: Tutorial-style examples for users
- **`benchmarks/`**: Performance validation and comparison tests

---

## Getting Started

### Environment Requirements

- **Operating System**: Linux (Ubuntu 20.04.6 LTS)
- **Compilers**:
  - **GCC**: 10.1.0+
  - **Clang**: 11.0.0+
  - **AppleClang**: 13.1.6+ (macOS)
- **Build Tools**:
  - **CMake**: 3.13.0+ (Required)

### Installation

#### Step 1: Install System Dependencies

First, install the essential build tools and libraries:

```bash
sudo apt update
sudo apt install build-essential cmake git software-properties-common
```

#### Step 2: Check and Upgrade GCC Version

Check your current GCC version:

```bash
gcc --version
```

**If your GCC version is 10.1.0 or higher**, proceed to [Step 3](#step-3-clone-and-configure-ue-sim).

**If your GCC version is below 10.1.0** (Ubuntu 20.04 default is 9.3.0), upgrade it:

```bash
# Add Ubuntu Toolchain PPA for newer GCC versions
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
# Install GCC 10 and G++ 10
sudo apt install gcc-10 g++-10
# Set GCC 10 as the default compiler
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10
```

#### Step 3: Clone and Configure UE-Sim

> **Important Note**: [This version of ns3 does not allow direct execution as root user](https://groups.google.com/g/ns-3-users/c/xDtfcaUrCwg?pli=1), please run the following commands as a regular user.

```
# Clone the project
git clone https://github.com/kaima2022/UE-Sim.git
cd UE-Sim

# Configure ns-3 environment
./ns3 configure --enable-examples --enable-tests
```

####  Step 4: Build 

```bash
./ns3 build
```

---

### Usage

UE-Sim supports end-to-end concept walkthroughs.

#### Concept Walkthrough Mode
Demonstrates the basic protocol interaction with annotated logs, illustrating the flow through SES (fragmentation), PDS (dispatch), and PDC:
```bash
./ns3 run uec-e2e-concepts -- --transactionSize=4000 --packetCount=2
```

---

## Changelog

### v1.0.0
- **Initial Release**:
  - Complete implementation of SES, PDS, and PDC layers.
  - Implemented SES packet segmentation/reassembly and metadata management.
  - Implemented PDS packet dispatching and PDC allocation logic.
  - Support for both IPDC (unreliable) and TPDC (reliable) transport contexts.

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
- Use **GitHub Issues** for bug reports and feature requests.
- For code changes, submit a Pull Request with a runnable reproduction command if applicable.

---

## Contact us

For questions, suggestions, or bug reports, please feel free to contact us:

- **Project Email**: chasermakai@gmail.com

---

## Citation

If you find this project useful for your research, please consider citing it in the following format:

```bibtex
@software{UECSim,
  title   = {{UE-Sim: End-to-End Ultra Ethernet Simulation Platform}},
  url     = {https://github.com/kaima2022/UE-Sim},
  version = {v1.0.0},
  year    = {2025}
}
```

---

## Acknowledgments

This project is based on [Soft-UE](https://github.com/lipu0324/Soft-UE). We greatly appreciate their pioneering work on the Ultra Ethernet software prototype.

---

## License

GPLv2 License. See `LICENSE`.

---

<div align="center">

If you find this project helpful, please consider giving it a ⭐ star! Your support is greatly appreciated.

Made by the UE-Sim Project Team

</div>
