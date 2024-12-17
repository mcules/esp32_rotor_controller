#include <WiFi.h>
#include <WiFiManager.h>
#include <vector>
#include <WebServer.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include "Rotor.h"

#ifdef ESP32_C3_DEVKITM_1
#define AZIMUTH_PIN_LEFT GPIO_NUM_0
#define AZIMUTH_PIN_RIGHT GPIO_NUM_1
#define AZIMUTH_PIN_POTI GPIO_NUM_2
#define ELEVATION_PIN_LEFT GPIO_NUM_3
#define ELEVATION_PIN_RIGHT GPIO_NUM_4
#define ELEVATION_PIN_POTI GPIO_NUM_5
#endif

#ifdef ESP32_C3_SUPERMINI
#define AZIMUTH_PIN_LEFT GPIO_NUM_0
#define AZIMUTH_PIN_RIGHT GPIO_NUM_1
#define AZIMUTH_PIN_POTI GPIO_NUM_2
#define ELEVATION_PIN_LEFT GPIO_NUM_3
#define ELEVATION_PIN_RIGHT GPIO_NUM_4
#define ELEVATION_PIN_POTI GPIO_NUM_5
#endif

WiFiManager wifiManager;

// Define default values
#define DEFAULT_TCP_SERVER_PORT 4533
#define DEFAULT_WEBSERVER_PORT 80
#define DEFAULT_POSITION_UPDATE_INTERVAL 10
#define DEFAULT_POTI_TOLERANCE 2
#define DEFAULT_NUM_READINGS 64

// EEPROM addresses for storing configuration
#define TCP_SERVER_PORT_ADDR 0          // 2 Bytes (uint16_t)
#define POSITION_UPDATE_INTERVAL_ADDR 2 // 2 Bytes (uint16_t)
#define AZIMUTH_HOME_ADDR 4             // 4 Bytes (double -> uint32_t)
#define AZIMUTH_MIN_ADDR 8              // 4 Bytes (double -> uint32_t)
#define AZIMUTH_MAX_ADDR 12             // 4 Bytes (double -> uint32_t)
#define ELEVATION_HOME_ADDR 16          // 4 Bytes (double -> uint32_t)
#define ELEVATION_MIN_ADDR 20           // 4 Bytes (double -> uint32_t)
#define ELEVATION_MAX_ADDR 24           // 4 Bytes (double -> uint32_t)
#define POTI_TOLERANCE_ADDR 28          // 2 Bytes (uint16_t)
#define NUM_READINGS_ADDR 30            // 2 Bytes (uint16_t)
#define WEBSERVER_PORT_ADDR 32          // 2 Bytes (uint16_t)

// Variables to store configuration
int tcpServerPort = DEFAULT_TCP_SERVER_PORT;
int webServerPort = DEFAULT_WEBSERVER_PORT;
int positionUpdateInterval = DEFAULT_POSITION_UPDATE_INTERVAL;
int potiTolerance = DEFAULT_POTI_TOLERANCE;
int numReadings = DEFAULT_NUM_READINGS;

Rotor rotorAzimuth(0.00, AZIMUTH_HOME_ADDR, 0.00, AZIMUTH_MIN_ADDR, 0.00, AZIMUTH_MAX_ADDR, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_2, potiTolerance, numReadings);
Rotor rotorElevation(0.00, ELEVATION_HOME_ADDR, 0.00, ELEVATION_MIN_ADDR, 0.00, ELEVATION_MAX_ADDR, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_2, potiTolerance, numReadings);

WiFiServer server(tcpServerPort);
WebServer webServer(webServerPort);

unsigned long lastPositionUpdate = 0;
std::vector<WiFiClient> clients;

void saveConfig();

void splitString(const String &input, std::vector<String> &output)
{
    int start = 0;
    int end;
    while ((end = input.indexOf(' ', start)) != -1)
    {
        output.push_back(input.substring(start, end));
        start = end + 1;
    }
    output.push_back(input.substring(start));
}

static String getRotorCurrentPosition()
{
    return String(rotorAzimuth.getCurrent(), 2) + "\n" + String(rotorElevation.getCurrent(), 2) + "\nRPRT 0\n";
}

static String setRotorPosition(double azimuth, double elevation)
{
    rotorAzimuth.setTarget(azimuth);
    rotorElevation.setTarget(elevation);
    return "RPRT 0\n";
}

