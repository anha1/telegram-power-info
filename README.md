# telegram-power-info
This project submits messages into a Telegram chat when power goes out or is back on:

![chat](tg.jpg)

## Description and infrastructure requirenments
The project is based on esp32 board, please use VSCode with PlatformIO to upload the software.
WiFi connectivity is required in a moment of status chages to be able to send the messages,
so some king of UPS is required for your WiFi router. 
Esp32 also requires backup power to send the message, there are two ways to achieve this:

### Approach 1. Using LiIon battery for ESP32
This could be done using a regular LiIon 3.7v battery and a DW01 based charging board, please see this
[video](https://www.youtube.com/watch?v=Lk__xTxLlY0) for details. 
The single 5v power adapter could be used both to charge the esp32 battery and to detect the power outage.

### Approach 2. Using two power supplies
It is possible to use two 5v power supplies: one connected directly do detect power loss,
and another one connected to UPS to power esp32. In this cases, grounds of both supplies should be connected.

Please note, that the backup power is required only to send the messages (even 1 minute is enough) and is NOT required for the
whole period of power outage.

![schematic](schematic.png)

The voltage divider circuit (two resistors, 2*R1 = R2 ) is used to reduce power probe voltage to 3.3v, please do not apply 5v to an input pin directly.

### Telegram settings
In Telegram, you need to determine chat id using @myidbot and create a bot that will submit the messages using @BotFather.
It is also required to add the bot to the chat.

### Viber implementation
Viber API gives an ability to implement something similar, but the technical implemenation is much 
more demanding (is requires a server with callback URL and a kind of a database to track subscribed users)
so a simple Viber/Telegram switch will not be implemented here.

Your community affected by outages will enjoy it:

![chat](reallife.jpg)
