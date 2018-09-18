#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "irq_numa.h"

/* globals */

#define PROC_CPU  "/proc/stat"
#define PROC_SOFTIRQ "/proc/softirqs"
#define PROC_INTERRUPTS "/proc/interrupts"
#define PROC_SOFTNET_STATS "/proc/net/softnet_stat"

#define N_SOFTIRQ_VECTORS 10

#define C_START "[48;5;"
#define C_END   "m"
#define C_RESET "[0m"

#define VERSION 1.1

// this is a high contrast 16 colour scale based on an old astrophysics false colour scheme from my childhood. 
#define max_colors 16
char *colors[] = { "17", "19", "20", "32", "37", "40", "46", "82", "156", "226", "214", "202", "213", "201", "196", "15"};
     
#define MAX_METRICS 8
#define MAX_LABEL   64
#define MAX_CPUS (MAX_SOCKETS*MAX_THREADS*MAX_CORES)  

#define TYPE_CPU 0
#define TYPE_IRQ 1
#define TYPE_SOFTIRQ 2
#define TYPE_IRQSUM 3
#define TYPE_SOFTNET_PACKETS 4

struct line_struct {
     int cursor;
     char buffer[4096];
};

#define LINE_METRIC 0
#define LINE_SOCKET 1
#define LINE_THREAD 2
#define LINE_CPUID1 3
#define LINE_CPUID2 4
#define LINE_COUNT  5

struct header_struct {
     struct line_struct line[LINE_COUNT];
} header;

struct metrics_struct {
     int type;
     char label[MAX_LABEL];
     int label_length;
     int index;   // used for cpu. which column in /proc/stat  
     unsigned long int previous[MAX_CPUS];
     unsigned long int current[MAX_CPUS];
} metrics [MAX_METRICS];

int metric_count;

void usage(char *argv[]) 
{
     printf("usage: %s -C | -S <label> | -I <label> [-i interval] [-t duration]\n",argv[0]);
     printf("usage: interval, duration in seconds. interval default is 1, duration is unlimited\n");
     printf("usage: -C <label> Show cpu time for user,nice,sys,idle,wio,irq,softirq (in jiffies from /proc/stat).\n");
     printf("                  the label 'all' will show the sum of user,nice,sys,wio,irq,softirq\n");
     printf("usage: -S <label> Show the SOFTIRQ vector corresponding with that label, e.g. SCHED, NET_RX\n");
     printf("usage: -I <label> Show the IRQ activity for that vector from /proc/interrupts e.g. 75, NMI\n");
     printf("usage: -M <string> Sum the IRQ activity across all vectors that match this terminal string e.g. p5p1-TxRx\n");
     printf("usage: -P <string> Show the activity in the softnet_stats by column: packets, dropped, squeeze\n");
     printf("Version %f, Limits: max_metrics=%d, max_cpus=%d\n",VERSION, MAX_METRICS, MAX_CPUS);
     exit(0);
}

void error()
{
     printf("error: message was %s\n",strerror(errno));
     exit(1);
}

// this is how we convert the delta to a displayed value. In this case we're taking the power of 2 via bitshift
// it would be possible to have other scale factors. 
unsigned int power2(unsigned long int delta)
{
     int i=0;
     while (delta >= 1) {
	  delta = delta >> 1;
	  i++;
     }
     return i;
}

int get_procstat_column(char *name, char *argv[])
{
     int len = strlen(name);
     
     if (strncmp(name,"all",len)==0) return 0;
     if (strncmp(name,"user",len)==0) return 1;
     if (strncmp(name,"nice",len)==0) return 2;
     if (strncmp(name,"sys",len)==0) return 3;
     if (strncmp(name,"idle",len)==0) return 4;
     if (strncmp(name,"wio",len)==0) return 5;
     if (strncmp(name,"irq",len)==0) return 6;
     if (strncmp(name,"softirq",len)==0) return 7;
     
     usage(argv);
     return -1; // keep gcc happy 
}

int get_procsoftnet_column(char *name, char *argv[])
{
     int len = strlen(name);
     
     if (strncmp(name,"packets",len)==0) return 0;
     if (strncmp(name,"dropped",len)==0) return 1;
     if (strncmp(name,"squeeze",len)==0) return 2;
     if (strncmp(name,"collision",len)==0) return 3;
     if (strncmp(name,"recv_rps",len)==0) return 4;
     if (strncmp(name,"flow_limit",len)==0) return 5;
     
     usage(argv);
     return -1; // keep gcc happy 
}