static String stopRotor()
{
    rotorAzimuth.setTarget(rotorAzimuth.getCurrent());
    rotorElevation.setTarget(rotorElevation.getCurrent());
    return "RPRT 0\n";
}

static String homeRotor()
{
    rotorAzimuth.moveHome();
    rotorElevation.moveHome();
    return "RPRT 0\n";
}

static String resetRotor()
{
    rotorAzimuth.reset();
    rotorElevation.reset();
    return "RPRT 0\n";
}

static String getDumpState()
{
    return "min_az=-180.00\nmax_az=180.00\nmin_el=-90.00\nmax_el=90.00\nsouth_zero=10.00\nRPRT 0\n";
}

void processCommand(String command, WiFiClient &client)
{
    String toSendString = "";
    std::vector<String> output;

    command.trim();
    if (command[0] == '/' || command[0] == '+')
        command.remove(0, 1);

    if (!command.isEmpty())
    {
        Serial.println("Received command: " + command);
        splitString(command, output);

        if (output[0] == "p")
        {
            toSendString = getRotorCurrentPosition();
        }
        else if (output[0] == "P")
        {
            if (output.size() == 3)
            {
                output[1].replace(',', '.');
                output[2].replace(',', '.');
                toSendString = setRotorPosition(output[1].toDouble(), output[2].toDouble());
            }
            else
            {
                toSendString = "ERR Invalid command length\nRPRT -8\n";
            }
        }
        else if (output[0] == "S")
        {
            toSendString = stopRotor();
        }
        else if (output[0] == "_")
        {
            toSendString = "Model Name: ESP32 Rotor Controller Az/El\nRPRT 0\n";
        }
        else if (output[0] == "q")
        {
            client.stop();
            return;
        }
        else if (output[0] == "dump_state")
        {
            toSendString = getDumpState();
        }
        else
        {
            toSendString = "ERR Unknown command\nRPRT -1\n";
        }

        if (client.connected())
        {
            client.print(toSendString);
        }
    }
}

void updatePosition()
{
    if (millis() - lastPositionUpdate >= positionUpdateInterval)
    {
        lastPositionUpdate = millis();
        rotorAzimuth.updatePosition();
        rotorElevation.updatePosition();
    }
}

void handleClients()
{
    WiFiClient newClient = server.available();
    if (newClient)
    {
        clients.push_back(newClient);
    }

    for (int i = clients.size() - 1; i >= 0; i--)
    {
        WiFiClient &client = clients[i];
        if (!client.connected())
        {
            clients.erase(clients.begin() + i);
            continue;
        }

        if (client.available() > 0)
        {
            String command = client.readStringUntil('\n');
            processCommand(command, client);
        }
    }
}

uint16_t readIntFromEEPROM(int address)
{
    return EEPROM.read(address) << 8 | EEPROM.read(address + 1);
}

void writeIntToEEPROM(int address, uint16_t value)
{
    EEPROM.write(address, value >> 8);
    EEPROM.write(address + 1, value & 0xFF);
}

double readDoubleFromEEPROM(int address)
{
    uint32_t intValue = ((uint32_t)EEPROM.read(address) << 24) |
                        ((uint32_t)EEPROM.read(address + 1) << 16) |
                        ((uint32_t)EEPROM.read(address + 2) << 8) |
                        EEPROM.read(address + 3);
    return intValue / 100.0;
}

void writeDoubleToEEPROM(int address, double value)
{
    uint32_t intValue = value * 100;
    EEPROM.write(address, (intValue >> 24) & 0xFF);
    EEPROM.write(address + 1, (intValue >> 16) & 0xFF);
    EEPROM.write(address + 2, (intValue >> 8) & 0xFF);
    EEPROM.write(address + 3, intValue & 0xFF);
    EEPROM.commit();
}

