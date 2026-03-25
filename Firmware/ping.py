import socket
import time
from statistics import mean

def percentile_distribution(data, parts=10):
    data = sorted(data)
    n = len(data)
    result = []

    for i in range(parts):
        start_idx = int(n * i / parts)
        end_idx = int(n * (i + 1) / parts) - 1

        start_val = data[start_idx]
        end_val = data[end_idx if end_idx >= start_idx else start_idx]

        result.append((i * 10, (i + 1) * 10, start_val, end_val))

    return result

def recv_until_crlf(sock):
    buffer = b""

    while not buffer.endswith(b"ping ok\r\n"):
        byte = sock.recv(1)
        if not byte:
            raise ConnectionError("Соединение закрыто")

        buffer += byte

    return buffer[:-2]

def ping_tcp_single_connection(ip, port, message="$PING\n", count=3000, timeout=5):
    times = []
    success = 0
    failed = 0

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)

    try:
        sock.connect((ip, port))
    except Exception as e:
        print(f"Ошибка подключения: {e}")
        return

    for i in range(1, count + 1):

        try:
            start = time.perf_counter()

            sock.sendall(message.encode())
            recv_until_crlf(sock)

            end = time.perf_counter()
            latency_ms = (end - start) * 1000

            times.append(latency_ms)
            success += 1

            print(f"[{i}/{count}] time={latency_ms:.2f} ms")

        except socket.timeout:
            failed += 1
            print(f"[{i}/{count}] TIMEOUT")

        except Exception as e:
            failed += 1
            print(f"[{i}/{count}] ERROR: {e}")
            break  # если соединение умерло — дальше нет смысла

        
        time.sleep(0.030)


    sock.close()

    print("\n--- Итоговая статистика ---")
    print(f"Успешно: {success}")
    print(f"Ошибок: {failed}")

    if times:
        print(f"Минимум: {min(times):.2f} ms")
        print(f"Максимум: {max(times):.2f} ms")
        print(f"Среднее:  {mean(times):.2f} ms")

        top20 = sorted(times, reverse=True)[:20]

        for v in top20:
            print(f"max {v:.2f} ms")

        print("\n--- Распределение (перцентили) ---")
        dist = percentile_distribution(times)

        for start_p, end_p, vmin, vmax in dist:
            print(f"{start_p:02d}-{end_p:02d}%: {vmin:.2f} - {vmax:.2f} ms")

    else:
        print("Нет успешных измерений")

# пример использования
ping_tcp_single_connection("192.168.4.1", 23)