void gather_softnet_metrics(struct metrics_struct *m)
{
     FILE *fp;
     char line[1024], *endptr;
     int cpu_count = topology.number_of_cpus;
     int c;
     unsigned long int datum;

     if ((fp = fopen(PROC_SOFTNET_STATS,"r")) == NULL) return;
     
     // line format is one line per cpu. Similar to cpu stats
     // Columns. Total packets processed, packets dropped, timesqueezed, cpu_collision, recv_rps, flow_limit
     //          We look for 'packets', 'dropped', 'squeeze'
     for (c=0;c<cpu_count;c++) {
	  if (fgets(line,1024,fp) == NULL) return;
	  endptr=line-1; // first column has no space. 
	  switch(m->index) {
	  case 5: // column 6 - flow_limit
	       datum  = strtoul(endptr+1,&endptr,16);	       
	  case 4: // column 5 - recv_rps
	       datum  = strtoul(endptr+1,&endptr,16);	       
	  case 3: // column 4 - collision
	       datum  = strtoul(endptr+1,&endptr,16);	       
	  case 2: // column 3 - squeeze
	       datum  = strtoul(endptr+1,&endptr,16);	       
	  case 1: // column 2 - dropped
	       datum  = strtoul(endptr+1,&endptr,16);
	  case 0: // column 1 - packets
	       datum  = strtoul(endptr+1,&endptr,16);
	  }
//	  printf("parsed from softnet_stat cpu %d, column %d, value %lu\n",c,m->index,datum);
	  m->current[c]=datum;
     }
     fclose(fp);
     return;
}

void gather_cpu_metrics(struct metrics_struct *m)
{
     FILE *fp;
     char line[1024], *endptr;
     int cpu_count = topology.number_of_cpus;
     int c;
     unsigned long int cpuid,datum;
     
     if ((fp = fopen(PROC_CPU,"r")) == NULL) return;

     // jump the total line
     fgets(line,1024,fp);
     
     // line format as of linux 2.6.32
     // cpu   user   nice sys   idle     wio  irq softirq steal  guest 
     // cpu9 5429580 316 345401 72678053 1794 0   239     0      0
     // cpu10 6201797 236 987328 71237863 3546 0 282 0 0
     // 
     // Count is in jiffies. Need to scale that into something more reasonable. We have the clock tick value in topology
     for (c=0;c<cpu_count;c++) {
	  unsigned long int all=0; // all except idle
	  
	  if (fgets(line,1024,fp) == NULL) return;
	  cpuid = strtoul(line+3,&endptr,10);
	  
	  switch(m->index) {
	  case 0:
	  case 7: // column 7 - softirq
	       datum  = strtoul(endptr+1,&endptr,10);
	       all+=datum;
	  case 6: // column 6 - irq
	       datum  = strtoul(endptr+1,&endptr,10);	       
	       all+=datum;
	  case 5: // column 5 - wio
	       datum  = strtoul(endptr+1,&endptr,10);	       
	       all+=datum;
	  case 4: // column 4 - idle 
	       datum  = strtoul(endptr+1,&endptr,10);	       
	  case 3: // column 3 - sys 
	       datum  = strtoul(endptr+1,&endptr,10);	       
	       all+=datum;
	  case 2: // column 2 - nice 
	       datum  = strtoul(endptr+1,&endptr,10);
	       all+=datum;
	  case 1: // column 1 - user
	       datum  = strtoul(endptr+1,&endptr,10);
	       all+=datum;
	  }
	  if (m->index == 0) m->current[cpuid]= all; else m->current[cpuid]=datum;
     }
     fclose(fp);
     return;
}

