#ifdef _WIN32
#include <Winsock2.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ws2tcpip.h>
#include "macros.h"
#include "globals.h"

void _runFTPServer(void) {
  extern SOCKET s;
  extern SOCKET ns;
  extern SOCKET ns_data;
  extern SOCKET s_data_act;

  extern char send_buffer[200];
  extern char receive_buffer[200];
  extern char portNum[NI_MAXSERV];
  extern char clientHost[NI_MAXHOST];
  extern char clientService[NI_MAXSERV];

  extern int active;
  extern int n;
  extern int bytes;
  extern int addrlen;
  extern int iResult;

  extern struct sockaddr_storage localaddr;
  extern struct sockaddr_storage remoteaddr;
  extern struct addrinfo hints;
  extern struct addrinfo* result;
  extern struct addrinfo* ipv4;
  extern struct addrinfo* ipv6;
  extern struct addrinfo* pAddrInfo = result;
  memset(&hints, 0, sizeof(struct addrinfo));

  // Setup IP-agnostic structs.
  memset(&localaddr, 0, sizeof(localaddr));
  memset(&remoteaddr, 0, sizeof(remoteaddr));

  // Client IP.
  memset(clientHost, 0, sizeof(clientHost));

  // Client port.
  memset(clientService, 0, sizeof(clientService));

  _setupWinsock();
  _printHeading();
  _determineListeningPort();
  _determineIPVersion();
  _setupWelcomeSocket();
  _bindSocket();
  _handleConnections();
}

void _setupWinsock(void) {
  WSADATA wsadata;

  // Check that Winsock works.
  if (WSAStartup(WSVERS, &wsadata) == INVALID_SOCKET) {
    fprintf(stderr, "Error: WSAStartup failed\n");
    WSACleanup();
    exit(1);
  }

  // Check for Winsock version.
  if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2) {
    fprintf(stderr, "Error: Could not find a usable version of Winsock.dll\n");
    WSACleanup();
    exit(1);
  }
}

void _printHeading(void) {
  fprintf(stdout, "*************************************\n");
  fprintf(stdout, "*                                   *\n");
  fprintf(stdout, "*        A Simple FTP Server        *\n");
  fprintf(stdout, "*                                   *\n");
  fprintf(stdout, "* Author:  Adam Peryman             *\n");
  fprintf(stdout, "* Contact: adam.peryman@gmail.com   *\n");
  fprintf(stdout, "*************************************\n");
}

void _determineListeningPort(void) {
  if (argc == 2) {
    iResult = getaddrinfo(NULL, argv[1], &hints, &result);
    sprintf(portNum, "%s", argv[1]);
  } else {
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    sprintf(portNum, "%s", DEFAULT_PORT);
  }
  freeaddrinfo(result);

  if (iResult != 0) {
    fprintf(stderr, "Error: getaddrinfo failed: %d\n", iResult);
    WSACleanup();
    exit(1);
  }
}

void _determineIPVersion(void) {
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  while (pAddrInfo) {
    if ((!ipv4) && (pAddrInfo->ai_family == AF_INET)) {
        ipv4 = pAddrInfo;
    } else if ((!ipv6) && (pAddrInfo->ai_family == AF_INET6)) {
      ipv6 = pAddrInfo;
    } else {
      break;
    }

    pAddrInfo = pAddrInfo->ai_next;
  }
}

