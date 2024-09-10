#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <time.h>

const char* targetSSID = "YourHomeNetworkHere";              // Name of your home WiFi network *it's optional just to log time and date
const char* targetPassword = "YourHomeNetworkPasswordHere";  // Password for your home WiFi network

ESP8266WebServer server(80);

// Pin to which the built-in LED is connected
const int ledPin = LED_BUILTIN; // Built-in LED on NodeMCU

struct Network {
  String ssid;
  int rssi;
  int channel;
  uint8_t encryptionType;
  String bssid;
};

bool compareBySignalStrength(Network a, Network b) {
  return a.rssi > b.rssi; // Sort in descending order: better signal strength on top
}

String getTimestamp() {
  time_t now = time(nullptr);
  char timestamp[20];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
  return String(timestamp);
}

int getLastIndex() {
  int lastIndex = 0;
  File logFile = SPIFFS.open("/networks.txt", "r");
  if (logFile) {
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      if (line.startsWith("| ")) {
        int indexStart = line.indexOf('|') + 2;
        int indexEnd = line.indexOf('|', indexStart);
        String indexStr = line.substring(indexStart, indexEnd);
        indexStr.trim();  // Trim unnecessary spaces and characters
        int index = indexStr.toInt();
        if (index > lastIndex) {
          lastIndex = index;
        }
      }
    }
    logFile.close();
  }
  return lastIndex;
}

void logNetwork(const Network& network, bool logTimestamp) {
  static int currentIndex = getLastIndex() + 1;

  File logFile = SPIFFS.open("/networks.txt", "a");
  if (logFile) {
    // Table header (only on the first entry)
    if (logFile.size() == 0) {
      logFile.println("| Index | Timestamp | SSID | RSSI (dBm) | Channel | Encryption | BSSID |");
      logFile.println("|-------|-----------|------|------------|---------|------------|-------|");
    }
    
    // Log data
    logFile.print("| ");
    logFile.print(currentIndex);
    logFile.print(" | ");
    if (logTimestamp) {
      logFile.print(getTimestamp());
    } else {
      logFile.print("N/A");
    }
    logFile.print(" | ");
    logFile.print(network.ssid);
    logFile.print(" | ");
    logFile.print(network.rssi);
    logFile.print(" dBm | ");
    logFile.print(network.channel);
    logFile.print(" | ");
    logFile.print(getEncryptionTypeString(network.encryptionType));
    logFile.print(" | ");
    logFile.print(network.bssid);
    logFile.println(" |");
    logFile.close();

    currentIndex++;
  }
}

bool networkExists(const String& ssid) {
  File logFile = SPIFFS.open("/networks.txt", "r");
  if (logFile) {
    while (logFile.available()) {
      String line = logFile.readStringUntil('\n');
      if (line.indexOf(ssid) != -1) {
        logFile.close();
        return true;
      }
    }
    logFile.close();
  }
  return false;
}

bool connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  int timeout = 10; // Timeout in seconds
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    timeout--;
  }
  return WiFi.status() == WL_CONNECTED;
}

String getEncryptionTypeString(uint8_t encryptionType) {
  switch (encryptionType) {
    case ENC_TYPE_NONE: return "Open";
    case ENC_TYPE_WEP: return "WEP";
    case ENC_TYPE_TKIP: return "WPA/TKIP";
    case ENC_TYPE_CCMP: return "WPA2/CCMP";
    case ENC_TYPE_AUTO: return "WPA/WPA2";
    default: return "Unknown";
  }
}