void printConfigAsTable(String comment)
{
    Serial.println(comment);
    Serial.println("+-------------------------+----------------------+----------------------+");
    Serial.println("|       Parameter         |      Stored Value    |    Current Value     |");
    Serial.println("+-------------------------+----------------------+----------------------+");

    Serial.printf("| %-23s | %20d | %20d |\n", "TCP Server Port", readIntFromEEPROM(TCP_SERVER_PORT_ADDR), tcpServerPort);
    Serial.printf("| %-23s | %20d | %20d |\n", "Web Server Port", readIntFromEEPROM(WEBSERVER_PORT_ADDR), webServerPort);
    Serial.printf("| %-23s | %20d | %20d |\n", "Update Interval", readIntFromEEPROM(POSITION_UPDATE_INTERVAL_ADDR), positionUpdateInterval);
    Serial.printf("| %-23s | %20d | %20d |\n", "Poti Tolerance", readIntFromEEPROM(POTI_TOLERANCE_ADDR), potiTolerance);
    Serial.printf("| %-23s | %20d | %20d |\n", "Number of Readings", readIntFromEEPROM(NUM_READINGS_ADDR), numReadings);
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Azimuth Home", readDoubleFromEEPROM(rotorAzimuth.getHomeAddr()), rotorAzimuth.getHome());
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Azimuth Min", readDoubleFromEEPROM(rotorAzimuth.getMinAddr()), rotorAzimuth.getMin());
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Azimuth Max", readDoubleFromEEPROM(rotorAzimuth.getMaxAddr()), rotorAzimuth.getMax());
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Elevation Home", readDoubleFromEEPROM(rotorElevation.getHomeAddr()), rotorElevation.getHome());
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Elevation Min", readDoubleFromEEPROM(rotorElevation.getMinAddr()), rotorElevation.getMin());
    Serial.printf("| %-23s | %20.2f | %20.2f |\n", "Elevation Max", readDoubleFromEEPROM(rotorElevation.getMaxAddr()), rotorElevation.getMax());

    Serial.println("+-------------------------+----------------------+----------------------+");
}

void loadConfig()
{
    bool save = false;

    EEPROM.begin(512);

    tcpServerPort = readIntFromEEPROM(TCP_SERVER_PORT_ADDR);
    if (tcpServerPort < 1 || tcpServerPort > 65535)
    {
        tcpServerPort = DEFAULT_TCP_SERVER_PORT;
        save = true;
    }

    webServerPort = readIntFromEEPROM(WEBSERVER_PORT_ADDR);
    if (webServerPort < 1 || webServerPort > 65535)
    {
        webServerPort = DEFAULT_WEBSERVER_PORT;
        save = true;
    }

    positionUpdateInterval = readIntFromEEPROM(POSITION_UPDATE_INTERVAL_ADDR);
    if (positionUpdateInterval < 1)
    {
        positionUpdateInterval = DEFAULT_POSITION_UPDATE_INTERVAL;
        save = true;
    }

    potiTolerance = readIntFromEEPROM(POTI_TOLERANCE_ADDR);
    if (potiTolerance < 0)
    {
        potiTolerance = DEFAULT_POTI_TOLERANCE;
        save = true;
    }

    numReadings = readIntFromEEPROM(NUM_READINGS_ADDR);
    if (numReadings < 1)
    {
        numReadings = DEFAULT_NUM_READINGS;
        save = true;
    }

    rotorAzimuth.setHome(readDoubleFromEEPROM(rotorAzimuth.getHomeAddr()));
    if (rotorAzimuth.getHome() < -360.0 || rotorAzimuth.getHome() > 360.0)
    {
        rotorAzimuth.setHome(0.0);
        save = true;
    }

    rotorAzimuth.setMin(readDoubleFromEEPROM(rotorAzimuth.getMinAddr()));
    if (rotorAzimuth.getMin() < -360.0 || rotorAzimuth.getMin() > 360.0)
    {
        rotorAzimuth.setMin(0.0);
        save = true;
    }

    rotorAzimuth.setMax(readDoubleFromEEPROM(rotorAzimuth.getMaxAddr()));
    if (rotorAzimuth.getMax() < -360.0 || rotorAzimuth.getMax() > 360.0)
    {
        rotorAzimuth.setMax(0.0);
        save = true;
    }

    rotorElevation.setHome(readDoubleFromEEPROM(rotorElevation.getHomeAddr()));
    if (rotorElevation.getHome() < -360.0 || rotorElevation.getHome() > 360.0)
    {
        rotorElevation.setHome(0.0);
        save = true;
    }

    rotorElevation.setMin(readDoubleFromEEPROM(rotorElevation.getMinAddr()));
    if (rotorElevation.getMin() < -360.0 || rotorElevation.getMin() > 360.0)
    {
        rotorElevation.setMin(0.0);
        save = true;
    }

    rotorElevation.setMax(readDoubleFromEEPROM(rotorElevation.getMaxAddr()));
    if (rotorElevation.getMax() < -360.0 || rotorElevation.getMax() > 360.0)
    {
        rotorElevation.setMax(0.0);
        save = true;
    }

    if (save)
    {
        saveConfig();
    }

    printConfigAsTable("loadConfig");
}

