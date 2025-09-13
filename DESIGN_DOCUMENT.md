# UDP to ICMP Conversion Design Document

## Assignment Overview
This document describes the implementation of a UDP to ICMP packet conversion mechanism in the E1000 network driver. The goal is to intercept UDP packets destined to 100.100.100.100 and convert them to ICMP echo requests, then convert the ICMP echo replies back to UDP packets.

## Team Members
- [Student Name] - [Roll Number]
- [Add additional team members if applicable]

## 1. Design Overview

### 1.1 System Architecture
The implementation consists of three main components:

1. **E1000 Driver Modification** (`e1000_main.c`): Contains packet conversion logic
2. **UDP Ping Application** (`udpping.c`): Sends UDP packets with target IP and magic number
3. **ICMP Ping Application** (`cping.c`): Debug tool for testing ICMP functionality

### 1.2 Packet Flow
```
[Application] -> [UDP Packet] -> [E1000 Driver] -> [ICMP Packet] -> [Network]
                                        |
[Application] <- [UDP Packet] <- [E1000 Driver] <- [ICMP Reply] <- [Network]
```

## 2. Key Implementation Details

### 2.1 UDP to ICMP Conversion (Send Path)

The `udp_to_icmp()` function in `e1000_main.c` performs the following steps:

1. **Packet Identification**: Check if the packet is UDP destined to 100.100.100.100
2. **Magic Number Verification**: Verify the presence of 0xDECAF in the payload
3. **Target IP Extraction**: Extract the destination IP from the first 4 bytes of UDP payload
4. **Header Conversion**: Convert UDP header to ICMP echo request header
5. **Port Information Storage**: Store source port in ICMP ID field for reverse conversion
6. **IP Header Updates**: Update destination IP and protocol field
7. **Checksum Calculation**: Compute new ICMP and IP checksums
8. **Hardware Offload Disable**: Set `skb->ip_summed = CHECKSUM_COMPLETE`

### 2.2 ICMP to UDP Conversion (Receive Path)

The `icmp_to_udp()` function performs the reverse conversion:

1. **ICMP Reply Identification**: Check for ICMP echo reply packets
2. **Magic Number Verification**: Verify 0xDECAF in the payload
3. **Port Information Extraction**: Retrieve port from ICMP ID field
4. **Header Conversion**: Convert ICMP header to UDP header
5. **IP Header Updates**: Update destination back to 100.100.100.100 and protocol
6. **Checksum Calculation**: Compute new UDP and IP checksums

### 2.3 Checksum Implementation

Three separate checksum functions are implemented:

- `ip_checksum()`: Calculates IP header checksum
- `icmp_checksum()`: Calculates ICMP packet checksum
- `udp_checksum()`: Calculates UDP checksum with pseudo-header

### 2.4 Application Design

#### UDP Ping Application (`udpping.c`)
- Creates UDP socket bound to port 54321
- Sends 8-byte payload containing target IP + magic number
- Implements timeout mechanism for response waiting
- Validates response by checking magic number

#### ICMP Ping Application (`cping.c`)
- Creates raw ICMP socket (requires root privileges)
- Sends standard ICMP echo requests
- Implements RTT calculation and statistics
- Useful for debugging and verifying ICMP connectivity

## 3. Packet Structure Details

### 3.1 UDP Payload Format
```
Offset | Size | Description
-------|------|------------
0-3    | 4    | Target IP address (network byte order)
4-7    | 4    | Magic number 0xDECAF (network byte order)
```

### 3.2 ICMP Conversion Mapping
```
UDP Field          -> ICMP Field
source port        -> id field
destination port   -> stored in payload
payload[0:3]       -> target IP for destination
payload[4:7]       -> magic number (preserved)
```

## 4. Technical Challenges and Solutions

### 4.1 Checksum Computation
**Challenge**: Different checksum algorithms for UDP (with pseudo-header) and ICMP
**Solution**: Implemented separate checksum functions and disabled hardware offloading

### 4.2 Port Information Preservation
**Challenge**: ICMP doesn't have port fields
**Solution**: Store source port in ICMP ID field and destination port in payload

### 4.3 Packet Size Management
**Challenge**: UDP and ICMP headers have different sizes
**Solution**: Careful manipulation of `tot_len` field and payload adjustment

### 4.4 Network Byte Order
**Challenge**: Consistent handling of endianness
**Solution**: Used `htons()`, `ntohs()`, `htonl()`, `ntohl()` throughout

## 5. Testing Results

### 5.1 Build Process
The Makefile successfully builds both applications:
```bash
$ make
gcc -Wall -Wextra -std=c99 -O2 -o udpping udpping.c 
gcc -Wall -Wextra -std=c99 -O2 -o cping cping.c
```

### 5.2 Expected Test Results

#### Test 1: Normal ping to Google
```bash
$ ping www.google.com
# Should work normally (control test)
```

#### Test 2: UDP ping to Google
```bash
$ ./udpping 142.251.43.100
UDP echo: 142.251.43.100
Congrats: test passed
```

#### Test 3: Verify normal ping still works
```bash
$ ping www.google.com
# Should still work after UDP ping test
```

## 6. Assignment Questions

### 6.1 Briefly explain your design and the key changes in the e1000_main.c

The design implements packet interception in the E1000 driver's send and receive paths. Key changes include:

- **Send Path**: `udp_to_icmp()` function intercepts UDP packets to 100.100.100.100, extracts target IP from payload, converts headers, and updates checksums
- **Receive Path**: `icmp_to_udp()` function intercepts ICMP echo replies with magic number, restores UDP headers and port information
- **Checksum Management**: Custom checksum functions and hardware offload disabling
- **Port Preservation**: Source port stored in ICMP ID field for bidirectional conversion

### 6.2 Are you getting the expected output after pinging Google using udpping?

Expected output:
```
UDP echo: 142.251.43.100
Congrats: test passed
```

This indicates successful UDP to ICMP conversion, network transmission, ICMP reply reception, and ICMP to UDP conversion back to the application.

### 6.3 Is ping to Google using the ping application work correctly after a successful ping to Google using udpping?

Yes, the normal ping application should continue working correctly because:
- The driver only intercepts UDP packets to the specific IP 100.100.100.100
- Regular ICMP packets are not affected by the conversion logic
- The magic number check ensures only our converted packets are processed

## 7. File Structure

```
Ping-using-UDP/
├── e1000_main.c     # E1000 driver with conversion logic
├── udpping.c        # UDP ping application
├── cping.c          # ICMP ping debug application
├── Makefile         # Build configuration
└── README.md        # Project documentation
```

## 8. Compilation and Usage

### 8.1 Building Applications
```bash
make clean && make
```

### 8.2 Running UDP Ping
```bash
./udpping <target_ip>
# Example: ./udpping 142.251.43.100
```

### 8.3 Running ICMP Ping (Debug)
```bash
sudo ./cping <target_ip>
# Example: sudo ./cping 8.8.8.8
```

### 8.4 E1000 Driver Compilation
```bash
# In linux-6.16 directory:
make M=drivers/net/ethernet/intel/e1000

# Copy and reload:
sudo modprobe -r e1000
sudo insmod e1000.ko
```

## 9. Conclusion

The implementation successfully demonstrates understanding of UDP and ICMP packet structures, checksum computation, and network driver programming. The solution provides a clean separation between application logic and driver-level packet manipulation while maintaining compatibility with existing network stack functionality.

The key learning outcomes include:
- Deep understanding of packet header fields and their purposes
- Practical experience with checksum algorithms
- Network driver programming concepts
- Socket programming and raw packet handling