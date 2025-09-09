#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

static const int PORT = 8080;

std::string metrics_body(double uptime_sec) {
  std::string b;
  b += "# HELP build_info Build information.\n";
  b += "# TYPE build_info gauge\n";
  b += "build_info{git_sha=\"dev\",version=\"0.0.1\"} 1\n";
  b += "# HELP engine_uptime_seconds Engine uptime in seconds.\n";
  b += "# TYPE engine_uptime_seconds gauge\n";
  b += "engine_uptime_seconds " + std::to_string(uptime_sec) + "\n";
  return b;
}

std::string http_ok_text(const std::string& body, const std::string& content_type="text/plain") {
  std::string hdr = "HTTP/1.1 200 OK\r\n";
  hdr += "Content-Type: " + content_type + "\r\n";
  hdr += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  hdr += "Connection: close\r\n\r\n";
  return hdr + body;
}

// JUST CREATING SERVER NOT CLIENT
int main() {
  using clock = std::chrono::steady_clock;
  const auto start = clock::now();

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0); // create server socket
  if (server_fd < 0) { std::perror("socket"); return 1; }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);

  if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { // bind socket to addr
    std::perror("bind"); return 1;
  }
  if (listen(server_fd, 16) < 0) {
    std::perror("listen"); return 1;
  }

  std::cout << "[engine] listening on 0.0.0.0:" << PORT << " ...\n";
  while (true) {
    sockaddr_in cli{}; socklen_t len = sizeof(cli);
    int client = accept(server_fd, (sockaddr*)&cli, &len);
    if (client < 0) { std::perror("accept"); continue; }

    char buf[2048];
    ssize_t n = recv(client, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(client); continue; }
    buf[n] = '\0';
    std::string req(buf, n);
    bool want_metrics = req.find("GET /metrics") == 0 || req.find("GET /metrics ") != std::string::npos;

    auto uptime = std::chrono::duration<double>(clock::now() - start).count();
    std::string body = want_metrics ? metrics_body(uptime) : std::string("ok\n");
    std::string resp = http_ok_text(body, want_metrics ? "text/plain; version=0.0.4" : "text/plain");

    send(client, resp.data(), resp.size(), 0);
    close(client);
  }
  return 0;
}


// // C++ program to show the example of server application in
// // socket programming
// #include <cstring>
// #include <iostream>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <unistd.h>

// using namespace std;

// int main()
// {
//     // creating socket
//     int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

//     // specifying the address
//     sockaddr_in serverAddress;
//     serverAddress.sin_family = AF_INET;
//     serverAddress.sin_port = htons(8080);
//     serverAddress.sin_addr.s_addr = INADDR_ANY;

//     // binding socket.
//     bind(serverSocket, (struct sockaddr*)&serverAddress,
//          sizeof(serverAddress));

//     // listening to the assigned socket
//     listen(serverSocket, 5);

//     // accepting connection request
//     int clientSocket
//         = accept(serverSocket, nullptr, nullptr);

//     // recieving data
//     char buffer[1024] = { 0 };
//     recv(clientSocket, buffer, sizeof(buffer), 0);
//     cout << "Message from client: " << buffer
//               << endl;

//     // closing the socket.
//     close(serverSocket);

//     return 0;
// }