void saveConfig()
{
    EEPROM.begin(512);

    if (tcpServerPort != readIntFromEEPROM(TCP_SERVER_PORT_ADDR))
    {
        writeIntToEEPROM(TCP_SERVER_PORT_ADDR, tcpServerPort);
    }

    if (webServerPort != readIntFromEEPROM(WEBSERVER_PORT_ADDR))
    {
        writeIntToEEPROM(WEBSERVER_PORT_ADDR, webServerPort);
    }

    if (positionUpdateInterval != readIntFromEEPROM(POSITION_UPDATE_INTERVAL_ADDR))
    {
        writeIntToEEPROM(POSITION_UPDATE_INTERVAL_ADDR, positionUpdateInterval);
    }

    if (potiTolerance != EEPROM.read(POTI_TOLERANCE_ADDR))
    {
        writeIntToEEPROM(POTI_TOLERANCE_ADDR, potiTolerance);
    }

    if (numReadings != EEPROM.read(NUM_READINGS_ADDR))
    {
        writeIntToEEPROM(NUM_READINGS_ADDR, numReadings);
    }

    if (rotorAzimuth.getHome() != readDoubleFromEEPROM(rotorAzimuth.getHomeAddr()))
    {
        writeDoubleToEEPROM(rotorAzimuth.getHomeAddr(), rotorAzimuth.getHome());
    }

    if (rotorElevation.getHome() != readDoubleFromEEPROM(rotorElevation.getHomeAddr()))
    {
        writeDoubleToEEPROM(rotorElevation.getHomeAddr(), rotorElevation.getHome());
    }

    if (rotorAzimuth.getMin() != readDoubleFromEEPROM(rotorAzimuth.getMinAddr()))
    {
        writeDoubleToEEPROM(rotorAzimuth.getMinAddr(), rotorAzimuth.getMin());
    }

    if (rotorAzimuth.getMax() != readDoubleFromEEPROM(rotorAzimuth.getMaxAddr()))
    {
        writeDoubleToEEPROM(rotorAzimuth.getMaxAddr(), rotorAzimuth.getMax());
    }

    if (rotorElevation.getMin() != readDoubleFromEEPROM(rotorElevation.getMinAddr()))
    {
        writeDoubleToEEPROM(rotorElevation.getMinAddr(), rotorElevation.getMin());
    }

    if (rotorElevation.getMax() != readDoubleFromEEPROM(rotorElevation.getMaxAddr()))
    {
        writeDoubleToEEPROM(rotorElevation.getMaxAddr(), rotorElevation.getMax());
    }

    EEPROM.commit();

    printConfigAsTable("saveConfig");
}

