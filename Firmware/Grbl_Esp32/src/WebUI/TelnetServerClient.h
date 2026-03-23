#pragma once 

#include <WiFi.h>
#include <esp_wifi.h>
#include "freertos/ringbuf.h"

class cnc_wifi_controller
{
public:
    explicit cnc_wifi_controller(WiFiClient& wifi_client)
        : client(wifi_client),
          ring_buffer(nullptr),
          rx_buffer_len(0),
          queued_items(0),
          blocking_busy(false),
          blocking_done(false),
          blocking_result(false)
    {
        rx_buffer[0] = '\0';
        blocking_cmd[0] = '\0';
    }

    ~cnc_wifi_controller()
    {
        if (ring_buffer != nullptr)
        {
            vRingbufferDelete(ring_buffer);
            ring_buffer = nullptr;
        }
    }

    bool begin(size_t ring_buffer_size = 4096)
    {
        esp_wifi_set_ps(WIFI_PS_NONE);

        ring_buffer = xRingbufferCreate(ring_buffer_size, RINGBUF_TYPE_NOSPLIT);
        return ring_buffer != nullptr;
    }

    void update()
    {
        if (!is_connected() || ring_buffer == nullptr)
        {
            return;
        }

        read_client_to_rx_buffer();
        parse_rx_buffer();
        process_motion();
    }

    bool is_connected() const
    {
        return client && client.connected();
    }

private:
    enum cmd_type
    {
        cmd_empty,
        cmd_buffered,
        cmd_blocking,
        cmd_invalid
    };

    WiFiClient& client;
    RingbufHandle_t ring_buffer;

    static constexpr size_t rx_buffer_capacity = 2048;
    char rx_buffer[rx_buffer_capacity];
    size_t rx_buffer_len;

    size_t queued_items;

    bool blocking_busy;
    bool blocking_done;
    bool blocking_result;
    char blocking_cmd[128];

private:
    void read_client_to_rx_buffer()
    {
        int available_bytes = client.available();
        if (available_bytes <= 0)
        {
            return;
        }

        size_t free_space = rx_buffer_capacity - 1 - rx_buffer_len;

        if (free_space == 0)
        {
            send_line("error:rx_overflow");
            reset_rx_buffer();
            flush_client_input();
            return;
        }

        size_t bytes_to_read = (size_t)available_bytes;
        if (bytes_to_read > free_space)
        {
            bytes_to_read = free_space;
        }

        size_t bytes_read = client.readBytes(rx_buffer + rx_buffer_len, bytes_to_read);
        rx_buffer_len += bytes_read;
        rx_buffer[rx_buffer_len] = '\0';

        if (bytes_read < (size_t)available_bytes)
        {
            send_line("error:rx_overflow");
            flush_client_input();
        }
    }

    void parse_rx_buffer()
    {
        size_t line_start = 0;

        for (size_t i = 0; i < rx_buffer_len; ++i)
        {
            if (rx_buffer[i] == '\n' || rx_buffer[i] == '\r')
            {
                rx_buffer[i] = '\0';

                if (i > line_start)
                {
                    handle_line(rx_buffer + line_start);
                }

                line_start = i + 1;
            }
        }

        if (line_start > 0)
        {
            size_t remaining = rx_buffer_len - line_start;
            memmove(rx_buffer, rx_buffer + line_start, remaining);
            rx_buffer_len = remaining;
            rx_buffer[rx_buffer_len] = '\0';
        }

        if (rx_buffer_len == rx_buffer_capacity - 1)
        {
            send_line("error:line_too_long");
            reset_rx_buffer();
        }
    }

    void handle_line(const char* raw_line)
    {
        char cleaned_line[128];
        preprocess_gcode_line(raw_line, cleaned_line, sizeof(cleaned_line));

        cmd_type type = classify_command(cleaned_line);

        switch (type)
        {
            case cmd_empty:
                send_line("ok");
                return;

            case cmd_invalid:
                send_line("error:bad_command");
                return;

            case cmd_blocking:
                handle_blocking_command(cleaned_line);
                return;

            case cmd_buffered:
                handle_buffered_command(cleaned_line);
                return;
        }
    }

