// 使用wiringPi偵測實體按鈕的按壓，每次按一下就依序送出 "00"、"01"、"10"、"11" 四種狀態中的一個到client
#include <wiringPi.h>           // 控制GPIO腳位
#include <sys/socket.h>         // 建立socket所需
#include <netinet/in.h>         // sockaddr_in 結構與常數
#include <arpa/inet.h>          // inet_ntoa, inet_aton
#include <unistd.h>             // read(), write(), close()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// wiringPi.h：控制實體GPIO。其它都是標準socket API所需。

#define BUTTON_PIN 0  // 按鈕接在wiringPi的第0號腳位（實際為BCM GPIO17）

// 定義四種要傳送的狀態，state_index是目前要傳哪一個。
const char* states[] = {"00", "01", "10", "11"};
int state_index = 0;

// 讓server綁定所有網卡，不管是從本機、區網、或外部來連都能連上來。
const char* host = "0.0.0.0";
int port = 7000;        // 監聽port 7000，等client來連線。

int main()
{
    // 1. 初始化 wiringPi
    if (wiringPiSetup() == -1) {
        perror("wiringPiSetup failed");
        return 1;
    }

    pinMode(BUTTON_PIN, INPUT);          // 將BUTTON_PIN設定為輸入模式
    // 啟用BUTTON_PIN的內建上拉電阻（Pull-Up Resistor）。 腳位預設為高電位（1），按下時拉到GND變低電位（0）
    pullUpDnControl(BUTTON_PIN, PUD_UP); 

    // 2. 建立TCP socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);  // 建立使用IPv4和TCP協定的網路socket
    if (sock_fd == -1) {
        perror("Socket creation error");
        exit(1);
    }

    int on = 1;
    /*設定socket的重用地址選項，即使程式剛剛才關閉，也能馬上重新啟動伺服器，不會卡在Address already in use的錯誤*/
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

    // 3. 綁定地址並開始監聽
    // my_addr: Server的IP & Port，用於bind()
    // client_addr: 儲存從accept()傳進來的client資訊
    struct sockaddr_in my_addr, client_addr;
    my_addr.sin_family = AF_INET;           // 指定socket使用IPv4網路協定
    inet_aton(host, &my_addr.sin_addr);     // 將IP字串（"0.0.0.0"）轉成二進位的格式，存進sin_addr
    my_addr.sin_port = htons(port);         // 將主機上的整數（7000）轉成network byte order，存進sin_port

    // 如果綁的是0.0.0.0:7000：連接到這台電腦port 7000的client都會導向這個socket
    if (bind(sock_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
        perror("Binding error");
        exit(1);
    }

    // 監聽socket，允許最多5個client同時排隊
    listen(sock_fd, 5);
    printf("Server started on %s:%d\nWaiting for connection...\n", host, port);

    // 4. 接受client的連線
    socklen_t addrlen = sizeof(client_addr);
    // 等待client來連線。一旦client連上，就會建立一個新的socket描述符new_fd。填好client_addr（包含client的IP、port）
    int new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (new_fd == -1) {
        perror("Accept error");
        return 1;
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    // 5. 持續檢查按鈕狀態
    int last_state = 1;
    while (1) {
        int current = digitalRead(BUTTON_PIN);      // 讀取按鈕的目前狀態

        // 檢查是否按下（由高到低）
        if (last_state == 1 && current == 0) {
            const char* msg = states[state_index];      // 從states[]陣列中，取出目前的控制字串（如 "01"）
            send(new_fd, msg, strlen(msg), 0);          // 將字串msg透過TCP socket傳給client
            printf("Button pressed. Sent: %s\n", msg);
            state_index = (state_index + 1) % 4;        // 每按一次按鈕就會將狀態循環切換：讓它循環在0~3之間

            delay(300);     // 防彈跳用
        }

        last_state = current;       // 儲存這輪的狀態（供下一輪比對）
        delay(10);      // 每次loop等10ms，不要跑太快
    }

    close(new_fd);
    close(sock_fd);
    return 0;
}
