// g++ server-wiringpi.c -o server -lwiringPi
#include <wiringPi.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUTTON_PIN 0  // wiringPi 編號，對應 BCM GPIO17
const char* states[] = {"00", "01", "10", "11"};
int state_index = 0;

const char* host = "0.0.0.0";
int port = 7000;

int main()
{
    // 1. 初始化 wiringPi
    if (wiringPiSetup() == -1) {
        perror("wiringPiSetup failed");
        return 1;
    }

    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP); // 使用內建上拉

    // 2. 建立 socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation error");
        exit(1);
    }

    int on = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

    struct sockaddr_in my_addr, client_addr;
    my_addr.sin_family = AF_INET;
    inet_aton(host, &my_addr.sin_addr);
    my_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        perror("Binding error");
        exit(1);
    }

    listen(sock_fd, 5);
    printf("Server started on %s:%d\nWaiting for connection...\n", host, port);

    socklen_t addrlen = sizeof(client_addr);
    int new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addrlen);    // 等待client來連線
    if (new_fd == -1) {
        perror("Accept error");
        return 1;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    // 3. 持續檢查按鈕狀態
    int last_state = 1;
    while (1) {
        int current = digitalRead(BUTTON_PIN);

        // 檢查是否按下（由高到低）
        if (last_state == 1 && current == 0) {
            const char* msg = states[state_index];
            send(new_fd, msg, strlen(msg), 0);
            printf("Button pressed. Sent: %s\n", msg);
            state_index = (state_index + 1) % 4;

            delay(300); // 防彈跳用
        }

        last_state = current;
        delay(10); // loop delay
    }

    close(new_fd);
    close(sock_fd);
    return 0;
}