    void handle_blocking_command(const char* line)
    {
        if (blocking_busy)
        {
            send_line("error:blocking_busy");
            return;
        }

        if (queued_items > 0)
        {
            send_line("error:planner_not_empty");
            return;
        }

        copy_string(blocking_cmd, sizeof(blocking_cmd), line);
        blocking_busy = true;
        blocking_done = false;
        blocking_result = false;
    }

    void handle_buffered_command(const char* line)
    {
        size_t len = strlen(line) + 1;

        if (xRingbufferSend(ring_buffer, line, len, 0) == pdTRUE)
        {
            ++queued_items;
            send_line("ok");
        }
        else
        {
            send_line("error:queue_full");
        }
    }

    void process_motion()
    {
        if (blocking_busy)
        {
            process_blocking_motion();
            return;
        }

        process_buffered_motion();
    }

    void process_blocking_motion()
    {
        if (!blocking_done)
        {
            if (execute_blocking_step(blocking_cmd))
            {
                blocking_done = true;
            }
        }

        if (blocking_done)
        {
            send_line(blocking_result ? "ok" : "error:blocking_failed");
            blocking_busy = false;
            blocking_done = false;
        }
    }

    void process_buffered_motion()
    {
        size_t item_size = 0;
        char* item = (char*)xRingbufferReceive(ring_buffer, &item_size, 0);

        if (item == nullptr)
        {
            return;
        }

        if (queued_items > 0)
        {
            --queued_items;
        }

        bool ok = execute_buffered(item);
        vRingbufferReturnItem(ring_buffer, item);

        if (!ok)
        {
            send_line("alarm:exec_failed");
        }
    }

    cmd_type classify_command(const char* line) const
    {
        const char* s = skip_spaces(line);

        if (*s == '\0')
        {
            return cmd_empty;
        }

        if (is_blocking_command(s))
        {
            return cmd_blocking;
        }

        if (is_buffered_command(s))
        {
            return cmd_buffered;
        }

        return cmd_invalid;
    }

    bool is_blocking_command(const char* line) const
    {
        if (*line == '$')
        {
            return true;
        }

        if (contains_gcode_word(line, "G38")) return true;
        if (contains_gcode_word(line, "G4"))  return true;
        if (contains_gcode_word(line, "M6"))  return true;
        if (contains_word(line, "$H"))        return true;

        return false;
    }

    bool is_buffered_command(const char* line) const
    {
        if (contains_gcode_word(line, "G0"))  return true;
        if (contains_gcode_word(line, "G1"))  return true;
        if (contains_gcode_word(line, "G2"))  return true;
        if (contains_gcode_word(line, "G3"))  return true;
        if (contains_gcode_word(line, "M3"))  return true;
        if (contains_gcode_word(line, "M5"))  return true;
        if (contains_gcode_word(line, "G90")) return true;
        if (contains_gcode_word(line, "G91")) return true;
        if (contains_standalone_letter_word(line, 'F')) return true;
        if (contains_standalone_letter_word(line, 'S')) return true;

        return false;
    }

    bool execute_buffered(const char* line)
    {
        Serial.print("[buffered] ");
        Serial.println(line);

        delay(2);
        return true;
    }

    bool execute_blocking_step(const char* line)
    {
        static uint32_t start_ms = 0;
        static bool active = false;

        if (!active)
        {
            active = true;
            start_ms = millis();

            Serial.print("[blocking_start] ");
            Serial.println(line);
        }

        if (millis() - start_ms >= 1500)
        {
            Serial.print("[blocking_done] ");
            Serial.println(line);

            blocking_result = true;
            active = false;
            return true;
        }

        return false;
    }

