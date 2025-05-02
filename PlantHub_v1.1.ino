// Lesson 10 Capstone Project
// @malonestar

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <DHT.h>
#include <LED.h>
#include <Plant.h>

#define MAX_CONVO 10
#define DHT_PIN 33
#define DHT_TYPE DHT11
#define PR_PIN 37
#define MST_PIN 32

String chatHistory[MAX_CONVO];
int chatIndex = 0;

const char* ssid = "my-ssid";
const char* password = "my-password!";
const char* openai_api_key = "my-api-key";

const long  gmtOffset_sec = -25200; // Mountain time
const int   daylightOffset_sec = 3600; // dst offset
const char* ntpServer = "pool.ntp.org";

unsigned long lastDhtReadTime = 0;
float cachedTemp = 0;
float cachedHumidity = 0;
String lastUpdatedTime = "";

const char header[] = R"rawliteral(
  <head>
    <title>
      PlantHub ESP32 Chat
    </title>
    <style>
      .table {
        margin: 20px auto;
        border-collapse: collapse;
      }
      .td {
        padding: 10px;
      }
      .button {
      width: 200px;
      height: 50px;
      font-size: 18px;
      text-align: center; /* Centers text inside button */
      display: block;
      }
    }
    </style>
  </head>
)rawliteral";

const char plantTable[] = R"rawliteral(
  <table border='1' cellpadding='8' cellspacing='0'>
    <tr>
      <th></th>
      <th>Plant 1</th>
      <th>Plant 2</th>
      <th>Plant 3</th>
    </tr>
    <tr>
      <th>🏷️ Name</th>
      <td>%PLANT1%</td>
      <td><i>null</i></td>
      <td><i>null</i></td>
    </tr>
    <tr>
      <th>🌡️ Temp (F)</th>
      <td>%TEMP1%</td>
      <td><i>null</i></td>
      <td><i>null</i></td>
    </tr>
    <tr>
      <th>💧 Humidity (%)</th>
      <td>%HUMIDITY1%</td>
      <td><i>null</i></td>
      <td><i>null</i></td>
    </tr>
    <tr>
      <th>🌱 Soil Moisture (%)</th>
      <td>%MOISTURE1%</td>
      <td><i>null</i></td>
      <td><i>null</i></td>
    </tr>
    <tr>
      <th>☀️ Light Level (%)</th>
      <td>%LIGHT1%</td>
      <td><i>null</i></td>
      <td><i>null</i></td>
    </tr>
  </table>
)rawliteral";

const char plantHubLogo[] = R"rawliteral(
  <div style="text-align: center; margin-top: 20px;">
    <img src="https://i.imgur.com/tKawB7N.png" style="max-width: 300px; width: 100%; height: auto;">
  </div>
)rawliteral";

const char avatarResponse[] = R"rawliteral(
  <div style="display: flex; align-items: flex-start; margin-top: 20px;">
    <img src="https://i.imgur.com/FtYcmbr.png" alt="Dr Greenthumb Avatar" style="width: 100px; height: 100px; border-radius: 50%; margin-right: 15px;">
      <div style="flex: 1;">
        <h2>Dr. Greenthumb Says:</h2>
          <p id="ai-response" style="font-size: 18px; line-height: 1.4;">Loading advice...
    </p>
  </div>
</div>
)rawliteral";

Plant plant1("Plant 1", MST_PIN, DHT_PIN, PR_PIN, 15, 2, 17);
WiFiServer server(80);

