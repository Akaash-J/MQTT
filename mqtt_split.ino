#define TINY_GSM_MODEM_SIM7600
#include <PubSubClient.h>
#include <TinyGsmClient.h>

// LTE Modem pins
#define LTE_PWRKEY_PIN 5
#define LTE_RESET_PIN 6
#define LTE_FLIGHT_PIN 7

// APN and MQTT credentials
const char apn[] = "airtelgprs.com";
const char broker[] = "35.200.163.26";
const int brokerPort = 1883;
const char topic[] = "sim7600/static";

// GSM and MQTT clients
TinyGsm modem(Serial1);
TinyGsmClient gsmClient(modem);
PubSubClient mqttClient(gsmClient);

// Intervals
const unsigned long sendInterval = 2000; // 2 seconds
unsigned long lastSendTime = 0;
void checkInternet() {
    if (modem.isGprsConnected()) {
        SerialUSB.println("Modem is connected to the Internet.");
    } else {
        SerialUSB.println("Modem is NOT connected to the Internet. Retrying...");
        modem.gprsDisconnect();
        modem.gprsConnect(apn, "", "");
    }
}
// MQTT reconnect function
void reconnectMQTT() {
    while (!mqttClient.connected()) {
        SerialUSB.print("Connecting to MQTT broker...");
        if (mqttClient.connect("StaticDataClient")) {
            SerialUSB.println("connected.");
        } else {
            SerialUSB.print("failed, rc=");
            SerialUSB.print(mqttClient.state());
            SerialUSB.println(" trying again in 5 seconds.");
            checkInternet();
            delay(5000);
        }
    }
}

// LTE modem initialization
void initModem() {
    modem.restart();
    if (!modem.gprsConnect(apn, "", "")) {
        SerialUSB.println("Failed to connect to GPRS. Retrying...");
        while (!modem.gprsConnect(apn, "", "")) {
            delay(5000);
        }
    }
    SerialUSB.println("GPRS connected.");
}

void setup() {
    SerialUSB.begin(115200);
    Serial1.begin(115200);

    // Configure LTE pins
    pinMode(LTE_RESET_PIN, OUTPUT);
    digitalWrite(LTE_RESET_PIN, LOW);

    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(LTE_PWRKEY_PIN, HIGH);
    delay(2000);
    digitalWrite(LTE_PWRKEY_PIN, LOW);

    pinMode(LTE_FLIGHT_PIN, OUTPUT);
    digitalWrite(LTE_FLIGHT_PIN, LOW);

    while (!SerialUSB) {
        delay(10);
    }

    SerialUSB.println("Initializing modem...");
    initModem();

    mqttClient.setServer(broker, brokerPort);
}

void loop() {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }

    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval) {
        lastSendTime = currentTime;
        String payload = "{\"carId\":1, \"message\": \"Static Data\"}";
        if (mqttClient.publish(topic, payload.c_str())) {
            SerialUSB.println("Static data sent: " + payload);
        } else {
            SerialUSB.println("Failed to send static data.");
        }
    }
}
