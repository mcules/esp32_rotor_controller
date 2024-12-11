#include <WiFi.h>
#include <WiFiManager.h>
#include <vector>

WiFiManager wifiManager;

#define TCP_SERVER_PORT 4533

double currentAzimuth = 0.0;
double currentElevation = 0.0;

double targetAzimuth = 0.0;
double targetElevation = 0.0;

char delimiter = ' ';

WiFiServer server(TCP_SERVER_PORT);

void splitString(String input, char delimiter, std::vector<String> &output)
{
    int start = 0;

    while (true)
    {
        int end = input.indexOf(delimiter, start);

        if (end == -1)
        {
            output.push_back(input.substring(start));
            break;
        }
        else
        {
            output.push_back(input.substring(start, end));
            start = end + 1;
        }
    }
}

static String getRotorCurrentPosition()
{
    Serial.printf("get_pos %f %f\n", currentAzimuth, currentElevation);

    return String(currentAzimuth, 2) + "\n" + String(currentElevation, 2) + "\n";
}

static String setRotorPosition(double azimuth, double elevation)
{
    targetAzimuth = azimuth;
    targetElevation = elevation;

    Serial.printf("set_pos %f %f\n", azimuth, elevation);

    return "RPRT 0\n";
}

static String stopRotor()
{
    // TODO: needs implementation
    return "RPRT 0\n";
}

void setup()
{
    Serial.begin(115200);

    wifiManager.autoConnect();

    server.begin();
    Serial.printf("\n Server started on Port %d\n", TCP_SERVER_PORT);
}

void handleClient(WiFiClient client)
{
    while (client.connected())
    {
        String command = client.readStringUntil('\n');
        command.trim();

        std::vector<String> output;
        splitString(command, delimiter, output);

        String toSendString = "";

        if (output[0] == "p" || output[0] == "get_pos")
        {
            toSendString = getRotorCurrentPosition();
        }
        else if (output[0] == "P" || output[0] == "set_pos")
        {
            if (output.size() == 3)
            {
                toSendString = setRotorPosition(output[1].toDouble(), output[2].toDouble());
            }
            else
            {
                toSendString = "ERR Invalid command length\n";
            }
        }
        else if (output[0] == "S" || output[0] == "stop")
        {
            toSendString = stopRotor();
        }
        else if (output[0] == "K" || output[0] == "park")
        {
            setRotorPosition(0.0, 0.0);
        }

        Serial.println(toSendString);

        if (client.connected())
        {
            client.println(toSendString);
        }
    }
    client.stop();
    Serial.println("Client disconnected");
}

void loop()
{
    WiFiClient client = server.available();

    if (client)
    {
        Serial.println("New client connected");
        handleClient(client);
    }

    if (targetAzimuth > currentAzimuth)
    {
        currentAzimuth += 0.00005;
    }
    else if (targetAzimuth < currentAzimuth)
    {
        currentAzimuth -= 0.00005;
    }

    if (targetElevation > currentElevation)
    {
        currentElevation += 0.00005;
    }
    else if (targetElevation < currentElevation)
    {
        currentElevation -= 0.00005;
    }
}