void connectToWifi() {
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connection Successful.");
  Serial.println(WiFi.localIP());
  return;
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Failed to obtain time";
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

String sendToOpenAI(String userMessage) {
  WiFiClientSecure client;
  client.setInsecure(); // For dev only — use certificate in production

  HTTPClient https;
  if (https.begin(client, "https://api.openai.com/v1/chat/completions")) {
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Authorization", String("Bearer ") + openai_api_key);

    DynamicJsonDocument doc(4096);
    doc["model"] = "gpt-3.5-turbo";
    JsonArray messages = doc.createNestedArray("messages");

    // Add system instruction
    JsonObject sys = messages.createNestedObject();
    sys["role"] = "system";
    sys["content"] = 
      "You are Dr. Greenthumb, a helpful plant care assistant. "
      "You have access to real-time sensor data including temperature, humidity, light level, and soil moisture. "
      "Use the soil moisture reading as the most important factor when giving watering advice. "
      "If soil moisture is below 15%, the plant may need watering soon. If it's above 70%, caution the user against overwatering. "
      "Provide friendly, clear advice in 2 to 4 sentences. Be specific, avoid repetition, and include care tips tailored to the current sensor readings.";

    // Add previous conversation from memory
    for (int i = 0; i < chatIndex; i += 2) {
      JsonObject user = messages.createNestedObject();
      user["role"] = "user";
      user["content"] = chatHistory[i];

      if (i + 1 < chatIndex) {
        JsonObject assistant = messages.createNestedObject();
        assistant["role"] = "assistant";
        assistant["content"] = chatHistory[i + 1];
      }
    }

    // Add current user message
    JsonObject newMsg = messages.createNestedObject();
    newMsg["role"] = "user";
    newMsg["content"] = userMessage;

    // Prepare and send request
    String requestBody;
    serializeJson(doc, requestBody);

    int httpCode = https.POST(requestBody);
    String payload;

    if (httpCode == 200) {
      DynamicJsonDocument responseDoc(4096);
      deserializeJson(responseDoc, https.getString());
      payload = responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
      payload = "Error: " + String(httpCode);
    }

    https.end();

    // Save conversation turn if space allows
    if (chatIndex < MAX_CONVO - 1) {
      chatHistory[chatIndex++] = userMessage;
      chatHistory[chatIndex++] = payload;
    }

    return payload;
  }

  return "Failed to connect to OpenAI";
}

String urlDecode(String input) {
  String decoded = "";
  char temp[] = "0x00";  // For hex conversion
  unsigned int len = input.length();
  unsigned int i = 0;

  while (i < len) {
    char c = input[i];
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%' && i + 2 < len) {
      temp[2] = input[i + 1];
      temp[3] = input[i + 2];
      decoded += (char) strtol(temp, NULL, 16);
      i += 2;
    } else {
      decoded += c;
    }
    i++;
  }

  return decoded;
}