    void flush_client_input()
    {
        uint8_t tmp[128];

        while (client.connected() && client.available() > 0)
        {
            size_t chunk = (size_t)client.available();
            if (chunk > sizeof(tmp))
            {
                chunk = sizeof(tmp);
            }
            client.read(tmp, chunk);
        }
    }

    void send_line(const char* msg)
    {
        if (!is_connected())
        {
            return;
        }

        client.print(msg);
        client.print('\n');
    }

    void reset_rx_buffer()
    {
        rx_buffer_len = 0;
        rx_buffer[0] = '\0';
    }

    static void preprocess_gcode_line(const char* src, char* dst, size_t dst_size)
    {
        if (dst_size == 0)
        {
            return;
        }

        char tmp[128];
        strip_comments(src, tmp, sizeof(tmp));
        normalize_spaces(tmp, dst, dst_size);
    }

    static void strip_comments(const char* src, char* dst, size_t dst_size)
    {
        size_t j = 0;
        bool in_paren = false;

        for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; ++i)
        {
            char c = src[i];

            if (c == ';' && !in_paren)
            {
                break;
            }

            if (c == '(')
            {
                in_paren = true;
                continue;
            }

            if (c == ')')
            {
                in_paren = false;
                continue;
            }

            if (in_paren)
            {
                continue;
            }

            dst[j++] = c;
        }

        dst[j] = '\0';
    }

    static void normalize_spaces(const char* src, char* dst, size_t dst_size)
    {
        size_t j = 0;
        bool prev_space = false;

        for (size_t i = 0; src[i] != '\0' && j < dst_size - 1; ++i)
        {
            char c = src[i];

            if (c == '\t')
            {
                c = ' ';
            }

            if (c == ' ')
            {
                if (prev_space)
                {
                    continue;
                }
                prev_space = true;
            }
            else
            {
                prev_space = false;
            }

            dst[j++] = c;
        }

        while (j > 0 && dst[j - 1] == ' ')
        {
            --j;
        }

        dst[j] = '\0';
    }

    static const char* skip_spaces(const char* s)
    {
        while (*s == ' ' || *s == '\t')
        {
            ++s;
        }
        return s;
    }

    static void copy_string(char* dst, size_t dst_size, const char* src)
    {
        size_t i = 0;

        for (; i < dst_size - 1 && src[i] != '\0'; ++i)
        {
            dst[i] = src[i];
        }

        dst[i] = '\0';
    }

    static bool is_token_boundary(char c)
    {
        return c == '\0' || c == ' ' || c == '\t';
    }

    static bool contains_word(const char* line, const char* word)
    {
        size_t word_len = strlen(word);

        for (size_t i = 0; line[i] != '\0'; ++i)
        {
            if ((i == 0 || is_token_boundary(line[i - 1])) &&
                strncmp(&line[i], word, word_len) == 0 &&
                is_token_boundary(line[i + word_len]))
            {
                return true;
            }
        }

        return false;
    }

    static bool contains_gcode_word(const char* line, const char* prefix)
    {
        size_t len = strlen(prefix);
        size_t i = 0;

        while (line[i] != '\0')
        {
            while (line[i] == ' ' || line[i] == '\t')
            {
                ++i;
            }

            if (strncmp(&line[i], prefix, len) == 0)
            {
                char next = line[i + len];

                if (next == '\0' || next == ' ' || next == '\t' ||
                    (next >= '0' && next <= '9') ||
                    next == '.' || next == '-')
                {
                    return true;
                }
            }

            while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t')
            {
                ++i;
            }
        }

        return false;
    }

    static bool contains_standalone_letter_word(const char* line, char letter)
    {
        for (size_t i = 0; line[i] != '\0'; ++i)
        {
            if (line[i] == letter)
            {
                if (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')
                {
                    return true;
                }
            }
        }

        return false;
    }
};