#include <stdio.h>
#include <numa.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

// numalib does not understand hyperthreads, so we extend it here. 

// hierarchy = sockets have hyperthreads have cores. 
// The last two are inverted from the normal sense of thinking about it, but 
// it helps as that's the way we display it - following numactl --hardware 
#define MAX_SOCKETS 4
#define MAX_THREADS 4
#define MAX_CORES 64
 
struct core_desc_struct {
     int configured; 
     int core_id; // this can be strange
     int cpu_id;  // from the pov of linux
};

struct ht_desc_struct {
     int configured;
     int core_count;
     struct core_desc_struct cores[MAX_CORES];
};

struct socket_struct {
     int configured;
     int thread_count;
     struct ht_desc_struct threads[MAX_THREADS];
};

struct numa_topology {
     int number_of_sockets; 
     int number_of_cpus; 
     int clock_tick_ms;
     struct socket_struct map[MAX_SOCKETS];
}; 

extern struct numa_topology topology;

/* irq_numa.c */
int irqnuma_num_hyperthreads(void);
int irqnuma_sysfs_integer(char *path);
struct bitmask *irqnuma_sysfs_cpustring(char *path);
int irqnuma_get_packageid(int cpuid);
int irqnuma_get_coreid(int cpuid);
int irqnuma_get_threadid(int cpuid);
void irqnuma_add_cpu_to_topology(int cpuid, int socket, int coreid, int thread_id);
int irqnuma_get_clocktick_ms(void);
void irqnuma_init_topology(void);
void irqnuma_dump_topology(void);