void gather_tagged_table_metrics(struct metrics_struct *m, char *table_name) 
{
     FILE *fp;
     char line[1024], *endptr, *startptr;
     int cpu_count = topology.number_of_cpus;
     int c,k=0,found_flag=0;
     unsigned long int thing;
     
     if ((fp = fopen(table_name,"r")) == NULL) return;
     // find the label we're after. 
     while (fgets(line,1024,fp) != NULL) {
	  k=0;
	  while (line[k]==' ') k++;
	  if (strncmp(&line[k],m->label,m->label_length) == 0) {
	       found_flag ++;
	       break;
	  }
     }
     if (!found_flag) {
	  fprintf(stderr,"Could not find label %s in file %s\n",m->label,table_name);
	  exit(-1);
     }
     // should have the correct line now.
     startptr = &line[ (k+m->label_length+2) ]; // there's a ':' 
     for (c=0;c<cpu_count;c++) {
	  thing = strtoul(startptr,&endptr,10); 
//	  printf("parsed the following in %s: cpu %d, thing (%s) %lu\n",table_name,c,m->label,thing);
	  m->current[c]=thing;
	  startptr = endptr+1;
     }
     fclose(fp);
     return;
}

// this differs from the above in that we're looking at the last field and summing everything that matches. 
void gather_irqsum_metrics(struct metrics_struct *m)
{
     FILE *fp;
     char line[4096], *endptr, *startptr, *cp;
     int cpu_count = topology.number_of_cpus;
     int c,found_flag=0;
     unsigned long int thing;

     // reset the current counter
     for (c=0;c<cpu_count;c++) {
	  m->current[c]=0;
     }
     
     if ((fp = fopen(PROC_INTERRUPTS,"r")) == NULL) return;
     // find the label we're after. 
     while (fgets(line,4096,fp) != NULL) {
	  // get the last ' ' in the string. 
	  cp = strrchr(line,' ');
	  if (cp == NULL) return;
	  cp++;
	  if (strncmp(cp,m->label,m->label_length) == 0) {
	       found_flag ++;
//	       printf("found %s in |%s|\n",m->label,line);
	       // skip past the label: 
	       cp = strchr(line,':');
	       if (cp == NULL) return;
	       
	       startptr = cp+1;
	       for (c=0;c<cpu_count;c++) {
		    thing = strtoul(startptr,&endptr,10); 
		    //	  printf("parsed the following in %s: cpu %d, thing (%s) %lu\n",table_name,c,m->label,thing);
		    m->current[c]+=thing;
		    startptr = endptr+1;
	       }
	  } // a matched thing. 
     }
     if (found_flag==0) {
	  fprintf(stderr,"Could not find label %s in file %s\n",m->label,PROC_INTERRUPTS);
	  exit(-1);
     }
     return;
}

void gather_irq_metrics(struct metrics_struct *m)
{
     // There are two possibilities. The start label/vector or the description. Not all lines have descriptions, so we go with the vector.
     //   77:   81913889          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0          0   PCI-MSI-edge      eth0
     //  NMI:     137218     111219      82276      75520      74516      71308     106739      96861      78649      73834      94933      85022      75767      71584      69532      67931      84265      79965      72898      69879   Non-maskable interrupts
     return gather_tagged_table_metrics(m,PROC_INTERRUPTS);
}

void gather_softirq_metrics(struct metrics_struct *m)
{
     //                 CPU0       CPU1       CPU2       CPU3       CPU4       CPU5       CPU6       CPU7       CPU8       CPU9       CPU10      CPU11      CPU12      CPU13      CPU14      CPU15      CPU16      CPU17      CPU18      CPU19      CPU20      CPU21      CPU22      CPU23      CPU24      CPU25      CPU26      CPU27      CPU28      CPU29      CPU30      CPU31
     //   NET_RX:   81883188     197652     176139      83872      45232      37908     153370     165532     155666      76019     144362     158300      99478      45608      33239      28539     155949     102458      74487      43986          0          0          0          0          0          0          0          0          0          0          0          0
     // find the label we're after.
     return gather_tagged_table_metrics(m,PROC_SOFTIRQ);
}