void setupWebInterface()
{
    webServer.on("/", HTTP_GET, []()
                 {
        String filePath = "/index.html";
        if (SPIFFS.exists(filePath)) {
            File file = SPIFFS.open(filePath, "r");
            String html = file.readString();
            file.close();
        
            html.replace("%AZIMUTH_HOME%", String(rotorAzimuth.getHome(), 2));
            html.replace("%ELEVATION_HOME%", String(rotorElevation.getHome(), 2));
            webServer.send(200, "text/html", html);
        } else {
        webServer.send(404, "text/plain", "File not found");
        } });

    webServer.on("/api/set_position", HTTP_GET, []()
                 {
        if (webServer.hasArg("azimuth") && webServer.hasArg("elevation")) {
        double azimuth = webServer.arg("azimuth").toDouble();
        double elevation = webServer.arg("elevation").toDouble();
        setRotorPosition(azimuth, elevation);
        webServer.send(200, "text/plain", "Position set successfully.");
        } else {
        webServer.send(400, "text/plain", "Missing azimuth or elevation parameters.");
        } });

    webServer.on("/api/stop", HTTP_GET, []()
                 {
        stopRotor();
        webServer.send(200, "text/plain", "Rotor stopped."); });

    webServer.on("/api/coordinates", HTTP_GET, []()
                 {
        String json = "{\"azimuth\":\"" + String(rotorAzimuth.getCurrent(), 2) + "\","
                    "\"azimuthTarget\":\"" + String(rotorAzimuth.getTarget(), 2) + "\","
                    "\"elevation\":\"" + String(rotorElevation.getCurrent(), 2) + "\","
                    "\"elevationTarget\":\"" + String(rotorElevation.getTarget(), 2) + "\"}";
        webServer.send(200, "application/json", json); });

    webServer.on("/api/current-config", HTTP_GET, []()
                 {
        printConfigAsTable("/api/current-config");

        String json = "{";
        json += "\"tcp_server_port\":" + String(tcpServerPort) + ",";
        json += "\"web_server_port\":" + String(webServerPort) + ",";
        json += "\"position_update_interval\":" + String(positionUpdateInterval) + ",";
        json += "\"poti_tolerance\":" + String(potiTolerance) + ",";
        json += "\"num_readings\":" + String(numReadings) + ",";
        json += "\"azimuth_home\":" + String(rotorAzimuth.getHome(), 2) + ",";
        json += "\"azimuth_min\":" + String(rotorAzimuth.getMin(), 2) + ",";
        json += "\"azimuth_max\":" + String(rotorAzimuth.getMax(), 2) + ",";
        json += "\"elevation_home\":" + String(rotorElevation.getHome(), 2) + ",";
        json += "\"elevation_min\":" + String(rotorElevation.getMin(), 2) + ",";
        json += "\"elevation_max\":" + String(rotorElevation.getMax(), 2);
        json += "}";

        webServer.send(200, "application/json", json); });

    webServer.on("/configure", HTTP_GET, []()
                 {
    if (SPIFFS.exists("/configure.html")) {
        File file = SPIFFS.open("/configure.html", "r");
        String html = file.readString();
        file.close();

        webServer.send(200, "text/html", html);
    } else {
        webServer.send(404, "text/plain", "File not found");
    } });

    webServer.on("/configure", HTTP_POST, []()
                 {
    if (webServer.hasArg("tcp_server_port") && webServer.hasArg("position_update_interval") && webServer.hasArg("azimuth_home") && webServer.hasArg("elevation_home")) {
        tcpServerPort = webServer.arg("tcp_server_port").toInt();
        positionUpdateInterval = webServer.arg("position_update_interval").toInt();
        rotorAzimuth.setHome(webServer.arg("azimuth_home").toDouble());
        rotorElevation.setHome(webServer.arg("elevation_home").toDouble());
        potiTolerance = webServer.arg("poti_tolerance").toInt();
        webServerPort = webServer.arg("web_server_port").toInt();
        numReadings = webServer.arg("num_readings").toInt();

        saveConfig();

        server.close();
        server = WiFiServer(tcpServerPort);
        server.begin();

        webServer.sendHeader("Location", "/configure", true);
        webServer.send(302, "text/plain", ""); 
    } else {
        if (SPIFFS.exists("/config_error.html")) {
            File file = SPIFFS.open("/config_error.html", "r");
            webServer.streamFile(file, "text/html");
            file.close();
        } else {
            webServer.send(404, "text/plain", "File not found");
        }
    } });

    webServer.on("/api/findMin", HTTP_GET, []()
                 {
        if (webServer.hasArg("poti")) {
            int potiId = webServer.arg("poti").toInt();            
            Serial.println(potiId);
            if(potiId == 0) {
                rotorAzimuth.findMin();
                webServer.send(200, "text/plain", "Min value saved.");
            }
            else if(potiId == 1) {
                rotorElevation.findMin();
                webServer.send(200, "text/plain", "Min value saved.");
            } else {
                webServer.send(400, "text/plain", "Invalid poti ID.");
            }
        } else {
            webServer.send(400, "text/plain", "Missing poti parameter.");
    } });

    webServer.on("/api/findMax", HTTP_GET, []()
                 {
        if (webServer.hasArg("poti")) {
            
            int potiId = webServer.arg("poti").toInt();
            
            if(potiId == 0) {
                rotorAzimuth.findMax();
                webServer.send(200, "text/plain", "Min value saved.");
            }
            else if(potiId == 1) {
                rotorElevation.findMax();
                webServer.send(200, "text/plain", "Min value saved.");
            } else {
                webServer.send(400, "text/plain", "Invalid poti ID.");
            }
        } else {
            webServer.send(400, "text/plain", "Missing poti parameter.");
    } });

    webServer.begin();
    Serial.printf("\nTCP Server started on port %d\n", tcpServerPort);
}

void setup()
{
    Serial.begin(115200);

    loadConfig();

    rotorAzimuth.initialize();
    rotorElevation.initialize();

    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }

    wifiManager.autoConnect();
    server.begin();
    setupWebInterface();
}

void loop()
{
    updatePosition();
    handleClients();
    webServer.handleClient();
}
