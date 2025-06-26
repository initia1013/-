#include <WiFi.h>

const char* ssid = "U+NetMRNA";
const char* password = "meowmeow";

WiFiServer server(80);

// 명령 상태 & 회전수
String motorCmd[3] = {"OFF", "OFF", "OFF"};
long motorRotCount[3] = {0, 0, 0};  // 누적 틱 수

const float TICKS_PER_REV = 1440.0;  // 틱 수 (예: 1440틱 = 1바퀴)

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  client.flush();

  // 명령 처리
  if (req.indexOf("/cmd?") != -1) {
    int espIndex = getESPIndex(req);
    if (espIndex != -1) {
      String& cmd = motorCmd[espIndex];
      if (req.indexOf("action=OFF") != -1) {
        cmd = "OFF";
      } else if (req.indexOf("action=ON") != -1) {
        cmd = "ON,128,FWD";
      } else if (req.indexOf("action=ROTATE") != -1 && cmd.startsWith("ON")) {
        int s1 = cmd.indexOf(',') + 1;
        int s2 = cmd.lastIndexOf(',');
        String speed = cmd.substring(s1, s2);
        String dir = cmd.substring(s2 + 1);
        String newDir = (dir == "FWD") ? "REV" : "FWD";
        cmd = "ON," + speed + "," + newDir;
      } else if (req.indexOf("action=SETSPEED") != -1) {
        int valIndex = req.indexOf("val=");
        if (valIndex != -1 && cmd.startsWith("ON")) {
          String valStr = req.substring(valIndex + 4);
          valStr.trim();
          int comma2 = cmd.lastIndexOf(',');
          String dir = cmd.substring(comma2 + 1);
          cmd = "ON," + valStr + "," + dir;
        }
      }
      Serial.printf("Command for ESP %c set to %s\n", 'A' + espIndex, cmd.c_str());
    }
    sendRedirect(client);
    return;
  }

  // 회전수 초기화 처리
  if (req.indexOf("/reset?") != -1) {
    int espIndex = getESPIndex(req);
    if (espIndex != -1) {
      motorRotCount[espIndex] = 0;
      Serial.printf("ESP %c rotation count reset\n", 'A' + espIndex);
    }
    sendRedirect(client);
    return;
  }

  // 회전수 보고 처리
  if (req.indexOf("/report?") != -1) {
    int espIndex = getESPIndex(req);
    if (espIndex != -1) {
      int valIndex = req.indexOf("rot=");
      if (valIndex != -1) {
        String valStr = req.substring(valIndex + 4);
        valStr.trim();
        long delta = valStr.toInt();
        motorRotCount[espIndex] += delta;
        Serial.printf("ESP %c reported delta %ld, total %ld\n", 'A' + espIndex, delta, motorRotCount[espIndex]);
      }
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("OK");
    client.stop();
    return;
  }

  // 명령 요청 처리
  if (req.indexOf("/getcmd?") != -1) {
    int espIndex = getESPIndex(req);
    if (espIndex != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      client.println(motorCmd[espIndex]);
      client.stop();
      return;
    }
  }

  // 웹 UI 출력
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<html><head><title>ESP32 Motor Control</title><style>");
  client.println("body { font-family: Arial; text-align: center; background: #f0f0f0; padding: 20px; }");
  client.println("h1 { color: #333; } button { font-size: 20px; padding: 10px 20px; margin: 5px; }");
  client.println("input[type='text'] { font-size: 18px; padding: 5px; width: 70px; text-align: center; }");
  client.println("</style></head><body>");
  client.println("<h1>ESP32 Motor Control Demo</h1>");

  for (int i = 0; i < 3; i++) {
    char espName = 'A' + i;
    float rotations = motorRotCount[i] / TICKS_PER_REV;
    client.printf("<p><b>ESP %c motor:</b> %s</p>", espName, motorCmd[i].c_str());
    client.printf("<p><b>ESP %c rotation:</b> %ld ticks (%.2f revolutions)</p>", espName, motorRotCount[i], rotations);
    // 회전수 초기화 버튼 추가
    client.printf("<a href=\"/reset?esp=%c\"><button>Reset Rotation</button></a><br>", espName);

    client.printf("<a href=\"/cmd?esp=%c&action=ON\"><button>%c ON</button></a> ", espName, espName);
    client.printf("<a href=\"/cmd?esp=%c&action=OFF\"><button>%c OFF</button></a><br>", espName, espName);
    client.printf(
      "<form action=\"/cmd\" method=\"get\">"
      "<input type=\"hidden\" name=\"esp\" value=\"%c\">"
      "<input type=\"hidden\" name=\"action\" value=\"SETSPEED\">"
      "Speed: <input type=\"text\" name=\"val\">"
      "<input type=\"submit\" value=\"Set Speed\">"
      "</form>",
      espName
    );
    client.printf("<a href=\"/cmd?esp=%c&action=ROTATE\"><button>Rotate</button></a><br><br>", espName);
  }

  client.println("</body></html>");
  client.stop();
}

int getESPIndex(String& req) {
  if (req.indexOf("esp=A") != -1) return 0;
  else if (req.indexOf("esp=B") != -1) return 1;
  else if (req.indexOf("esp=C") != -1) return 2;
  return -1;
}

void sendRedirect(WiFiClient& client) {
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println();
  client.stop();
}