// there's a lot to show here and an uncertain amount of space to show it in. 
void init_header()
{
     int i,m,s,t,c,offset;

     memset((void *)&header,0,sizeof(header));

     sprintf(header.line[LINE_METRIC].buffer,"%-10s","Metric");
     sprintf(header.line[LINE_SOCKET].buffer,"%-10s","Socket");
     sprintf(header.line[LINE_THREAD].buffer,"%-10s","Thread");
     sprintf(header.line[LINE_CPUID1].buffer,"%-10s","Cpu");
     sprintf(header.line[LINE_CPUID2].buffer,"%-10s","   ");
     
     offset=10; // offset from start. 
     for (i=0;i<LINE_COUNT;i++) header.line[i].cursor = 10;
     
     for (m=0;m<metric_count;m++) {
	  while (header.line[LINE_METRIC].cursor < offset) header.line[LINE_METRIC].buffer[header.line[LINE_METRIC].cursor++]=' ';
	  sprintf(&header.line[LINE_METRIC].buffer[header.line[LINE_METRIC].cursor],"%s",metrics[m].label);
	  header.line[LINE_METRIC].cursor+=metrics[m].label_length;
	  
	  for (s=0;s<topology.number_of_sockets;s++) {
	       while (header.line[LINE_SOCKET].cursor < offset) header.line[LINE_SOCKET].buffer[header.line[LINE_SOCKET].cursor++]=' ';
	       sprintf(&header.line[LINE_SOCKET].buffer[header.line[LINE_SOCKET].cursor],"%1.1d",s);
	       header.line[LINE_SOCKET].cursor++;
	       
	       for (t=0;t<topology.map[s].thread_count;t++) {
		    while (header.line[LINE_THREAD].cursor < offset) header.line[LINE_THREAD].buffer[header.line[LINE_THREAD].cursor++]=' ';
		    sprintf(&header.line[LINE_THREAD].buffer[header.line[LINE_THREAD].cursor],"%1.1d",t);
		    header.line[LINE_THREAD].cursor++;
		    
		    for (c=0;c<topology.map[s].threads[t].core_count;c++){
			 int cpuid = topology.map[s].threads[t].cores[c].cpu_id;
			 
			 while (header.line[LINE_CPUID1].cursor < offset) header.line[LINE_CPUID1].buffer[header.line[LINE_CPUID1].cursor++]=' ';
			 sprintf(&header.line[LINE_CPUID1].buffer[header.line[LINE_CPUID1].cursor],"%1.1d",(cpuid/10));
			 header.line[LINE_CPUID1].cursor++;
			 
			 while (header.line[LINE_CPUID2].cursor < offset) header.line[LINE_CPUID2].buffer[header.line[LINE_CPUID2].cursor++]=' ';
			 sprintf(&header.line[LINE_CPUID2].buffer[header.line[LINE_CPUID2].cursor],"%1.1d",(cpuid%10));
			 header.line[LINE_CPUID2].cursor++;
			 
			 offset++;
		    }
		    if (t<topology.map[s].thread_count-1) offset++; // '|'
	       }
	       if (s<topology.number_of_sockets-1) offset++; // ' ' 
	  }
	  offset+=2;
     }
     return;
}

void print_header()
{
     int i;
     for (i=0;i<LINE_COUNT;i++) fprintf(stdout,"%s\n",header.line[i].buffer);
     return;
}

// iterate through the metrics and system topology and then display the result as a heatmap. 
void display_metric_heatmap(time_t now, int interval_count)
{
     int m,s,t,c;
     struct tm *tmp;
     char timestamp[256];
     
     tmp = localtime(&now);
     strftime(timestamp,sizeof(timestamp),"%H:%M:%S",tmp);
     fprintf(stdout,"%8s: ",timestamp); // 10 characters
     for (m=0;m<metric_count;m++) {
	  for (s=0;s<topology.number_of_sockets;s++) {
	       for (t=0;t<topology.map[s].thread_count;t++) {
		    for (c=0;c<topology.map[s].threads[t].core_count;c++){
			 int cpuid = topology.map[s].threads[t].cores[c].cpu_id;
			 int delta = metrics[m].current[cpuid] - metrics[m].previous[cpuid];
			 int value;
			 
			 value = power2(delta);
			 if (value >= max_colors) value=max_colors - 1;
			 fprintf(stdout,"%s%s%s%x",C_START,colors[value],C_END,value);
		    }
		    if (t<topology.map[s].thread_count-1) fprintf(stdout,"%s|",C_RESET);
	       }
	       if (s<topology.number_of_sockets-1) fprintf(stdout,"%s ",C_RESET);
	  }
	  fprintf(stdout,"%s  ",C_RESET);
     }
     printf("\n");
     return;
}
// flip the current and previous buffers. 
void advance_metrics()
{
     int m;
     
     for (m=0;m<metric_count;m++) {
	  memcpy(metrics[m].previous,metrics[m].current,sizeof(metrics[m].previous));
     }
     return;
}

