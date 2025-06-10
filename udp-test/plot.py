import matplotlib.pyplot as plt

with open('udp-test.log', 'r') as f:
    lines = f.readlines()

udp_times = []

for line in lines:
    parts = line.split()
    time = float(parts[0])
    udp_times.append(time)

with open('tcp-test.log', 'r') as f:
    lines = f.readlines()

tcp_times = []

for line in lines:
    parts = line.split()
    time = float(parts[0])
    tcp_times.append(time)

plt.figure(figsize=(10, 5))
plt.hist(udp_times, bins=50, alpha=0.5, label='UDP Latency', color='blue')
plt.hist(tcp_times, bins=50, alpha=0.5, label='TCP Latency', color='red')
plt.xlabel('Frequency')
plt.ylabel('Times')
plt.title('UDP vs TCP Latency Over Time')
plt.legend()
plt.grid()
plt.savefig('latency_comparison.png')
plt.show()
