#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <map>
#include <functional>

using Handler = std::function<std::string(const std::string &body, int &status)>;

// ============ 路由表 ============
static std::map<std::string, Handler> routes;

void registerRoute(const std::string &key, Handler h)
{
    routes[key] = std::move(h);
}

// ============ HTTP 工具 ============
struct Request
{
    std::string method;
    std::string path;
    std::string body;
};

Request parseRequest(const std::string &raw)
{
    Request req;
    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos)
        return req;
    std::istringstream ls(raw.substr(0, lineEnd));
    ls >> req.method >> req.path;
    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd != std::string::npos)
    {
        req.body = raw.substr(headerEnd + 4);
    }
    return req;
}

std::string makeResponse(int status, const std::string &body)
{
    std::string statusText;
    switch (status)
    {
    case 200:
        statusText = "OK";
        break;
    case 400:
        statusText = "Bad Request";
        break;
    case 404:
        statusText = "Not Found";
        break;
    case 500:
        statusText = "Internal Server Error";
        break;
    default:
        statusText = "OK";
        break;
    }
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusText << "\r\n"
        << "Content-Type: application/json; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

// ============ 业务处理函数（对应你原来的逻辑） ============
std::string alarmReset(const std::string &body)
{
    // TODO: 解析 body，执行复位
    return "{\"result\":\"ALARMRESET ok\"}";
}

std::string sensorAlarm(const std::string &body)
{
    return "{\"result\":\"faultStatus ok\",\"faults\":[]}";
}

std::string alarmSilencing(const std::string &body)
{
    return "{\"result\":\"ALARMSILENCING ok\"}";
}

std::string getAlarmType(const std::string &body)
{
    return "{\"result\":\"ALARMTYPE ok\",\"types\":[\"fire\",\"gas\",\"water\"]}";
}

// ============ 连接处理（对应 onHttpAccepted） ============
void handleClient(int clientFd)
{
    char buf[8192];
    ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
    {
        close(clientFd);
        return;
    }
    buf[n] = '\0';

    Request req = parseRequest(std::string(buf, n));

    int status = 404;
    std::string body = "{\"error\":\"not found\"}";

    // 按 URL 关键字匹配，逻辑和你原来的 if/else 一致
    for (auto &[key, handler] : routes)
    {
        if (req.path.find(key) != std::string::npos)
        {
            body = handler(req.body, status);
            break;
        }
    }

    std::string resp = makeResponse(status, body);
    send(clientFd, resp.c_str(), resp.size(), 0);
    close(clientFd);
}

// ============ main ============
int main()
{
    // 注册路由
    registerRoute("ALARMRESET", [](const std::string &b, int &s)
                  { s = 200; return alarmReset(b); });
    registerRoute("faultStatus", [](const std::string &b, int &s)
                  { s = 200; return sensorAlarm(b); });
    registerRoute("ALARMSILENCING", [](const std::string &b, int &s)
                  { s = 200; return alarmSilencing(b); });
    registerRoute("ALARMTYPE", [](const std::string &b, int &s)
                  { s = 200; return getAlarmType(b); });
    // Railway 健康检查
    registerRoute("health", [](const std::string &, int &s)
                  { s = 200; return std::string("{\"status\":\"ok\"}"); });

    // ⚠️ 关键：从环境变量读取 PORT，绑定 0.0.0.0
    const char *portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    addr.sin_port = htons(port);

    if (bind(serverFd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(serverFd, 64) < 0)
    {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on 0.0.0.0:" << port << std::endl;

    while (true)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr *)&clientAddr, &clientLen);
        if (clientFd < 0)
            continue;
        std::thread(handleClient, clientFd).detach();
    }

    close(serverFd);
    return 0;
}