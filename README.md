# PCIe System TLM2.0 Simulator
This project implement a system simulation model including PCIe.

## Simulator Information
### Version
v2.0
### Component Diagram
![image info](./simulator_diagram.png)
### New Features
1. Introduce PCIe hierachy
   - Transaction Layer
     - send TLP
     - tag management
     - credit management
   - Data Link Layer
     - send DLLP
     - seqNum management
     - replay buffer (header, payload)

#### Simulater Output
![image info](./TLP_outstanding.png)

#### TLM2.0 4-way handshake
![image info](./TLM2.0_handshake.png)
 
## Compile and Run
```
make
./_sim
```
