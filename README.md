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

This is a two socket machine. It's slightly damaged as can be seen from the numbering of the cores and asymmettry. 
Socket 0 has hyperthreads with 2 hyperthreads per core. Linux's view of the cpu number is listed in the cpu column.

So socket 0 has the following cpus listed by hyperthread siblings; (06,16), (07,17), (08,18), (09,19)

The user time is being show using the default blue-green-yellow scale. Each second the number of milliseconds
spent by the cpu in user state is shown on a log2 scale. In this case, 100% cpu would be 1000 ms/second which
is shown by the character 'a'. The scales and colour maps are in the help - see below. 

### Network Interrupts 
![network interrupts][network_interrupts]

This is a larger machine. It has a symmettrical topology, with 8 cores per socket and 2 hyperthreads per core. 
The -M option sums all interrupt vectors in /proc/interrupts that match the given tag. In this case p5p1 and p5p2 which 
are the two legs of an MLAG bond. The interrupts and l4 hashing of the flows gives a striping effect. We can also see that 
this system has both of these interfaces attached to socket 0, and that numa local irq pinning is in effect. 

### Softirq network 
![softirq interrupts][softirq_interrupts]

Again on the same machine. This time we're looking at softirq stats. In this case we can see that there's a bit of
softnet squeeze happening, but at a very low level. This color scale runs from dark blue through green to yellow and white. 

## Color Scales 

Sometimes you're interested in the top range, sometimes in the bottom. There are several color scales that can be used. 
![color scales][color_scales]

And some examples of looking at cpu, network and softirq in each. 

### default, bgy
![bgy heatmap][bgy_heatmap]

The default. Colors range from blue at the bottom up through green and yellow to white. 

### red temperature 
![red temperature][red_heatmap]

This one looks a bit halloween but is useful for folks with missing color receptors. 

### rainbow/acid 
![rainbow heatmap][rbw_heatmap]

This accentuates the bottom of the range. It runs from red (lowest energy/longest wavelength) up through 
orange, green, blue to purple. You memorised the order in primary school. The time squeeze shows up nicely
as does the scattered cpu usage on socket 1. 

### mono 
![monochrome heatmap][mono_heatmap]

Useful late at night. 

[simple_cpu]:https://github.com/andyphillips/irq_heatmap/raw/master/images/cpu_simple.png
[network_interrupts]:https://github.com/andyphillips/irq_heatmap/raw/master/images/network_interrupts.png
[softnet_interrupts]:https://github.com/andyphillips/irq_heatmap/raw/master/images/softnet_interrupts.png
[color_scales]:https://github.com/andyphillips/irq_heatmap/raw/master/images/color_scales.png
[bgy_heatmap]:https://github.com/andyphillips/irq_heatmap/raw/master/images/bgy_heatmap.png
[red_heatmap]:https://github.com/andyphillips/irq_heatmap/raw/master/images/red_heatmap.png
[rbw_heatmap]:https://github.com/andyphillips/irq_heatmap/raw/master/images/rbw_heatmap.png
[mono_heatmap]:https://github.com/andyphillips/irq_heatmap/raw/master/images/mono_heatmap.png

## Lazy Documentation

Run irq_heatmap without arguments or with -h to get help. 

usage
./irq_heatmap -C | -S <label> | -I <label> [-i interval] [-t duration]

 interval, duration in seconds. interval default is 1, duration is unlimited

 -C <label> Show cpu time for user,nice,sys,idle,wio,irq,softirq. See note below. The label 'all' will show the sum of user,nice,sys,wio,irq,softirq

 -S <label> Show the SOFTIRQ vector corresponding with that label, e.g. SCHED, NET_RX

 -I <label> Show the IRQ activity for that vector from /proc/interrupts e.g. 75, NMI

 -M <string> Sum the IRQ activity across all vectors that match this terminal string e.g. p5p1-TxRx

 -P <string> Show the activity in the softnet_stats by column: packets, dropped, squeeze

 -Z <string> Choose a color scale: bgy (blue green yellow, default), red (red temperature scale for loren acton), rbw (rainbow, long to short wavelengths)

Version 1.200000, Limits: max_metrics=16, max_cpus=1024, clock tick ms=10

CPU time. This is taken from the jiffies from /proc/stat. Its then scaled up to milliseconds using _SC_CLK_TCK.
	   This means that 100% cpu is 1000ms per second. This displays as the number 'a'

Scale is log2. So '9' is a delta of 2^9 (or 1<<9) per interval

## Building

### Redhat 

Ensure you have the kernel-devel and numactl packages installed

    # yum install kernel-devel numactl 

Then run make to build the binary, or 'make rpm' to build an rpm 
   
### Ubuntu 

Ensure you have the libnuma-dev and libnuma1 packages installed 
    
    # apt install libnuma1 libnuma-dev 
    
Then run make. 
    
