#include <WiFi.h>
#include <WiFiManager.h>
#include <vector>

WiFiManager wifiManager;

#define TCP_SERVER_PORT 4533
#define POSITION_UPDATE_INTERVAL 10 // Position update in ms

double currentAzimuth = 0.0;
double currentElevation = 0.0;

double targetAzimuth = 0.0;
double targetElevation = 0.0;

WiFiServer server(TCP_SERVER_PORT);

unsigned long lastPositionUpdate = 0;
std::vector<WiFiClient> clients;

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
    return String(currentAzimuth, 2) + "\n" + String(currentElevation, 2) + "\nRPRT 0\n";
}

static String setRotorPosition(double azimuth, double elevation)
{
    targetAzimuth = azimuth;
    targetElevation = elevation;
    return "RPRT 0\n";
}

static String stopRotor()
{
    targetAzimuth = currentAzimuth;
    targetElevation = currentElevation;

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
        else if (output[0] == "\\dump_state")
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
    if (millis() - lastPositionUpdate >= POSITION_UPDATE_INTERVAL)
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

void setup()
{
    Serial.begin(115200);
    wifiManager.autoConnect();
    server.begin();
    Serial.printf("\nServer started on Port %d\n", TCP_SERVER_PORT);
}

void loop()
{
    updatePosition();
    handleClients();
}
