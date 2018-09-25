# irq_heatmap
Command line heatmaps of linux interrupt vectors and cpu stats

## Why

There are several hard to visualise files. Interrupts, softirq. Most tools out there summarise them. 
If you're working in the low latency/high throughput space you want to see what's happening on a per 
cpu basis to evaluate the success of different pinning and isolation strategies. 

## Overview 

irq_heatmap discovers the topology of the machine (which can be dumped by using irq_numa) and then will
display metrics in a socket, core and hyperthread aware manner. Each line represents the delta from the line 
before. 

Allows a terminal visualisation of activity in the following files;
- /proc/stat. CPU utilisation -C user, sys, wio, idle, softirq, irq, all (which is effectively (1-idle))
- /proc/interrupts. IRQ utilisation. You can monitor an individual interrupt line (-I 171) or sum several interrupts together to view a device with multiple interrupts (-M p5p1)
- /proc/softirq. SOFTIRQ utilisation. e.g. -S SCHED or -S NET_RX 
- /proc/softnet_stat. Softnet statistics. -P packets, -P squeeze 

## Examples

### CPU 
![simple cpu heatmap][simple_cpu]














[simple_cpu]:https://github.com/andyphillips/irq_heatmap/raw/master/images/cpu_simple.png
