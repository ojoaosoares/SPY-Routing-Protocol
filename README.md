# SPY – Same Parity Yonder Routing Protocol for ns-3.29

This repository provides a **plugin for ns-3.29** that implements **SPY (Same Parity Yonder) Routing Protocol** — a **stateless, greedy, geographic routing protocol** that enhances packet delivery using **dual node-disjoint paths with same-hop parity constraints**.

---

## 🛰️ What is SPY Routing Protocol?

**SPY** is a geographic routing protocol designed for wireless ad hoc networks. It differs from traditional stateless greedy routing (like GPSR) by:

- Using **node positions** not just to forward packets greedily, but to **compute two disjoint paths** from source to destination.
- Ensuring **same-parity hop counts** for the dual paths (e.g., both paths are even or both are odd in length).
- Improving **redundancy and reliability** with minimal per-node state overhead.
- Maximizing **throughput** by leveraging dual disjoint paths for parallel, reliable packet delivery

---

## 📁 Project Structure

This repository is meant to be cloned **inside the `src/` folder** of an unmodified [ns-3.29](https://www.nsnam.org) source tree.

## 📦 Requirements

- Linux or macOS
- `gcc` version **< 10** (e.g., 9.x)
- `python2.7` (available as `python2`)
- `make`, `git`, `wget`, `tar`
- Recommended compiler flags:
  
  ```bash
  export CXXFLAGS="-Wall -std=c++0x"
  ```

## Instalation

### Download ns3.29

```bash
wget https://www.nsnam.org/releases/ns-allinone-3.29.tar.bz2
tar -xjf ns-allinone-3.29.tar.bz2
cd ns-allinone-3.29/ns-3.29
```

### Add the SPY module

Clone the SPY repository inside the src/ directory:

```bash
cd src
git clone https://github.com/ojoaosoares/SPY-Routing-Protocol
```

(Optional) If you also want GPSR

```bash
git clone https://github.com/dwosion/ns3.29-with-gpsr.git
cd ns3.29-with-gpsr/ns-3.29/src
git clone https://github.com/ojoaosoares/SPY-Routing-Protocol
```

## 3. Configure and Build

```bash
cd ..
./waf configure --enable-examples --enable-tests
./waf build
```