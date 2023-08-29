#include <arpa/inet.h>
#include <array>
#include <asm-generic/socket.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_timespec.h>
#include <ctime>
#include <errno.h>
#include <iostream>
#include <linux/net_tstamp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define MCAST_IPADDR "239.1.1.1"

static constexpr unsigned short PORT = 23533;
int run_server() {
  int r = -1;
  std::cout << "listen\n";

  sockaddr_in serv_addr;
  memset(&serv_addr, 0x00, sizeof(sockaddr_in));
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  inet_pton(AF_INET, "127.0.0.1", &(serv_addr.sin_addr));
  serv_addr.sin_port = htons(PORT);
  r = bind(server_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
  while (r == 0) {
    if (r == 0) {
      r = listen(server_socket, 10);
    }
    if (r == 0) {
      std::cout << "listen is up\n";
      int client_socket = accept(server_socket, nullptr, nullptr);
      int read_result = 0;
      char buffer[255];
      int l = 1;
      timespec ts;
      timespec prev;
      bool had_ts = false;
      do {
        read_result = read(client_socket, buffer, 255);
        if (!had_ts) {
          had_ts = true;
          clock_gettime(CLOCK_MONOTONIC, &ts);
          std::cout << "line #" << l << " - " << read_result
                    << "bytes ts 0 set to " << ts.tv_sec << "." << ts.tv_nsec
                    << "\n";
        } else {
          prev = ts;
          clock_gettime(CLOCK_MONOTONIC, &ts);
          std::cout << "line # " << l << " - " << read_result
                    << " bytes delta = " << ts.tv_sec - prev.tv_sec << "."
                    << ts.tv_nsec - prev.tv_nsec << "\n";
        }

        l++;
      } while (read_result == 255);

      std::cout << "client\n";
      close(client_socket);
    } else {
      std::cout << "listen failed!!!\n" << strerror(r) << "\n";
    }
  }
  close(server_socket);
  return r;
}

int run_client_normal() {
  int r = -1;

  sockaddr_in serv_addr;
  memset(&serv_addr, 0x00, sizeof(sockaddr_in));

  int send_socket = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, "localhost", &(serv_addr.sin_addr));
  std::cout << "send - connecting....\n";
  r = connect(send_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
  if (r == 0) {
    std::cout << "connected!\n";
    char temp_buff[255];
    for (int v = 0; v < 255; ++v) {
      temp_buff[v] = (char)v;
    }
    for (int i = 0; i < 255; ++i) {
      send(send_socket, temp_buff, 255, 0);
    }

    close(send_socket);
  } else {
    std::cout << "connect failed!!!\n" << strerror(r) << "\n";
  }
  return -1;
}

static int mcast_bind(int fd, int index) {
  int err;
  struct ip_mreqn req;
  memset(&req, 0, sizeof(req));
  req.imr_ifindex = index;
  err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &req, sizeof(req));
  if (err) {
    std::cout << "setsockopt IP_MULTICAST_IF failed:" << err << "\n";
    return -1;
  }
  return 0;
}

static int mcast_join(int fd, int index, const struct sockaddr *grp,
                      socklen_t grplen) {
  int err, off = 0;
  struct ip_mreqn req;
  struct sockaddr_in *sa = (struct sockaddr_in *)grp;

  memset(&req, 0, sizeof(req));
  memcpy(&req.imr_multiaddr, &sa->sin_addr, sizeof(struct in_addr));
  req.imr_ifindex = index;
  err = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req));
  if (err) {
    std::cout << "setsockopt IP_ADD_MEMBERSHIP failed: " << err << "\n";
    return -1;
  }
  err = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off));
  if (err) {
    std::cout << "setsockopt IP_MULTICAST_LOOP failed: " << err << "\n";
    return -1;
  }
  return 0;
}

int run_client() {
  sockaddr_in serv_addr;
  memset(&serv_addr, 0x00, sizeof(sockaddr_in));
  int on = 1;
  ;
  int prio = 3;
  //  int send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  int send_socket = socket(AF_INET, SOCK_STREAM, 0);
  int r = send_socket >= 0 ? 0 : -1;
  in_addr mcast_addr;
  if (r == 0)
    r = inet_aton(MCAST_IPADDR, &mcast_addr) == 0 ? -1 : 0;
  const char *name = "ens33";
  if (r == 0) {
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "localhost", &(serv_addr.sin_addr));
    std::cout << "send - connecting....\n";

    r = setsockopt(send_socket, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio));
    if (r != 0) {
      std::cout << "setsockopt SO_PRIORITY failed\n";
    }
    r = setsockopt(send_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (r != 0) {
      std::cout << "setsockopt SO_REUSEADDR failed\n";
    }
    //  r = bind(send_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
    if (r != 0) {
      std::cout << "bind failed!\n";
    }
    //    r = setsockopt(send_socket, SOL_SOCKET, SO_BINDTODEVICE, name, 5);
    if (r != 0) {
      std::cout << "setsockopt SO_BINDTODEVICE  failed\n";
    }
    //   serv_addr.sin_addr = mcast_addr;
    //   r = mcast_join(send_socket, 0, (sockaddr *)&serv_addr,
    //   sizeof(serv_addr));
    if (r != 0) {
      std::cout << "mcast_join failed\n";
    }
    // r = mcast_bind(send_socket, 0);
    if (r != 0) {
      std::cout << "mcast_bind failed\n";
    }
    sock_txtime x;
    x.clockid = CLOCK_MONOTONIC;
    x.flags = SOF_TXTIME_DEADLINE_MODE;

    r = setsockopt(send_socket, SOL_SOCKET, SO_TXTIME, &x, sizeof(x));
    if (r != 0) {
      std::cout << "setsockopt SO_TXTIME failed\n";
    }
    r = connect(send_socket, (sockaddr *)&serv_addr, sizeof(serv_addr));
    char temp_buff[255];
    for (int v = 0; v < 255; ++v) {
      temp_buff[v] = (char)v;
    }
    if (r == 0) {
      std::cout << "connected\n";
      cmsghdr *cmsg;
      msghdr msg;
      iovec iov;
      ssize_t cnt;
      char control[CMSG_SPACE(sizeof(__u64))];

      iov.iov_len = 255;
      iov.iov_base = temp_buff;

      memset(&msg, 0, sizeof(msg));
      msg.msg_name = &serv_addr;
      msg.msg_namelen = sizeof(serv_addr);
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      timespec ts;
      std::cout << "sizeof ts => " << sizeof(timespec) << " vs "
                << sizeof(__u64) << "-" << sizeof(ts.tv_nsec) << "\n";
      for (int i = 0; i < 100; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_nsec += 5000 * (i + 1);
        unsigned long long int send_time = 0;
        memcpy(&send_time, &ts.tv_nsec, sizeof(__u64));
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_TXTIME;
        cmsg->cmsg_len = CMSG_LEN(sizeof(__u64));
        *((__u64 *)CMSG_DATA(cmsg)) = send_time;
        std::cout << "sendmsg# " << i + 1 << "\n";
        int m = sendmsg(send_socket, &msg, 0);
        if (m != -1) {
          std::cout << "sendmsg -> " << m << "sent\n";
        } else {
          std::cout << "sendmsg err\n";
        }
      }

      close(send_socket);
    } else {
      std::cout << "connect failed\n";
    }
  }
  return r;
}

int main(int argc, char **argv) {
  int r = -1;
  if (argc >= 2) {
    if (argv[1][0] == '0') {
      r = run_server();
    } else if (argv[1][0] == '1') {
      r = run_client();
    }
  }

  return r;
}
