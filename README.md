# Optimizing Memory Performance in Multi-Core Systems

**Author:** Akshay Bhosale  
**Roll Number:** 234101006  
**Program:** M.Tech (Computer Science)  
**Thesis Guide:** Dr. Aryabartta Sahu  
**Institute:** Indian Institute of Technology Guwahati  

---

## 🧠 Project Overview

This thesis project explores memory optimization strategies for multi-core systems by modifying DRAM access behavior. The work involves:
- Bypassing cache layers to simulate pure DRAM traffic
- Implementing custom page placement and migration strategies based on core access patterns
- Evaluating performance using Ramulator2 and trace input from SniperSim

---

## 📁 Repository Structure

```
.
├── ramulator2/   # Modified DRAM simulator with custom translation policies
└── snipersim/    # Modified SniperSim used to generate DRAM access traces
```

---

## 🔧 Ramulator2 (DRAM Simulator)

### 🛠️ Key Modifications
- Custom translation policies:
  - `FCFSTranslation`: maps page to channel closest to first accessing core
  - `DynamicTranslation3`: migrates hot pages based on access distribution and latency gain
- LLC bypass in SimpleO3 frontend
- Channel-aware latency modeling in DRAM controller
- Core-to-channel latency matrix added via `utils.cpp`

### 🏗️ Build Instructions
```bash
cd ramulator2
mkdir build
cd build
cmake ..
make -j
```

### ▶️ Run Example
```bash
./ramulator -f ../example_config.yaml
```

### 🛠️ Config Notes
- Use one trace file per core
- Translation policy options:
  - `Dynamic_migration`
  - `Local_to_requester`
  - `Random_Translation2`
- Tunable parameters: `hot_page_threshold`, `window_size`, `cooldown_windows`

---

## 🧪 SniperSim (Trace Generator)

### 🛠️ Key Modifications
- Added `std::cout` statement in trace generation module to print memory accesses
- Used to generate DRAM-access-only traces by bypassing cache effects

### 🏗️ Build Instructions
```bash
cd snipersim
source scripts/env.sh
make
```

### ▶️ Run Example
```bash
./run-sniper -c gainestown -s stop-by-icount:10000000 -d output_folder ./test/matrix_multiply
```

You can redirect trace output to custom files for Ramulator input.

---

## 📌 Notes

- All experimental results in the thesis were obtained using the setup in this repository.
- Please cite appropriately if reusing or modifying this simulator setup.

---

## 📄 License

This project is for academic, non-commercial use only. Based on open-source simulators Ramulator2 and SniperSim.
