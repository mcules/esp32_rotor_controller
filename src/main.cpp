#include <WiFi.h>
#include <WiFiManager.h>
#include <vector>
#include <WebServer.h>
#include <EEPROM.h>

WiFiManager wifiManager;

// Define default values
#define DEFAULT_TCP_SERVER_PORT 4533
#define DEFAULT_WEBSERVER_PORT 80
#define DEFAULT_POSITION_UPDATE_INTERVAL 10

// EEPROM addresses for storing configuration
#define TCP_SERVER_PORT_ADDR 0
#define POSITION_UPDATE_INTERVAL_ADDR 2

// Variables to store configuration
int tcpServerPort = DEFAULT_TCP_SERVER_PORT;
int webServerPort = DEFAULT_WEBSERVER_PORT;
int positionUpdateInterval = DEFAULT_POSITION_UPDATE_INTERVAL;

double currentAzimuth = 0.0;
double currentElevation = 0.0;

double targetAzimuth = 0.0;
double targetElevation = 0.0;

WiFiServer server(tcpServerPort);
WebServer webServer(webServerPort);

unsigned long lastPositionUpdate = 0;
std::vector<WiFiClient> clients;

// Function to split a string by spaces
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

// Function to get the current rotor position
static String getRotorCurrentPosition()
{
    return String(currentAzimuth, 2) + "\n" + String(currentElevation, 2) + "\nRPRT 0\n";
}

// Function to set the target rotor position
static String setRotorPosition(double azimuth, double elevation)
{
    targetAzimuth = azimuth;
    targetElevation = elevation;
    return "RPRT 0\n";
}

// Function to stop the rotor
static String stopRotor()
{
    targetAzimuth = currentAzimuth;
    targetElevation = currentElevation;
    return "RPRT 0\n";
}

// Function to get the dump state
static String getDumpState()
{
    return "min_az=-180.00\nmax_az=180.00\nmin_el=-90.00\nmax_el=90.00\nsouth_zero=10.00\nRPRT 0\n";
}

// Function to process commands received from clients
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

// Function to update the rotor position
void updatePosition()
{
    if (millis() - lastPositionUpdate >= positionUpdateInterval)
    {
        lastPositionUpdate = millis();

        if (targetAzimuth > currentAzimuth)
            currentAzimuth += 0.1;
        if (targetAzimuth < currentAzimuth)
            currentAzimuth -= 0.1;

        if (targetElevation > currentElevation)
            currentElevation += 0.1;
        if (targetElevation < currentElevation)
            currentElevation -= 0.1;
    }
}

// Function to handle client connections
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

// Function to load configuration from EEPROM
void loadConfig()
{
    EEPROM.begin(4);
    tcpServerPort = EEPROM.read(TCP_SERVER_PORT_ADDR) << 8 | EEPROM.read(TCP_SERVER_PORT_ADDR + 1);
    positionUpdateInterval = EEPROM.read(POSITION_UPDATE_INTERVAL_ADDR) << 8 | EEPROM.read(POSITION_UPDATE_INTERVAL_ADDR + 1);

    // If the values are invalid, use defaults
    if (tcpServerPort < 1 || tcpServerPort > 65535)
    {
        tcpServerPort = DEFAULT_TCP_SERVER_PORT;
    }
    if (positionUpdateInterval < 1)
    {
        positionUpdateInterval = DEFAULT_POSITION_UPDATE_INTERVAL;
    }
}

// Function to save configuration to EEPROM
void saveConfig()
{
    EEPROM.write(TCP_SERVER_PORT_ADDR, tcpServerPort >> 8);
    EEPROM.write(TCP_SERVER_PORT_ADDR + 1, tcpServerPort & 0xFF);
    EEPROM.write(POSITION_UPDATE_INTERVAL_ADDR, positionUpdateInterval >> 8);
    EEPROM.write(POSITION_UPDATE_INTERVAL_ADDR + 1, positionUpdateInterval & 0xFF);
    EEPROM.commit();
}