int main(int argc,char *argv[])
{
     extern char *optarg;
     extern int optind;
     
     int opt, interval_count;
     
     time_t now,start,end;
     
     int interval = 1;
     int timespan = -1;
          
     const char *optstring="C:I:S:M:P:t:i:h";

     irqnuma_init_topology();
     
     metric_count = 0;
     memset((void *)metrics,0,sizeof(struct metrics_struct)*MAX_METRICS);
     
     while ((opt = getopt(argc, argv, optstring))!= -1) {
	  switch (opt) {
	  case 'C':
	       metrics[metric_count].type=TYPE_CPU;
	       sprintf(metrics[metric_count].label,"cpu ");
	       strncat(metrics[metric_count].label,optarg,MAX_LABEL-5);
	       metrics[metric_count].label_length = strlen(metrics[metric_count].label);
	       metrics[metric_count].index = get_procstat_column(optarg,argv);
	       metric_count ++;
	       break;
	  case 'I':
	       metrics[metric_count].type=TYPE_IRQ;
	       strncpy(metrics[metric_count].label,optarg,MAX_LABEL-1);
	       metrics[metric_count].label_length = strlen(metrics[metric_count].label);
	       metric_count ++;
	       break;
	  case 'S':
	       metrics[metric_count].type=TYPE_SOFTIRQ;
	       strncpy(metrics[metric_count].label,optarg,MAX_LABEL-1);
	       metrics[metric_count].label_length = strlen(metrics[metric_count].label);
	       metric_count ++;
	       break;
	  case 'M':
	       metrics[metric_count].type=TYPE_IRQSUM;
	       strncpy(metrics[metric_count].label,optarg,MAX_LABEL-1);
	       metrics[metric_count].label_length = strlen(metrics[metric_count].label);
	       metric_count ++;
	       break;	       
	  case 'P':
	       metrics[metric_count].type=TYPE_SOFTNET_PACKETS;
	       sprintf(metrics[metric_count].label,"softnet ");
	       strncat(metrics[metric_count].label,optarg,MAX_LABEL-9);
	       metrics[metric_count].index = get_procsoftnet_column(optarg,argv);
	       metrics[metric_count].label_length = strlen(metrics[metric_count].label);
	       metric_count ++;
	       break;
	  case 't':
	       timespan = atoi(optarg);
	       break;
	  case 'i':
	       interval = atoi(optarg);
	       break;
	  case 'h':
	  default:
	       usage(argv);
	  }
	  
	  if (metric_count >= MAX_METRICS) {
	       fprintf(stderr,"I can only handle %d metrics, please reduce number of arguments or increase MAX_METRICS\n",MAX_METRICS);
	       usage(argv);
	  }
     }

     if (metric_count == 0) usage(argv);

     // create the header 
     init_header(metric_count);
     
     // start the loop. 
     start = time(NULL);
     now  = time(NULL);
     if (timespan > -1) {
	  end = start + timespan; // seconds
     } else {
	  end = start + 100000000 ; // many
     }
     print_header();
     interval_count = 0;
     while (now < end) {
	  int m;
	  for (m=0;m<metric_count;m++) {
	       switch (metrics[m].type) {
	       case TYPE_CPU:
		    gather_cpu_metrics(&metrics[m]);
		    break;
	       case TYPE_IRQ:
		    gather_irq_metrics(&metrics[m]);
		    break;
	       case TYPE_SOFTIRQ:
		    gather_softirq_metrics(&metrics[m]);
		    break;
	       case TYPE_IRQSUM:
		    gather_irqsum_metrics(&metrics[m]);
		    break;
	       case TYPE_SOFTNET_PACKETS:
		    gather_softnet_metrics(&metrics[m]);
		    break;
	       default:
		    fprintf(stderr,"unknown metric type, internal consistency error\n");
		    exit(-1);
	       }
	  }
	  display_metric_heatmap(now,interval_count);
	  advance_metrics();
	  sleep(interval);
	  interval_count ++;
	  if ((interval_count % 60)==0) print_header();
	  now  = time(NULL);
     }
     return 0;
}
