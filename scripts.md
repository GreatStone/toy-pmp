# 每秒钟采样100次，产生off-cpu火焰图，0xfffffff栈以上的空白处是用户态代码
for i in `seq 1 100`; do cat /proc/$pid/stack | sed 's/+.*//;' | tac | awk '{printf("%s;",$2)}END{printf("\n")}'; usleep 10000; done | sort | uniq -c | awk '{print $2, $1}' | ./flamegraph.pl > store.svg
# 使用用户态pmp，只追踪主线程，每10毫秒采样一次，重复100次
./pmp --pid=$pid --attach=0 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
# 使用用户态pmp，同步追踪所有线程（对性能影响最大，线程数越多越明显），每10ms采样一次，重复100次
./pmp --pid=$pid --attach=1 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
# 使用用户态pmp，异步追踪所有线程，每10ms采样一次，重复100次
./pmp --pid=$pid --attach=2 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
