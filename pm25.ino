#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "PMS.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold24pt7b.h>

const char *ssid = " ";
const char *password = " ";

const char *mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char *mqtt_Client = " ";
const char *mqtt_username = " ";
const char *mqtt_password = " ";

const char *ntpServer = "th.pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

HardwareSerial SerialPMS(1);
PMS pms(SerialPMS);
PMS::DATA data;

#define RXD2 26
#define TXD2 25
#define LED1 16

// ESP32 --> Pantower PMS7003
// 26    --> RX
// 25    --> TX
// GND   --> GND

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;
WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "th.pool.ntp.org", 25200, 60000);

PubSubClient client(espClient);

char msg[100];

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection…");
        if (client.connect(mqtt_Client, mqtt_username, mqtt_password))
        {
            Serial.println("connected");
            client.subscribe("@msg/led");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println("try again in 5 seconds");
            delay(5000);
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    String message;
    for (int i = 0; i < length; i++)
    {
        message = message + (char)payload[i];
    }
    Serial.println(message);
    if (String(topic) == "@msg/led")
    {
        if (message == "on")
        {
            digitalWrite(LED1, 0);
            client.publish("@shadow/data/update", "{\"data\" : {\"led\" : \"on\"}}");
            Serial.println("LED on");
        }
        else if (message == "off")
        {
            digitalWrite(LED1, 1);
            client.publish("@shadow/data/update", "{\"data\" : {\"led\" : \"off\"}}");
            Serial.println("LED off");
        }
    }
}

void setup()
{
    // set serial port
    Serial.begin(9600);
    SerialPMS.begin(9600, SERIAL_8N1, RXD2, TXD2);
    pms.passiveMode();

    // set dispay
    display.setFont(&FreeSansBold24pt7b);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

    //connect to WiFi
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" CONNECTED");

    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    timeClient.begin();
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }

    //Serial.println("Waking up, wait 30 seconds for stable readings...");
    pms.wakeUp();
    delay(3000);

    //Serial.println("Send read request...");
    pms.requestRead();

    //Serial.println("Wait max. 1 second for read...");
    if (pms.readUntil(data))
    {

        //    Serial.print("PM 1.0 (ug/m3): ");
        //    Serial.println(data.PM_AE_UG_1_0);
        //
        //    Serial.print("PM 2.5 (ug/m3): ");
        //    Serial.println(data.PM_AE_UG_2_5);
        //
        //    Serial.print("PM 10.0 (ug/m3): ");
        //    Serial.println(data.PM_AE_UG_10_0);

        display.clearDisplay();
        display.setTextColor(WHITE, BLACK); //Text is white ,background is black
        display.setTextSize(1);
        display.setCursor(35, 50);
        display.print(data.PM_AE_UG_2_5);
        display.display();

        timeClient.update();
        Serial.println(timeClient.getFormattedDate());

        client.loop();
        String dataMsg = "{\"data\": { \"time\": \"" + String(timeClient.getFormattedDate()) + "\", \"pm1\":" + String(data.PM_AE_UG_1_0) + ", \"pm25\":" + String(data.PM_AE_UG_2_5) + ", \"pm100\":" + String(data.PM_AE_UG_10_0) + "}}";
        Serial.println(dataMsg);
        dataMsg.toCharArray(msg, (dataMsg.length() + 1));
        client.publish("@shadow/data/update", msg);
        delay(10000);
    }
    else
    {
        Serial.println("No data.");
    }

    //  Serial.println("Going to sleep for 60 seconds.");
    pms.sleep();
    delay(58000);

    // Do other stuff...
}
