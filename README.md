# Ping-using-UDP

A network driver implementation that converts UDP packets to ICMP echo requests and back, demonstrating understanding of packet structures and network protocols.

## Overview

This project implements a UDP to ICMP conversion mechanism in the E1000 network driver. When applications send UDP packets to a special IP address (100.100.100.100), the driver intercepts these packets, converts them to ICMP echo requests to the actual target, and converts the ICMP replies back to UDP packets for the application.

## Components

- **e1000_main.c**: E1000 driver modification with packet conversion logic
- **udpping.c**: UDP ping application that sends packets via the conversion mechanism
- **cping.c**: Traditional ICMP ping application for debugging
- **Makefile**: Build configuration for the applications
- **DESIGN_DOCUMENT.md**: Detailed technical documentation

## Building

```bash
make clean && make
```

## Usage

### UDP Ping (via conversion)
```bash
./udpping <target_ip>
# Example: ./udpping 142.251.43.100
```

### Direct ICMP Ping (for debugging)
```bash
sudo ./cping <target_ip>
# Example: sudo ./cping 8.8.8.8
```

## Expected Output

When working correctly, udpping should show:
```
UDP echo: 142.251.43.100
Congrats: test passed
```

## Driver Installation

The E1000 driver needs to be compiled and installed in a Linux kernel environment:

```bash
# In linux-6.16 directory:
make M=drivers/net/ethernet/intel/e1000

# Install the modified driver:
sudo modprobe -r e1000
sudo insmod e1000.ko
```

## Technical Details

- Intercepts UDP packets destined to 100.100.100.100
- Extracts target IP from UDP payload (first 4 bytes)
- Verifies magic number 0xDECAF (last 4 bytes)
- Converts UDP headers to ICMP echo request headers
- Stores port information for reverse conversion
- Handles checksum computation for both protocols
- Converts ICMP echo replies back to UDP packets

## Assignment Requirements

This implementation fulfills the requirements for understanding:
- UDP and ICMP packet structures
- Network driver programming
- Checksum computation algorithms
- Socket programming
- Packet header manipulation