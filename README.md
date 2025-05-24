# Optimizing Memory Performance in Multi-Core Systems

**Author:** Akshay Bhosale  
**Roll Number:** 234101006  
**Program:** M.Tech (Computer Science)  
**Thesis Guide:** Dr. Aryabartta Sahu  
**Institute:** Indian Institute of Technology Guwahati  

---

## 🎯 Problem Statement

In NUMA-based multi-core systems, DRAM access latency varies by core-to-channel distance. Default page placement strategies ignore this and often lead to remote memory accesses. We address this by adding lightweight, runtime translation policies to place data closer to frequently accessing cores, without hardware or OS changes.

---

## 🛠️ Implemented Translation Methods

We added two new page placement techniques under:

```
ramulator2/src/translation/impl/
```

- **Local_to_requester.cpp**  
  → Maps each page to the channel closest to the first accessing core

- **Dynamic_migration.cpp**  
  → Periodically migrates hot pages to channels with lowest access latency, based on per-core access frequency

---

## 🧩 Ramulator2 Modifications

To support NUMA-aware simulation:

- `src/frontend/simple_o3.cc` → Bypasses LLC to simulate direct DRAM traffic  
- `src/controller/generic_dram_controller.cc` → Adds latency penalty based on core-to-channel distance  
- `src/utils/utils.{h,cpp}` → Maintains core-to-channel latency matrix  
- `src/translation/impl/` → Includes:
  - `Local_to_requester.cpp`
  - `Dynamic_migration.cpp`

---

## 📤 SniperSim Modifications

- Modified to print DRAM memory accesses via `std::cout`
- Output is written to `dram_trace.txt` after running a benchmark



## 🏁 How to Run the Complete Flow

### 1. Generate Trace Files using SniperSim

```bash
cd snipersim
make
./run-sniper -n <num_cores> -g -g perf_model/l3_cache/shared_cores=1 -- <benchmark_binary>
```

Then process `dram_trace.txt` as described below.

### 2. Trace Preparation in desired format (2 Steps)

```bash
python3 sort.py dram_trace.txt sorted_trace.txt
python3 converttrace.py sorted_trace.txt
```

Generates one trace file per core (`core_0.txt`, `core_1.txt`, ...) to be used in Ramulator2.

---

---

### 2. Run Ramulator2 with Trace Files

```bash
cd ramulator2
mkdir build && cd build
cmake ..
make -j
./ramulator -f ../example_config.yaml
```

### Config Notes:
- Set `traces:` to one file per core
- Set `Translation.impl` to:
  - `Dynamic_migration`
  - `Local_to_requester`
  - `Random_Translation2`
- Parameters for Dynamic Migration:
  - `hot_page_threshold`
  - `window_size`
  - `cooldown_windows`

---

## 📄 License

This project is for academic, non-commercial use only. Based on open-source simulators Ramulator2 and SniperSim.
