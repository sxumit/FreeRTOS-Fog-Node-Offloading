# Dynamic Fog Offloading for Real-Time Cyber-Physical Systems

![Hardware](https://img.shields.io/badge/Hardware-ESP32-orange.svg)
![Language](https://img.shields.io/badge/Language-Python%20%7C%20C%2B%2B-green.svg)
![Protocol](https://img.shields.io/badge/Protocol-MQTT-yellow.svg)
![Status](https://img.shields.io/badge/Status-Research_Active-blue.svg)

## 📌 Introduction
This repository contains the architecture, core logic, and empirical data for an experimental distributed computing framework. The project investigates dynamic task offloading from highly constrained edge microcontrollers to localized fog infrastructure. It bridges embedded systems development with distributed networking to ensure deterministic execution in real-time environments.

## ⚠️ The Problem: Transient CPU Contention
In modern Cyber-Physical Systems (CPS), edge nodes (like the ESP32) are frequently tasked with maintaining strict Real-Time Operating System (RTOS) control loops (e.g., motor control, autonomous sensor sampling). 

Under nominal workloads, these microcontrollers meet their deadlines. However, when subjected to sudden, heavy concurrent tasks (transient CPU contention), the local processor experiences thread starvation. During baseline testing with a static local execution schedule, this CPU stress caused a **20.00% deadline miss rate**, which is catastrophic for closed-loop industrial systems.

## 🏗️ System Architecture
To isolate and solve this bottleneck, a localized, two-tier distributed architecture was engineered:

1. **The Edge Node (ESP32):** Acts as the primary real-time controller. It runs a high-priority strict control loop (50ms deadline) and continuously monitors its own CPU load and thread execution times.
2. **The Fog Server (Python):** A localized, high-performance computational node designed to asynchronously process heavy algorithmic workloads on behalf of the edge node.
3. **The Communication Layer (MQTT):** A Mosquitto Pub/Sub broker facilitates the telemetry and payload transfer. MQTT was selected over HTTP for its minimal header footprint and deterministic Quality of Service (QoS), critical for low-latency CPS environments.

## 💡 The Solution & Empirical Results
Instead of a static execution policy, a **Dynamic Offloading Decision Engine** was implemented on the edge node. 

The edge node monitors impending computational stress. When a transient heavy task threatens the primary control loop, the system dynamically serializes the workload and offloads it to the Python Fog Server via MQTT. The fog server computes the payload and returns the result asynchronously, keeping the ESP32's primary thread unblocked.

### Performance Outcome:
* **Static Execution (Baseline):** 20.00% critical deadline miss rate.
* **Dynamic Offloading (Proposed):** **0.00% deadline miss rate**. The RTOS maintained 100% deterministic execution under the exact same synthetic CPU stress.

## 📄 Academic Documentation
This experimental framework has been thoroughly documented and analyzed in an IEEE conference-style research paper. The documentation covers the mathematical isolation of network latency, architectural trade-offs, and complete empirical CPU load characteristics. 

*The compiled documentation, and empirical datasets are maintained for ongoing academic networking and research.*