// Function to set up the web interface
void setupWebInterface()
{
    webServer.on("/", HTTP_GET, []()
                 {
                     String html = "<!DOCTYPE html>"
                                   "<html>"
                                   "<head>"
                                   "<meta charset='utf-8'>" // Explicitly set character encoding to UTF-8
                                   "<title>ESP32 Rotor Controller</title>"
                                   "<style>"
                                   "body { font-family: sans-serif; text-align: center; }"
                                   "h1 { margin-bottom: 20px; }"
                                   ".container { display: flex; flex-direction: column; align-items: center;  }"
                                   ".data { display: flex; flex-direction: column; align-items: center; margin-bottom: 20px; }"
                                   ".coordinates { font-size: 24px; margin-bottom: 10px; }"
                                   ".input-group { display: flex; align-items: center; margin-bottom: 10px; }"
                                   "input[type='number'] { width: 80px; margin-right: 10px; }"
                                   "button { padding: 8px 16px; background-color: #4CAF50; color: white; border: none; }"
                                   "</style>"
                                   "</head>"
                                   "<body>"
                                   "<h1>ESP32 Rotor Controller</h1>"
                                   "<div class='container'>"
                                   "<div class='data'>"
                                   "<div class='coordinates'>Azimuth: <span id='azimuth'>" +
                                   String(currentAzimuth, 2) + "</span>&deg;"
                                                               " (<span id='targetAzimuth'>" +
                                   String(targetAzimuth, 2) + "</span>&deg;)</div>"
                                                              "<div class='coordinates'>Elevation: <span id='elevation'>" +
                                   String(currentElevation, 2) + "</span>&deg;"
                                                                 " (<span id='targetElevation'>" +
                                   String(targetElevation, 2) + "</span>&deg;)</div>"
                                                                "<form id='set-position-form'>"
                                                                "<div class='input-group'>"
                                                                "<label for='azimuthInput'>Azimuth:</label>"
                                                                "<input type='number' id='azimuthInput' name='azimuth' min='-180' max='180' step='0.1' value='" +
                                   String(targetAzimuth, 2) + "'>"
                                                              "</div>"
                                                              "<div class='input-group'>"
                                                              "<label for='elevationInput'>Elevation:</label>"
                                                              "<input type='number' id='elevationInput' name='elevation' min='-90' max='90' step='0.1' value='" +
                                   String(targetElevation, 2) + "'>"
                                                                "</div>"
                                                                "<button type='submit'>Set Position</button>"
                                                                "</form>"
                                                                "<a href='/configure'>Configure settings</a>"
                                                                "</div>"
                                                                "</div>"
                                                                "<script>"
                                                                "setInterval(function() {"
                                                                "  fetch('/coordinates')"
                                                                "  .then(response => response.json())"
                                                                "  .then(data => {"
                                                                "    document.getElementById('azimuth').textContent = data.azimuth;"
                                                                "    document.getElementById('targetAzimuth').textContent = data.targetAzimuth;"
                                                                "    document.getElementById('elevation').textContent = data.elevation;"
                                                                "    document.getElementById('targetElevation').textContent = data.targetElevation;"
                                                                "  });"
                                                                "}, 1000);"

                                                                "document.getElementById('set-position-form').addEventListener('submit', function(event) {"
                                                                "  event.preventDefault();"
                                                                "  var azimuth = document.getElementById('azimuthInput').value;"
                                                                "  var elevation = document.getElementById('elevationInput').value;"
                                                                "  fetch('/set_position?azimuth=' + azimuth + '&elevation=' + elevation)"
                                                                "  .then(response => response.text())"
                                                                "  .then(data => console.log(data));"
                                                                "});"
                                                                "</script>"
                                                                "</body>"
                                                                "</html>";
                     webServer.send(200, "text/html; charset=utf-8", html); // Set UTF-8 encoding in header
                 });

    // Endpoint to handle setting the position
    webServer.on("/set_position", HTTP_GET, []()
                 {
    if (webServer.hasArg("azimuth") && webServer.hasArg("elevation")) {
      double azimuth = webServer.arg("azimuth").toDouble();
      double elevation = webServer.arg("elevation").toDouble();
      setRotorPosition(azimuth, elevation); // Call your existing function to set the position
      webServer.send(200, "text/plain", "Position set successfully.");
    } else {
      webServer.send(400, "text/plain", "Missing azimuth or elevation parameters.");
    } });

    // Endpoint to provide coordinate data
    webServer.on("/coordinates", HTTP_GET, []()
                 {
    String json = "{\"azimuth\":\"" + String(currentAzimuth, 2) + "\","
                  "\"targetAzimuth\":\"" + String(targetAzimuth, 2) + "\","
                  "\"elevation\":\"" + String(currentElevation, 2) + "\","
                  "\"targetElevation\":\"" + String(targetElevation, 2) + "\"}";
    webServer.send(200, "application/json", json); });

    webServer.on("/configure", HTTP_GET, []()
                 {
    String form = "<h1>Configure Settings</h1>"
                  "<form method='POST' action='/configure'>"
                  "<label for='tcp_server_port'>TCP Server Port:</label><br>"
                  "<input type='number' id='tcp_server_port' name='tcp_server_port' value='" +
                  String(tcpServerPort) + "' required><br><br>"
                  "<label for='position_update_interval'>Position Update Interval (ms):</label><br>"
                  "<input type='number' id='position_update_interval' name='position_update_interval' value='" +
                  String(positionUpdateInterval) + "' required><br><br>"
                  "<input type='submit' value='Update'>"
                  "</form>";
    webServer.send(200, "text/html", form); });

    webServer.on("/configure", HTTP_POST, []()
                 {
    if (webServer.hasArg("tcp_server_port") && webServer.hasArg("position_update_interval"))
    {
      tcpServerPort = webServer.arg("tcp_server_port").toInt();
      positionUpdateInterval = webServer.arg("position_update_interval").toInt();

      // Save the updated configuration to EEPROM
      saveConfig();

      // Restart the server with the new port
      server.close();
      server = WiFiServer(tcpServerPort);
      server.begin();

      String response = "<h1>Configuration Updated</h1>"
                        "<p>TCP Server Port: " + String(tcpServerPort) + "</p>"
                        "<p>Position Update Interval: " + String(positionUpdateInterval) + " ms</p>"
                        "<p>Changes will be applied after restart.</p>"
                        "<p><a href='/'>Return to Home</a></p>";
      webServer.send(200, "text/html", response);
    }
    else
    {
      webServer.send(400, "text/html",
                     "<h1>Error: Missing Parameters</h1>"
                     "<p>Please provide both <i>tcp_server_port</i> and <i>position_update_interval</i>.</p>"
                     "<p><a href='/configure'>Go Back</a></p>");
    } });

    webServer.begin();
    Serial.printf("\nWebserver started on port %d\n", webServerPort);
}

void setup()
{
    Serial.begin(115200);

    // Load configuration from EEPROM
    loadConfig();

    wifiManager.autoConnect();
    server.begin();
    setupWebInterface();
    Serial.printf("\nTCP Server started on port %d\n", tcpServerPort);
}

void loop()
{
    updatePosition();
    handleClients();
    webServer.handleClient();
}