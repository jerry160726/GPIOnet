// g++ rgb.c -o rgb
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // open()

const char* host = "192.168.0.197";  // server IP
int port = 7000;                     // server port
const char* device_path = "/dev/gpio_device"; // GPIO device file

// 控制 GPIO 的 function
void write_to_gpio(const char* state) {
    int fd = open(device_path, O_WRONLY);   // kernel driver裡的gpio_open()。經由/dev/gpio_device和driver溝通。
    if (fd == -1) {
        perror("Failed to open /dev/gpio_device");
        return;
    }

    if (write(fd, state, strlen(state)) < 0) {  // 把字串state（"01"）寫入開啟的裝置檔案，觸發kernel的gpio_write()。
        perror("Failed to write to GPIO device");
    } else {
        printf("GPIO write: %s\n", state);
    }

    close(fd);
}

int main()
{
    int sock_fd;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // 建立 socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // 設定 server 位址
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_aton(host, &serv_addr.sin_addr);

    // 嘗試連線 server
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock_fd);
        exit(1);
    }

    printf("Connected to server %s:%d\n", host, port);

    // 接收 server 指令並控制 GPIO
    while (1) {
        int nbytes = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
        if (nbytes <= 0) {
            printf("Server closed connection or error.\n");
            break;
        }

        buffer[nbytes] = '\0'; // 字串結尾
        printf("Received command: %s\n", buffer);

        // 檢查是否為合法控制碼
        if (strcmp(buffer, "00") == 0 || strcmp(buffer, "01") == 0 ||
            strcmp(buffer, "10") == 0 || strcmp(buffer, "11") == 0) {
            write_to_gpio(buffer);  // 寫入 GPIO
        } else {
            printf("Ignored invalid command.\n");
        }
    }

    close(sock_fd);
    return 0;
}
