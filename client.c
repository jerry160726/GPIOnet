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
    int sock_fd;                    // 建立socket連線 (sock_fd)
    struct sockaddr_in serv_addr;   // 儲存server的IP與port資訊
    char buffer[1024] = {0};        // 存放接收的資料

    // 建立 socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);  // AF_INET：IPv4，SOCK_STREAM：TCP協定
    if (sock_fd == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // 建立server IP與port
    serv_addr.sin_family = AF_INET;             // 使用IPv4
    serv_addr.sin_port = htons(port);           // htons()：把主機位元序轉成網路位元序（Big Endian）
    inet_aton(host, &serv_addr.sin_addr);       // inet_aton()：將字串IP轉為binary IP 。

    // 建立TCP連線
    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock_fd);
        exit(1);
    }

    printf("Connected to server %s:%d\n", host, port);

    // 接收 server 指令並控制 GPIO
    while (1) {
        int nbytes = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);  
        // 從socket中接收資料，存到buffer裡，並將接收到的位元組數存進nbytes
        if (nbytes <= 0) {
            printf("Server closed connection or error.\n");
            break;
        }

        buffer[nbytes] = '\0';      // 為接收到的字串加上結尾，讓它變成C字串
        printf("Received command: %s\n", buffer);

        // 檢查接收到的指令是否有效（只允許 "00"、"01"、"10"、"11"）
        if (strcmp(buffer, "00") == 0 || strcmp(buffer, "01") == 0 ||
            strcmp(buffer, "10") == 0 || strcmp(buffer, "11") == 0) {
            write_to_gpio(buffer);
            // 把字串寫到/dev/gpio_device裝置節點, 觸發kernel module去控制LED。
        } else {
            printf("Ignored invalid command.\n");
        }
    }

    close(sock_fd);
    return 0;
}
