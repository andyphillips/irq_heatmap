all: irq_heatmap

irq_heatmap: irq_heatmap.c irq_numa.c irq_numa.h
	gcc -Wall -g -o irq_heatmap irq_heatmap.c irq_numa.c -l numa

irq_numa: irq_numa.h irq_numa.c 
	gcc -Wall -g -DDEBUG -o irq_numa irq_numa.c -l numa

clean: 
	rm -f *.o *~ irq_heatmap irq_numa
	