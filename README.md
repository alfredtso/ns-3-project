

# Background fdasj
> Excellent background, and comprehensive complete review and in-depth analysis.
# Problem definition
> Very well-defined and motivated problem(s) with clear and attainable objective(s).
# Design/Methodology 
> Thorough design/ methodology, academically rigorous and excellent critical thinking (e.g., considering different alternatives). Excellent result based on solid work.

## The UDP Packet Size
- MTU 1500, should we send 1500 ?
- Theoretically, 1472 max as 20 (IP) + 8 (IP) header
## Point-to-point
- Very simple point-to-point channel; Similar to RS-422 with no handshkaing
- There are two "wires" in the channel and each device gets one. Each wire is associated with IDLE or TRANSMITTING state
![[Pasted image 20210420173339.png]]
Simulation or Lab ?

## Use ns-3
![[Pasted image 20210420213325.png]]
![[Pasted image 20210421095910.png]]
![[Pasted image 20210421100054.png]]
## Steps
Physical Link: Delay 2ms, UDP in simulation lowest number for 5ms
No packet loss ?
# Result
## Expected results

## Limitation
### Queue
The abstraction of `NetDevice` in `ns-3` "covers both the software driver and the simulated hardware"
Two-layer queueing approach: since recent releases of ns-3, outgoing packets traverse two queueing layers before reaching the channel object

– The first software queueing layer encountered is called the ‘traffic control layer’

• Active queue management (RFC7567) and prioritization due to quality-of-service (QoS) takes place in a device-independent manner through the use of queueing disciplines

– The second hardware queueing layer is typically found in the NetDevice objects

• Different devices (e.g. LTE, Wi-Fi) have different implementations of these queues

• The traffic control layer is effective only if it is notified by the NetDevice when the device queue is full

– So that the traffic control layer can stop sending packets to the NetDevice

– Currently, flow control is supported by the following NetDevices, which use Queue objects (or objects of Queue subclasses) to store their packets (also support BQL)

• Point-To-Point, Csma, Wi-Fi, SimpleNetDevice

• The selection of queueing disciplines has a large impact on performance

– Highly impacted by the size of the queues used by the NetDevices

• Can be dynamically adjusted by enabling BQL (Byte Queue Limits) – Not autotuned, typically the simplest variants (e.g. FIFO scheduling with drop-tail behavior)

## Reference
[https://www.nsnam.org/consortium/activities/training/](https://www.nsnam.org/consortium/activities/training/)

### Inspiration
![[Pasted image 20210420165629.png]]