// TODO: Add support for other protocols.
void _setupWelcomeSocket(void) {
  if (ipv4) {
    s = socket(AF_INET, result->ai_socktype, result->ai_protocol);
  } else if (ipv6) {
    s = socket(AF_INET6, result->ai_socktype, result->ai_protocol);
  } else {
    fprintf(stderr, "Error: Invalid protocol.\n");
    freeaddrinfo(result);
    WSACleanup();
    exit(1);
  }

  if (s == INVALID_SOCKET) {
    fprintf(stderr, "Error: socket(): %d\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
    exit(1);
  }
}

void _bindSocket(void) {
  if (ipv4) {
    iResult = bind(s, ipv4->ai_addr, (int)result->ai_addrlen);
  } else {
    iResult = bind(s, ipv6->ai_addr, (int)result->ai_addrlen);
  }

  if (iResult == SOCKET_ERROR) {
    fprintf(stderr, "Error: Bind failed with error: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    closesocket(s);
    WSACleanup();
    exit(1);
  }
}

void _handleConnections(void) {
  if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
    fprintf(stderr, "Error: Listen failed with error: %d\n", WSAGetLastError());
    closesocket(s);
    WSACleanup();
    exit(1);
  } else {
    fprintf(stdout, "\nSERVER is listening at PORT: %s\n", portNum);
  }

  while (!PIGS_FLY) {
    fprintf(stdout, "SERVER is waiting for an incoming connection request...");

    addrlen = sizeof(remoteaddr);

    ns = INVALID_SOCKET;
    ns = accept(s, (struct sockaddr *)(&remoteaddr), &addrlen);

    if (ns == INVALID_SOCKET) {
      fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
      closesocket(s);
      WSACleanup();
      exit(1);
    } else {
      fprintf(stdout, "\nA <<<CLIENT>>> has been accepted.\n");

      getnameinfo((struct sockaddr *)&remoteaddr, addrlen,
        clientHost, sizeof(clientHost),
        clientService, sizeof(clientService),
        NI_NUMERICHOST);

      fprintf(stdout, "\nConnected to <<<Client>>> with IP address: %s, at Port: %s\n",
        clientHost, clientService);
    }

    // All's going well.
    sprintf(send_buffer, "220 FTP Server ready. \r\n");
    bytes = send(ns, send_buffer, strlen(send_buffer), 0);

    // Communication loop.
    while (!PIGS_FLY) {
      n = 0;
      while (!PIGS_FLY) {
        // Receive byte by byte.
        bytes = recv(ns, &receive_buffer[n], 1, 0);
        if (bytes <= 0) {
          break;
        }
        // End on line feed.
        if (receive_buffer[n] == '\n') {
          receive_buffer[n] = '\0';
          break;
        }
        // Trim CRs.
        if (receive_buffer[n] != '\r') {
          n++;
        }
      }

      if (bytes <= 0) {
        break;
      }

      // Begin processing commands.
      if (strncmp(receive_buffer, "USER", 4) == 0) {
        if (strstr(receive_buffer, DEFAULT_USER) != NULL) {
          fprintf(stdout, "Logging in \n");
          sprintf(send_buffer, "331 Password required \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
          if (bytes < 0) {
            break;
          }
        } else {
          sprintf(send_buffer, "530 Invalid user.\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        }
      }

      else if (strncmp(receive_buffer, "PASS", 4) == 0) {
        if (strstr(receive_buffer, DEFAULT_PASSWORD) != NULL) {
          fprintf(stdout, "Typing password (anything will do... \n");
          sprintf(send_buffer, "230 Public login sucessful \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
          if (bytes < 0) {
            break;
          }
        } else {
          sprintf(send_buffer, "530 Invalid password.\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        }
      }

      else if (strncmp(receive_buffer, "SYST", 4) == 0) {
        fprintf(stdout, "Information about the system \n");
        sprintf(send_buffer, "215 Windows Type: WIN32 \r\n");
        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        if (bytes < 0) {
          break;
        }
      }

      else if (strncmp(receive_buffer, "QUIT", 4) == 0) {
        fprintf(stdout, "Quit \n");
        sprintf(send_buffer, "221 Connection closed by client \r\n");
        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        if (bytes < 0) {
          closesocket(ns);
          break;
        }
      }

      // Needs IPv4/IPv6 fixes.
      else if (strncmp(receive_buffer, "PORT", 4) == 0) {
        // Port vars.
        int act_port[2];
        int act_ip[4], port_dec;
        char ip_decimal[40];
        fprintf(stdout, "\n\tActive FTP mode, the client is listening... \n");

        // IPv4. This is tough to fix.
        struct sockaddr_in local_data_addr_act = {};
        memset(&local_data_addr_act, 0, sizeof(local_data_addr_act));

        s_data_act = socket(AF_INET, SOCK_STREAM, 0);
        if (s_data_act == INVALID_SOCKET) {
          fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
          closesocket(s);
          WSACleanup();
          exit(1);
        }

        // Connection flag.
        active = 1;

        int scannedItems = sscanf(receive_buffer, "PORT %d,%d,%d,%d,%d,%d",
          &act_ip[0], &act_ip[1], &act_ip[2], &act_ip[3], &act_port[0], &act_port[1]);

        if (scannedItems < 6) {
          sprintf(send_buffer, "501 Syntax error in arguments \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
          break;
        }

        // Setup remote data struct.
        local_data_addr_act.sin_family = AF_INET;
        sprintf(ip_decimal, "%d.%d.%d.%d", act_ip[0], act_ip[1], act_ip[2], act_ip[3]);

        local_data_addr_act.sin_addr.s_addr = inet_addr(ip_decimal);

        port_dec = act_port[0];
        port_dec = port_dec << 8;
        port_dec = port_dec + act_port[1];

        local_data_addr_act.sin_port = htons(port_dec);

        // Attempt connections.
        if (connect(s_data_act, (struct sockaddr *)&local_data_addr_act, (int) sizeof(struct sockaddr)) != 0) {
          fprintf(stdout, "trying connection in %s %d\n", inet_ntoa(local_data_addr_act.sin_addr),
            ntohs(local_data_addr_act.sin_port));

          sprintf(send_buffer, "425 Something is wrong, can't start active connection... \r\n");

          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
          closesocket(s_data_act);
        } else {
          sprintf(send_buffer, "200 PORT Command successful\r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
          fprintf(stdout, "Connected to client\n");
        }
      }

      else if ((strncmp(receive_buffer, "LIST", 4) == 0) || (strncmp(receive_buffer, "NLST", 4) == 0)) {
        system("dir > tmp.txt");
        FILE *fin = fopen("tmp.txt", "r");
        sprintf(send_buffer, "150 Opening ASCII mode data connection... \r\n");
        bytes = send(ns, send_buffer, strlen(send_buffer), 0);

        char temp_buffer[80];
        while (!feof(fin)) {
          fgets(temp_buffer, 78, fin);
          sprintf(send_buffer, "%s", temp_buffer);
          if (active == 0) {
            send(ns_data, send_buffer, strlen(send_buffer), 0);
          } else {
            send(s_data_act, send_buffer, strlen(send_buffer), 0);
          }
        }
        fclose(fin);

        sprintf(send_buffer, "226 File transfer complete. \r\n");
        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        if (active == 0) {
          closesocket(ns_data);
        } else {
          closesocket(s_data_act);
        }
      }

      else if (strncmp(receive_buffer, "RETR", 4) == 0) {
        // Get filename.
        char* temp = (char*)malloc(strlen(receive_buffer + 1) * sizeof(char));
        strcpy(temp, receive_buffer);
        char* excess = (char*)malloc(BUFF_SIZE * sizeof(char));
        char* file = (char*)malloc(BUFF_SIZE * sizeof(char));
        sscanf(temp, "%s %s", excess, file);
        free(temp);
        free(excess);

        // From LIST.
        FILE *fin = fopen(file, "r");
        sprintf(send_buffer, "150 Opening ASCII mode data connection... \r\n");

        bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        char temp_buffer[80] = { '\0' };
        while (!feof(fin)) {
          fgets(temp_buffer, 78, fin);
          sprintf(send_buffer, "%s\n", temp_buffer);
          if (active == 0) {
            send(ns_data, send_buffer, strlen(send_buffer), 0);
          } else {
            send(s_data_act, send_buffer, strlen(send_buffer), 0);
          }
          fclose(fin);
          free(file);

          sprintf(send_buffer, "226 File transfer complete. \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);

          if (active == 0) {
            closesocket(ns_data);
          } else {
            closesocket(s_data_act);
          }
        }

        else if (strncmp(receive_buffer, "STOR", 4) == 0) {
          // Get filename.
          char* temp = (char*)malloc(strlen(receive_buffer + 1) * sizeof(char));
          strcpy(temp, receive_buffer);
          char* excess = (char*)malloc(BUFF_SIZE * sizeof(char));
          char* file = (char*)malloc(BUFF_SIZE * sizeof(char));
          sscanf(temp, "%s %s", excess, file);
          free(temp);
          free(excess);

          // From main().
          FILE* fout = fopen(file, "w");
          sprintf(send_buffer, "150 File exists, proceeding. \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);

          while (!PIGS_FLY) {
            n = 0;
            while (!PIGS_FLY) {
              // Receive byte by byte.
              bytes = recv(s_data_act, &receive_buffer[n], 1, 0);

              // Process request.
              if (bytes <= 0) {
                break;
              }

              if (receive_buffer[n] == '\n') {
                receive_buffer[n] = '\0';
                break;
              }

              // Trim CRs.
              if (receive_buffer[n] != '\r') {
                n++;
              }
            }

            receive_buffer[n] = '\0';
            fprintf(fout, "%s\n", receive_buffer);
            if (bytes <= 0) {
              break;
            }
          }

          sprintf(send_buffer, "226 File transfer complete. \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);

          fclose(fout);
          free(file);
          if (active == 0) {
            closesocket(ns_data);
          } else {
            closesocket(s_data_act);
          }
        }

        // Bad command.
        else {
          sprintf(send_buffer, "202 Invalid command. \r\n");
          bytes = send(ns, send_buffer, strlen(send_buffer), 0);
        }
      }

      closesocket(ns);
      sprintf(send_buffer, "221 Goodbyte! \r\n");
      bytes = send(ns, send_buffer, strlen(send_buffer), 0);
    }

    closesocket(s);
    fprintf(stdout, "\nSERVER SHUTTING DOWN...\n");
    exit(0);
  }
}
