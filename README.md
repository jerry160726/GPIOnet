# 從底層驅動到遠端互動的嵌入式系統。

## gpio_led_driver.c
這是character device kernel module，用來控制GPIO腳位。實作了open、read、write、releas，使用者透過寫入/dev/gpio_device指定要設為高低電位的GPIO pin，控制LED閃爍。