String scanNetworks() {
  String response = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  response += "<meta http-equiv='refresh' content='5'>"; // Add meta tag for refreshing every 5 seconds
  response += "<style>";
  response += "body { font-family: Arial, sans-serif; }";
  response += "table { width: 100%; border-collapse: collapse; }";
  response += "th, td { border: 1px solid #ddd; padding: 8px; }";
  response += "th { padding-top: 12px; padding-bottom: 12px; text-align: left; background-color: #4CAF50; color: white; }";
  response += "tr:nth-child(even) { background-color: #f2f2f2; }";
  response += "tr:hover { background-color: #ddd; }";
  response += ".chart { display: flex; justify-content: space-around; align-items: flex-end; height: 200px; border-bottom: 1px solid #ddd; margin-top: 20px; }";
  response += ".bar { width: 20px; background-color: #4CAF50; }";
  response += ".bar-label { text-align: center; margin-top: 5px; }";
  response += "</style></head><body>";
  response += "<h1>Available WiFi Networks:</h1>";
  response += "<table border='1'><tr><th>No.</th><th>SSID</th><th>Signal Strength (dBm)</th><th>Channel</th><th>Encryption</th><th>BSSID</th></tr>";

  int n = WiFi.scanNetworks();
  Network networks[n]; // Array to store scan results

  if (n == 0) {
    response += "<tr><td colspan='6'>No networks found</td></tr>";
  } else {
    // Collect scan results into an array
    for (int i = 0; i < n; ++i) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].rssi = WiFi.RSSI(i);
      networks[i].channel = WiFi.channel(i);
      networks[i].encryptionType = WiFi.encryptionType(i);
      networks[i].bssid = WiFi.BSSIDstr(i);
    }
    
    // Sort the array by signal strength
    std::sort(networks, networks + n, compareBySignalStrength);

    // Log new networks to the log file
    for (int i = 0; i < n; ++i) {
      if (!networkExists(networks[i].ssid)) {
        logNetwork(networks[i], WiFi.status() == WL_CONNECTED);
      }
    }

    // Generate HTML from the sorted array
    for (int i = 0; i < n; ++i) {
      response += "<tr>";
      response += "<td>" + String(i + 1) + "</td>";
      response += "<td>" + networks[i].ssid + "</td>";
      response += "<td>" + String(networks[i].rssi) + " dBm</td>";
      response += "<td>" + String(networks[i].channel) + "</td>";
      response += "<td>" + getEncryptionTypeString(networks[i].encryptionType) + "</td>";
      response += "<td>" + networks[i].bssid + "</td>";
      response += "</tr>";
    }
  }
  response += "</table>";

  // Add bar chart with numbering
  response += "<div class='chart'>";
  for (int i = 0; i < n; ++i) {
    int height = map(networks[i].rssi, -100, -40, 10, 200); // Map signal strength to bar height
    response += "<div>";
    response += "<div class='bar' style='height:" + String(height) + "px'></div>";
    response += "<div class='bar-label'>" + String(i + 1) + "</div>";
    response += "</div>";
  }
  response += "</div>";

  // Link to download log file
  response += "<br><a href='/download'>Download Networks Log</a>";

  response += "</body></html>";
  return response;
}

void handleRoot() {
  // Turn on LED when someone views the homepage
  digitalWrite(ledPin, LOW);
  String response = scanNetworks();
  server.send(200, "text/html", response);
  // Turn off LED after handling the request
  digitalWrite(ledPin, HIGH);
}

void handleDownload() {
  // Turn on LED when someone downloads a file
  digitalWrite(ledPin, LOW);
  if (SPIFFS.exists("/networks.txt")) {
    File logFile = SPIFFS.open("/networks.txt", "r");
    if (logFile) {
      server.streamFile(logFile, "text/plain");
      logFile.close();
      // Turn off LED after file transfer
      digitalWrite(ledPin, HIGH);
      return;
    }
  }
  server.send(404, "text/plain", "File not found");
  // Turn off LED if file was not found
  digitalWrite(ledPin, HIGH);
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);

  server.on("/", []() {
    // Turn on LED
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);

    server.send(200, "text/html", scanNetworks());

    // Turn off LED after response is sent
    digitalWrite(ledPin, HIGH);
  });

  // Initialize file system
  SPIFFS.begin();

  // Check if connecting to the target WiFi network is possible
  Serial.println("Trying to connect to target WiFi network...");
  if (connectToWiFi(targetSSID, targetPassword)) {
    // Configure time for Europe/Warsaw time zone
    configTime(3600, 3600, "pool.ntp.org"); // Add UTC +1 hour offset
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); // Set time zone to Europe/Warsaw
    Serial.println("Connected to target WiFi network and updated time.");
  } else {
    Serial.println("Failed to connect to target WiFi network.");
  }

  // Create access point
  WiFi.softAP("ESP_APNameHere", "ESP_APPasswordHere");

  server.on("/", handleRoot); // Handle HTTP requests to the homepage
  server.on("/download", handleDownload); // Handle file download requests
  server.begin(); // Start listening for connections

  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP()); // Display the access point's IP address
}

void loop() {
  server.handleClient(); // Handle HTTP requests
}