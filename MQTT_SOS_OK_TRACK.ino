#define TINY_GSM_MODEM_SIM7600
#define DEBUG false

#include <PubSubClient.h>
#include <TinyGsmClient.h>

// LTE Modem pins
#define LTE_PWRKEY_PIN 5
#define LTE_RESET_PIN 6
#define LTE_FLIGHT_PIN 7

// Switch pins
const int switch1Pin = 8;
const int switch2Pin = 3;

// Flags for button presses
volatile bool sosPressed = false;
volatile bool okPressed = false;
volatile bool sendingMessage = false;

// APN and MQTT credentials
const char apn[] = "airtelgprs.com";
const char broker[] = "35.200.163.26";
const int brokerPort = 1883;

const char topicTracking[] = "sim7600/nmea";
const char topicSOS[] = "sim7600/sos";
const char topicOK[] = "sim7600/ok";

// Intervals
const unsigned long trackSendInterval = 1500; // 1.5 seconds
unsigned long lastTrackSendTime = 0;

// Variables for debouncing
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
const unsigned long debounceDelay = 50; // 50 milliseconds debounce time

// GSM and MQTT clients
TinyGsm modem(Serial1);
TinyGsmClient gsmClient(modem);
PubSubClient mqttClient(gsmClient);

// Function to extract NMEA sentence
String extractNMEA(String response) {
    int start = response.indexOf("+CGPSINFO: ");
    if (start != -1) {
        start += 11; // Move past "+CGPSINFO: "
        int end = response.indexOf("OK", start);
        if (end != -1 && end > start) {
            String nmea = response.substring(start, end);
            nmea.trim(); // Trim any leading/trailing whitespace
            return nmea;
        }
    }
    return ""; // Return empty string if NMEA sentence not found or invalid format
}

// Send AT command to the modem
String sendData(String command, const int timeout, boolean debug = false) {
    String response = "";
    Serial1.println(command);

    long int startTime = millis();
    while (millis() - startTime < timeout) {
        while (Serial1.available()) {
            char c = Serial1.read();
            response += c;
        }
    }

    if (debug) {
        SerialUSB.print(command);
        SerialUSB.print(" Response: ");
        SerialUSB.println(response);
    }

    return response;
}

// Function to publish MQTT messages
void publishMessage(const char* topic, const String& payload) {
    if (mqttClient.publish(topic, payload.c_str())) {
        SerialUSB.println(String("Message published to ") + topic + ": " + payload);
    } else {
        SerialUSB.println(String("Failed to publish message to ") + topic);
    }
}

// Function to handle SOS messages
void handleSosMessage() {
    sendingMessage = true;
    sosPressed = false;

    SerialUSB.println("Sending SOS message...");
    String payload = "{\"carId\":1, \"message\": \"SOS\"}";

    for (int attempt = 0; attempt < 25; attempt++) {
        publishMessage(topicSOS, payload);
        SerialUSB.println("SOS sent successfully.");
        break;
    }

    sendingMessage = false;
}

// Function to handle OK messages
void handleOkMessage() {
    sendingMessage = true;
    okPressed = false;

    SerialUSB.println("Sending OK message...");
    String payload = "{\"carId\":1, \"message\": \"OK\"}";

    for (int attempt = 0; attempt < 25; attempt++) {
        publishMessage(topicOK, payload);
        SerialUSB.println("OK sent successfully.");
        break;
    }

    sendingMessage = false;
}

// Function to publish NMEA data to MQTT
void sendTrackData() {
    if (sendingMessage) return;

    String gpsInfo = sendData("AT+CGPSINFO", 1300, DEBUG);
    String nmeaSentence = extractNMEA(gpsInfo);
    SerialUSB.println(nmeaSentence + " nmea");

    if (nmeaSentence.length() > 8) {
        SerialUSB.println("Publishing Track data to MQTT");
        String payload = "{\"nmea\": \"" + nmeaSentence + "\",\"carId\":1}";
        publishMessage(topicTracking, payload);
    }
}


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
        
        if (mqttClient.connect("TrackingClient")) {
            SerialUSB.println("connected.");
        } else {
            SerialUSB.print("failed, rc=");
            SerialUSB.print(mqttClient.state());
            SerialUSB.println(" trying again in 5 seconds.");
            // checkInternet();
            delay(8000);
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
    // Initialize serial communication
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

    // Configure switches
    pinMode(switch1Pin, INPUT_PULLUP);
    pinMode(switch2Pin, INPUT_PULLUP);

    // Wait for SerialUSB
    while (!SerialUSB) {
        delay(10);
    }

    SerialUSB.println("Initializing modem...");
    initModem();

    // Initialize MQTT
    mqttClient.setServer(broker, brokerPort);

    SerialUSB.println("Enabling GPS...");
    sendData("AT+CGPS=0", 3000, DEBUG); // Disable GPS (if already running)
    sendData("AT+CGPS=1", 3000, DEBUG); // Enable GPS
}

void loop() {
    // Reconnect MQTT if disconnected
    if (!mqttClient.connected()) {
        SerialUSB.println("Reconnecting mqtt client");
        reconnectMQTT();
    }
    // mqttClient.loop();

    // Check Switch 1 (SOS button)
    if (digitalRead(switch1Pin) == LOW) { // Button pressed
        if (millis() - lastDebounceTime1 > debounceDelay) { // Debounce logic
            lastDebounceTime1 = millis();
            sosPressed = true; // Set flag for SOS
        }
    }

    // Check Switch 2 (OK button)
    if (digitalRead(switch2Pin) == LOW) { // Button pressed
        if (millis() - lastDebounceTime2 > debounceDelay) { // Debounce logic
            lastDebounceTime2 = millis();
            okPressed = true; // Set flag for OK
        }
    }

    // Handle SOS message
    if (sosPressed) {
        handleSosMessage();
    }

    // Handle OK message
    if (okPressed) {
        handleOkMessage();
    }

    // Send track data at regular intervals
    unsigned long currentTime = millis();
    if (currentTime - lastTrackSendTime >= trackSendInterval) {
        lastTrackSendTime = currentTime;
        sendTrackData();
    }
}