void setup() {
  Serial.begin(115200);

  plant1.begin();  // Initialize DHT and LEDs FIRST
  delay(2000);     // Allow DHT to stabilize

  connectToWifi();
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing time...");
  
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for time...");
    delay(500);
  }
  Serial.println("Time synchronized.");

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // Read DHT sensor every 5 seconds
  if (currentMillis - lastDhtReadTime >= 5000) {
    lastDhtReadTime = currentMillis;
    float temp = plant1.getTemperatureF();
    float hum = plant1.getHumidity();
    
    if (!isnan(temp)) {
      cachedTemp = temp;
    }
    if (!isnan(hum)) {
      cachedHumidity = hum;
    }

    lastUpdatedTime = getCurrentTime();
/*
    //Debug serial printouts
    Serial.print("Updated Temp (F): ");
    Serial.println(cachedTemp);
    Serial.print("Updated Humidity (%): ");
    Serial.println(cachedHumidity);
    Serial.print("Last Updated: ");
    Serial.println(lastUpdatedTime);
*/
    plant1.updateLed(cachedTemp, cachedHumidity, plant1.getLightLevel(), plant1.getSoilMoisture());
  }
  plant1.updateFade();

  WiFiClient client = server.available();
  if (client) {
    //Serial.println("[DEBUG] Client connected");

    String request = client.readStringUntil('\r');
    //Serial.println("[DEBUG] Request: " + request);
    client.flush();

    String response;
    String question = "";
    String aiResponse = "";

    if (request.indexOf("GET /ask?question=") >= 0) {
      int qIndex = request.indexOf("question=") + 9;
      int endIndex = request.indexOf(" ", qIndex);
      if (endIndex == -1) endIndex = request.length();
      question = request.substring(qIndex, endIndex);
      question = urlDecode(question);

      String message = "The current time is " + getCurrentTime() + ". ";
      message += "Plant 1 is named '" + plant1.getName() + "'. ";
      message += "The current temperature near Plant 1 is " + String(cachedTemp, 1) + " degrees Fahrenheit. ";
      message += "The humidity near Plant 1 is " + String(cachedHumidity, 1) + " percent. ";
      message += "The light level near Plant 1 is " + String(plant1.getLightLevel()) + " percent. ";
      message += "The soil moisture near Plant 1 is " + String(plant1.getSoilMoisture(), 1) + " percent. ";
      message += "The user may refer to Plant 1 by name or number. ";
      message += "User question: " + question;
      aiResponse = sendToOpenAI(message);

    } else if (request.indexOf("GET /setname?name=") >= 0) {
        int nameIndex = request.indexOf("name=") + 5;
        int endIndex = request.indexOf(" ", nameIndex);
        if (endIndex == -1) endIndex = request.length();
        String newName = request.substring(nameIndex, endIndex);
        newName = urlDecode(newName);
        plant1.setName(newName);
        //Serial.println("[DEBUG] Plant name updated to: " + newName);
    }

    // Build full HTML page with optional response
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
    response += header;
    response += plantHubLogo;
    response += "<h1>Welcome to PlantHub on ESP32</h1>";
    response += "<h2>The dashboard for your houseplant health</h2>";
    response += "<h3>With plant care advice from Dr. Greenthumb, powered by ChatGPT</h3>";

    // --- START dynamic plant table replacement ---
    String dynamicTable = plantTable;

    String nameForm = plant1.getName() + 
    "<br><form action='/setname' method='get' style='margin-top:5px;'>"
    "<input type='text' name='name' placeholder='Rename' size='10'>"
    "<input type='submit' value='Update'>"
    "</form>";

    dynamicTable.replace("%PLANT1%", nameForm);
    dynamicTable.replace("%TEMP1%", String(cachedTemp, 1));
    dynamicTable.replace("%HUMIDITY1%", String(cachedHumidity, 1));
    dynamicTable.replace("%MOISTURE1%", String(plant1.getSoilMoisture(), 1));
    dynamicTable.replace("%LIGHT1%", String(plant1.getLightLevel()));
    // --- END dynamic plant table replacement ---

    response += dynamicTable;  // new, updated table added
    response += "<p><i>Last updated: " + lastUpdatedTime + "</i></p>";
    response += "</br>";
    response += "<form action=\"/ask\" method=\"get\">";
    response += "<input type=\"text\" name=\"question\" placeholder=\"Ask Dr. Greenthumb...\">";
    response += "<input type=\"submit\" value=\"Send\">";
    response += "</form>";
    
    if (question.length() > 0) {
      response += "<hr><h3>You asked:</h3><p>" + question + "</p>";
      response += avatarResponse;
      response.replace("Loading advice...", aiResponse); 
      }

    response += "</body></html>";

    client.print(response);
/*
    Serial.print("Plant: ");
    Serial.println(plant1.getName());
    Serial.print("Temperature (F): ");
    Serial.println(plant1.getTemperatureF());
    Serial.print("Humidity (%): ");
    Serial.println(plant1.getHumidity());
    Serial.print("Light Level (%): ");
    Serial.println(plant1.getLightLevel());
    Serial.print("Soil Moisture (%): ");
    Serial.println(plant1.getSoilMoisture(), 1);
    Serial.println("-----------------------------");
*/
    client.stop();
  }
}