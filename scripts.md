# 每秒钟采样100次，产生off-cpu火焰图，空白处是用户态代码
for i in `seq 1 100`; do cat /proc/$pid/stack | sed 's/+.*//;' | tac | awk '{printf("%s;",$2)}END{printf("\n")}'; usleep 10000; done | sort | uniq -c | awk '{print $2, $1}' | ./flamegraph.pl > store.svg
