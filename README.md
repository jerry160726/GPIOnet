# 從底層驅動到遠端互動的嵌入式系統。

### gpio_led_driver.c
這是character device kernel module，用來控制GPIO腳位。實作了open、read、write、releas，使用者透過寫入/dev/gpio_device指定要設為高低電位的GPIO pin，控制LED閃爍。

### Server.c
使用wiringPi偵測實體按鈕的按壓，每次按一下就依序送出 "00"、"01"、"10"、"11" 四種狀態中的一個到client。

### Client.c
連接到伺服器接收 "00", "01", "10", "11" 這四種2-bit 控制訊號，並把它寫入/dev/gpio_device來控制實際的LED。


