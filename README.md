# toy-pmp
A demo for poor man's profiler
## scripts
### 系统态通用
#### 每秒钟采样100次，产生off-cpu火焰图，0xfffffff栈以上的空白处是用户态代码
for i in `seq 1 100`; do cat /proc/$pid/stack | sed 's/+.*//;' | tac | awk '{printf("%s;",$2)}END{printf("\n")}'; usleep 10000; done | sort | uniq -c | awk '{print $2, $1}' | ./flamegraph.pl > store.svg
### Java用户态
#### jstack每10ms采样一次，重复100次，过滤RUNNABLE线程，去掉没有有效堆栈的线程
for i in `seq 0 100`; do jstack $pid; usleep 10000; done | grep -vE '\- locked|\- waiting on' | awk '/tid=/{if (x)print x; x="";}{x=(!x)?$0:x";"$0}END{if(x)print x}' | sed 's/(//g;s/)//g;s/@[^;]*;/;/g;s/;[[:space:]]*at /;/g;' | awk '{print $0"JAVA"}' | grep 'tid=' | grep 'java.lang.Thread.State: RUNNABLE' | awk -F';' '{for(i=NF;i>=3;i--) printf "%s%s", $i, (i==3?";\n":";")}' | sort | uniq -c | grep -v ' JAVA;$' | awk '{print $2,$1}' | ./flamegraph.pl > perf.svg

### c++用户态
#### 使用用户态pmp，只追踪主线程，每10毫秒采样一次，重复100次
./pmp --pid=$pid --attach=0 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
#### 使用用户态pmp，同步追踪所有线程（对性能影响最大，线程数越多越明显），每10ms采样一次，重复100次
./pmp --pid=$pid --attach=1 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
#### 使用用户态pmp，异步追踪所有线程，每10ms采样一次，重复100次
./pmp --pid=$pid --attach=2 --interval=10 --sample_times=100 | ./flamegraph.pl > redis.svg
