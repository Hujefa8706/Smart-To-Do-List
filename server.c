/*
 * Todo List - C backend using a linked list and Windows Winsock.
 * Build in VS Code PowerShell:
 *   gcc server.c -o todo_server.exe -lws2_32
 * Run:
 *   .\todo_server.exe
 * Open:
 *   http://localhost:8080
 */
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 8080
#define REQ_SIZE 16384
#define BODY_SIZE 8192

typedef struct Task {
    int id;
    int completed;
    char text[256];
    struct Task *next;
} Task;

static Task *head = NULL;
static int next_id = 1;

static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n' || c == '\r') { dst[j++] = ' '; }
        else if (c >= 32) dst[j++] = (char)c;
    }
    dst[j] = '\0';
}

static void send_response(SOCKET client, const char *status, const char *type, const char *body) {
    char header[512];
    int body_len = (int)strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        status, type, body_len);
    send(client, header, (int)strlen(header), 0);
    if (body_len > 0) send(client, body, body_len, 0);
}

static void send_file(SOCKET client, const char *path, const char *type) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        send_response(client, "404 Not Found", "text/plain; charset=utf-8", "File not found. Keep the public folder beside server.c.\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); send_response(client, "500 Internal Server Error", "text/plain", "Out of memory"); return; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    send_response(client, "200 OK", type, buf);
    free(buf);
}

static int get_int_field(const char *body, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return atoi(p);
}

/* Small JSON string-field reader for this project's simple request bodies. */
static int get_string_field(const char *body, const char *key, char *out, size_t cap) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap) {
        if (*p == '\\' && p[1]) p++;
        if (*p == '\n' || *p == '\r') out[j++] = ' ';
        else out[j++] = *p;
        p++;
    }
    out[j] = '\0';
    return 1;
}

static void tasks_json(char *out, size_t cap) {
    size_t used = 0;
    used += (size_t)snprintf(out + used, cap - used, "{\"tasks\":[");
    Task *cur = head;
    while (cur && used < cap - 128) {
        char safe[512];
        json_escape(cur->text, safe, sizeof(safe));
        used += (size_t)snprintf(out + used, cap - used,
            "%s{\"id\":%d,\"text\":\"%s\",\"completed\":%s}",
            cur == head ? "" : ",", cur->id, safe, cur->completed ? "true" : "false");
        cur = cur->next;
    }
    snprintf(out + used, cap - used, "]}");
}

static void add_task(const char *text) {
    Task *node = (Task *)calloc(1, sizeof(Task));
    if (!node) return;
    node->id = next_id++;
    strncpy(node->text, text, sizeof(node->text) - 1);
    node->completed = 0;
    if (!head) head = node;
    else {
        Task *cur = head;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
}

static void toggle_task(int id) {
    for (Task *cur = head; cur; cur = cur->next)
        if (cur->id == id) { cur->completed = !cur->completed; return; }
}

static void delete_task(int id) {
    Task *cur = head, *prev = NULL;
    while (cur) {
        if (cur->id == id) {
            if (prev) prev->next = cur->next; else head = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

static void handle_client(SOCKET client) {
    char req[REQ_SIZE];
    int total = 0, n;
    memset(req, 0, sizeof(req));
    while (total < (int)sizeof(req) - 1) {
        n = recv(client, req + total, (int)sizeof(req) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        req[total] = '\0';
        char *header_end = strstr(req, "\r\n\r\n");
        if (header_end) {
            int content_length = 0;
            char *cl = strstr(req, "Content-Length:");
            if (cl) content_length = atoi(cl + strlen("Content-Length:"));
            int header_bytes = (int)(header_end + 4 - req);
            if (total >= header_bytes + content_length) break;
        } else if (total > 0 && strstr(req, "\r\n\r\n") == NULL) continue;
    }

    char method[16] = {0}, path[256] = {0};
    sscanf(req, "%15s %255s", method, path);
    char *header_end = strstr(req, "\r\n\r\n");
    char *body = header_end ? header_end + 4 : "";
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client, "204 No Content", "text/plain", "");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/tasks") == 0) {
        char json[BODY_SIZE];
        tasks_json(json, sizeof(json));
        send_response(client, "200 OK", "application/json; charset=utf-8", json);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/add") == 0) {
        char text[256];
        if (get_string_field(body, "text", text, sizeof(text)) && text[0]) {
            add_task(text);
            char json[BODY_SIZE]; tasks_json(json, sizeof(json));
            send_response(client, "200 OK", "application/json; charset=utf-8", json);
        } else send_response(client, "400 Bad Request", "application/json", "{\"error\":\"Task text is required\"}");
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/toggle") == 0) {
        toggle_task(get_int_field(body, "id"));
        char json[BODY_SIZE]; tasks_json(json, sizeof(json));
        send_response(client, "200 OK", "application/json; charset=utf-8", json);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/delete") == 0) {
        delete_task(get_int_field(body, "id"));
        char json[BODY_SIZE]; tasks_json(json, sizeof(json));
        send_response(client, "200 OK", "application/json; charset=utf-8", json);
    } else if (strcmp(method, "GET") == 0 && (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)) {
        send_file(client, "public/index.html", "text/html; charset=utf-8");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/style.css") == 0) {
        send_file(client, "public/style.css", "text/css; charset=utf-8");
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/script.js") == 0) {
        send_file(client, "public/script.js", "application/javascript; charset=utf-8");
    } else {
        send_response(client, "404 Not Found", "text/plain; charset=utf-8", "Not found");
    }
    shutdown(client, SD_BOTH);
    closesocket(client);
}

int main(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        fprintf(stderr, "Could not create socket.\n");
        WSACleanup();
        return 1;
    }
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Could not bind port %d. Is another app using it?\n", PORT);
        closesocket(server); WSACleanup(); return 1;
    }
    if (listen(server, 10) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed.\n");
        closesocket(server); WSACleanup(); return 1;
    }
    printf("Todo List server running at http://localhost:%d\n", PORT);
    printf("Keep this terminal open. Press Ctrl+C to stop.\n");
    for (;;) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;
        handle_client(client);
    }
    closesocket(server);
    WSACleanup();
    return 0;
}
