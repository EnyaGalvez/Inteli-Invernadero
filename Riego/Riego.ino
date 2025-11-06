/******************************************************
* Universidad del Valle de Guatemala
* Sistema de Riego Inteligente - Versión Final
* Sensores: FC-28 + HW-611 + BH1750
* Control horario día/noche con cálculo de consumos
*******************************************************/

// ===== CONFIGURACIÓN BLYNK =====
#define BLYNK_TEMPLATE_ID "TMPL2EYZWJiJP"
#define BLYNK_TEMPLATE_NAME "CONTROL LED"
#define BLYNK_AUTH_TOKEN "H8Saafs-gCgDHx5JjC7doWJRGjkPZczp"
#define BLYNK_PRINT Serial

// ===== LIBRERÍAS =====
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <time.h>

// ===== CREDENCIALES WIFI =====
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "gnicoool";
char pass[] = "gironJ12";

// ===== GOOGLE SHEETS =====
String GAS_ID = "AKfycbyZzibz7ylOoFRXfCum_eJkePaoUsU8vywMmqS2IebXjrNQ7hvLVJYyH6aQNKz1NsR_";

// ===== PINES =====
const int PIN_RIEGO = 14;        // D5 - Relé o LED de riego
const int PIN_HUMEDAD = A0;      // A0 - Sensor FC-28
const int LED_INTERNO = 2;       // D4 - LED integrado

// ===== OBJETOS =====
WiFiClientSecure client;
Adafruit_BMP280 bmp;

// ===== VARIABLES DE SENSORES =====
float temperatura = 0;
float presion = 0;
float luz = 0;
int humedadSuelo = 0;
int humedadPorcentaje = 0;
bool riegoActivo = false;

// ===== VARIABLES DE TIEMPO =====
unsigned long ultimoEnvio = 0;
const long INTERVALO_ENVIO = 30000; // 30 segundos
unsigned long tiempoInicioRiego = 0;
unsigned long tiempoTotalRiego = 0; // En milisegundos

// ===== UMBRALES DE RIEGO - MODO DÍA (6:00 - 18:00) =====
const float TEMP_MAX_DIA = 30.0;      // °C - No regar si temp > 30°C
const float LUZ_MAX_DIA = 50000.0;    // Lux - No regar si luz solar muy fuerte
const int HUMEDAD_MIN_DIA = 35;       // % - Activar riego si < 35%

// ===== UMBRALES DE RIEGO - MODO NOCHE (18:00 - 6:00) =====
const float TEMP_MIN_NOCHE = 12.0;    // °C - No regar si temp < 12°C
const int HUMEDAD_MIN_NOCHE = 30;     // % - Activar riego si < 30%

// ===== UMBRAL COMÚN =====
const int HUMEDAD_CANCELAR = 70;      // % - Cancelar riego si > 70%

// ===== CONSTANTES DE SIMULACIÓN =====
const float CAUDAL_SIMULADO = 1.5;    // Litros/minuto
const float POTENCIA_SIMULADA = 15.0; // Watts

// ===== VARIABLES DE CONSUMO =====
float aguaConsumidaDiaria = 0;        // Litros
float energiaConsumidaDiaria = 0;     // Wh (Watt-hora)

// ===== CONFIGURACIÓN INICIAL =====
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  SISTEMA DE RIEGO INTELIGENTE UVG         ║");
  Serial.println("║  Control Horario + Consumo                 ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  // Configurar pines
  pinMode(PIN_RIEGO, OUTPUT);
  pinMode(LED_INTERNO, OUTPUT);
  digitalWrite(PIN_RIEGO, LOW);
  digitalWrite(LED_INTERNO, LOW);
  
  client.setInsecure();
  
  Serial.println("📌 Pines: Riego=D5, Humedad=A0, I2C=D2/D1\n");
  
  // Inicializar I2C
  Wire.begin(4, 5);
  delay(100);
  
  // Inicializar HW-611
  Serial.print("🌡️  HW-611... ");
  if (!bmp.begin(0x76)) {
    if (!bmp.begin(0x77)) {
      Serial.println("❌ ERROR");
      while (1) delay(10);
    } else {
      Serial.println("✅ 0x77");
    }
  } else {
    Serial.println("✅ 0x76");
  }
  
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);
  
  Serial.print("💡 BH1750... ");
  Wire.beginTransmission(0x23);
  if (Wire.endTransmission() == 0) {
    Serial.println("✅ Detectado");
  } else {
    Serial.println("⚠️ No detectado");
  }
  
  // Conectar WiFi
  Serial.print("\n📡 Conectando WiFi... ");
  Blynk.begin(auth, ssid, pass);
  Serial.println("✅");
  
  // Configurar hora (zona horaria Guatemala GMT-6)
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("⏰ Sincronizando hora... ");
  while (time(nullptr) < 100000) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" ✅");
  
  mostrarUmbrales();
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║        SISTEMA ACTIVO                      ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
}

// ===== MOSTRAR UMBRALES CONFIGURADOS =====
void mostrarUmbrales() {
  Serial.println("\n📊 UMBRALES CONFIGURADOS:");
  Serial.println("┌─────────────────────────────────────┐");
  Serial.println("│ MODO DÍA (6:00 - 18:00)             │");
  Serial.println("├─────────────────────────────────────┤");
  Serial.println("│ • Temp máxima: 30°C                 │");
  Serial.println("│ • Luz máxima: 50,000 lux            │");
  Serial.println("│ • Humedad mínima: 35%               │");
  Serial.println("└─────────────────────────────────────┘");
  Serial.println("┌─────────────────────────────────────┐");
  Serial.println("│ MODO NOCHE (18:00 - 6:00)           │");
  Serial.println("├─────────────────────────────────────┤");
  Serial.println("│ • Temp mínima: 12°C                 │");
  Serial.println("│ • Humedad mínima: 30%               │");
  Serial.println("└─────────────────────────────────────┘");
  Serial.println("┌─────────────────────────────────────┐");
  Serial.println("│ CANCELAR RIEGO: Humedad > 70%      │");
  Serial.println("└─────────────────────────────────────┘");
}

// ===== OBTENER HORA ACTUAL =====
int obtenerHoraActual() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  return timeinfo->tm_hour;
}

// ===== DETERMINAR SI ES DÍA O NOCHE =====
bool esDia() {
  int hora = obtenerHoraActual();
  return (hora >= 6 && hora < 18);
}

// ===== CONTROL MANUAL BLYNK =====
BLYNK_WRITE(V0) {
  int value = param.asInt();
  if (value == 1) {
    activarRiego();
    Serial.println("🔵 Riego MANUAL activado");
  } else {
    desactivarRiego();
    Serial.println("🔴 Riego MANUAL desactivado");
  }
}

// ===== LEER SENSOR DE LUZ BH1750 =====
float leerLuz() {
  Wire.beginTransmission(0x23);
  Wire.write(0x10); // Modo continuo alta resolución
  Wire.endTransmission();
  delay(180);
  
  Wire.beginTransmission(0x23);
  Wire.requestFrom(0x23, 2);
  
  if (Wire.available() == 2) {
    uint16_t valor = Wire.read();
    valor = (valor << 8) | Wire.read();
    return valor / 1.2;
  }
  return 0;
}

// ===== LEER TODOS LOS SENSORES =====
void leerSensores() {
  temperatura = bmp.readTemperature();
  presion = bmp.readPressure() / 100.0;
  luz = leerLuz();
  
  int valorAnalogico = analogRead(PIN_HUMEDAD);
  humedadSuelo = valorAnalogico;
  humedadPorcentaje = map(valorAnalogico, 1023, 300, 0, 100);
  humedadPorcentaje = constrain(humedadPorcentaje, 0, 100);
  
  // Obtener hora
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
  
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║       📊 LECTURAS DE SENSORES              ║");
  Serial.println("╠════════════════════════════════════════════╣");
  Serial.print("║ ⏰ Hora:            ");
  Serial.print(buffer);
  Serial.print(" (");
  Serial.print(esDia() ? "DÍA" : "NOCHE");
  Serial.println(")");
  Serial.print("║ 🌡️  Temperatura:    ");
  Serial.print(temperatura, 1);
  Serial.println(" °C");
  Serial.print("║ 🌀 Presión:         ");
  Serial.print(presion, 1);
  Serial.println(" hPa");
  Serial.print("║ 💡 Luz:             ");
  Serial.print(luz, 0);
  Serial.println(" lx");
  Serial.print("║ 💧 Humedad suelo:   ");
  Serial.print(humedadPorcentaje);
  Serial.println(" %");
  Serial.print("║ 💦 Riego:           ");
  Serial.println(riegoActivo ? "ON" : "OFF");
  Serial.println("╚════════════════════════════════════════════╝\n");
}

// ===== ENVIAR A BLYNK =====
void enviarDatosBlynk() {
  Blynk.virtualWrite(V1, temperatura);
  Blynk.virtualWrite(V2, presion);
  Blynk.virtualWrite(V3, humedadPorcentaje);
  Blynk.virtualWrite(V4, riegoActivo ? 1 : 0);
  Blynk.virtualWrite(V5, luz);
}

// ===== ENVIAR A GOOGLE SHEETS =====
// ===== ENVIAR A GOOGLE SHEETS =====
void enviarGoogleSheets() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi desconectado");
    return;
  }
  
  // 🔍 DIAGNÓSTICO DE RED
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║         🔍 DIAGNÓSTICO DE RED              ║");
  Serial.println("╠════════════════════════════════════════════╣");
  Serial.print("║ WiFi conectado a: ");
  Serial.println(WiFi.SSID());
  Serial.print("║ IP local: ");
  Serial.println(WiFi.localIP());
  Serial.print("║ Intensidad señal: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  // Probar DNS
  Serial.print("║ Probando DNS (google.com)... ");
  IPAddress test;
  if (WiFi.hostByName("google.com", test)) {
    Serial.print("✅ ");
    Serial.println(test);
  } else {
    Serial.println("❌ FALLO DNS");
    Serial.println("╚════════════════════════════════════════════╝");
    Serial.println("⚠️ No se puede resolver nombres de dominio");
    Serial.println("   Problema con el DNS del router o firewall\n");
    return;
  }
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  Serial.println("📤 Enviando a Google Sheets...");
  
  // Configurar cliente seguro
  client.setInsecure();
  client.setTimeout(20000);
  
  // Conectar al servidor
  Serial.print("🔗 Conectando a script.google.com:443... ");
  if (!client.connect("script.google.com", 443)) {
    Serial.println("❌ FALLO");
    Serial.println("   No se pudo conectar al servidor de Google");
    Serial.println("   Posibles causas:");
    Serial.println("   - Firewall bloqueando puerto 443");
    Serial.println("   - Red con restricciones HTTPS");
    Serial.println("   - Problema de memoria del ESP8266\n");
    return;
  }
  Serial.println("✅");
  
  // Construir path
  String path = "/macros/s/" + GAS_ID + "/exec?";
  path += "temperature=" + String(temperatura);
  path += "&pressure=" + String(presion);
  path += "&light=" + String(luz);
  path += "&humidity=" + String(humedadPorcentaje);
  path += "&status=" + String(riegoActivo ? "ON" : "OFF");
  path += "&water=" + String(aguaConsumidaDiaria, 2);
  path += "&energy=" + String(energiaConsumidaDiaria, 2);
  
  Serial.println("📡 Enviando petición GET...");
  Serial.print("   Path: ");
  Serial.println(path);
  
  // Enviar petición HTTP
  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: script.google.com\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");
  
  // Esperar respuesta
  Serial.print("⏳ Esperando respuesta");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 20000) {
      Serial.println("\n❌ Timeout: El servidor no respondió en 20s");
      client.stop();
      return;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ✅");
  
  // Leer respuesta
  Serial.println("\n--- RESPUESTA DEL SERVIDOR ---");
  bool encontrado = false;
  int lineCount = 0;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    lineCount++;
    if (line.indexOf("OK") >= 0 || line.indexOf("Datos guardados") >= 0) {
      encontrado = true;
    }
    if (line.length() > 2 && lineCount < 20) { // Mostrar primeras 20 líneas
      Serial.println(line);
    }
  }
  Serial.println("-------------------------------");
  Serial.print("Total líneas recibidas: ");
  Serial.println(lineCount);
  
  if (encontrado) {
    Serial.println("\n✅✅✅ DATOS ENVIADOS CORRECTAMENTE ✅✅✅\n");
  } else {
    Serial.println("\n⚠️ Respuesta recibida, verifica Google Sheets\n");
  }
  
  client.stop();
}
// ===== LÓGICA DE RIEGO INTELIGENTE =====
void evaluarRiego() {
  bool debeRegar = false;
  String razon = "";
  
  // C) CONDICIÓN DE CANCELACIÓN (prioridad máxima)
  if (humedadPorcentaje >= HUMEDAD_CANCELAR) {
    if (riegoActivo) {
      desactivarRiego();
      Serial.println("🛑 RIEGO CANCELADO: Suelo húmedo (" + String(humedadPorcentaje) + "%)");
    }
    return;
  }
  
  if (esDia()) {
    // A) MODO DÍA
    if (humedadPorcentaje < HUMEDAD_MIN_DIA) {
      if (temperatura <= TEMP_MAX_DIA && luz <= LUZ_MAX_DIA) {
        debeRegar = true;
        razon = "DÍA: Suelo seco, temp OK, luz OK";
      } else {
        razon = "DÍA: Suelo seco pero ";
        if (temperatura > TEMP_MAX_DIA) razon += "temp alta (" + String(temperatura, 1) + "°C) ";
        if (luz > LUZ_MAX_DIA) razon += "luz fuerte (" + String(luz, 0) + " lx)";
      }
    }
  } else {
    // B) MODO NOCHE
    if (humedadPorcentaje < HUMEDAD_MIN_NOCHE) {
      if (temperatura >= TEMP_MIN_NOCHE) {
        debeRegar = true;
        razon = "NOCHE: Suelo seco, temp OK";
      } else {
        razon = "NOCHE: Suelo seco pero temp baja (" + String(temperatura, 1) + "°C)";
      }
    }
  }
  
  // Aplicar decisión
  if (debeRegar && !riegoActivo) {
    activarRiego();
    Serial.println("🌱 RIEGO ACTIVADO: " + razon);
  } else if (!debeRegar && riegoActivo) {
    desactivarRiego();
    Serial.println("🛑 RIEGO DESACTIVADO: " + razon);
  }
}

// ===== ACTIVAR RIEGO =====
void activarRiego() {
  digitalWrite(PIN_RIEGO, HIGH);
  digitalWrite(LED_INTERNO, HIGH);
  riegoActivo = true;
  tiempoInicioRiego = millis();
  Blynk.virtualWrite(V0, 1);
}

// ===== DESACTIVAR RIEGO =====
void desactivarRiego() {
  if (riegoActivo) {
    // Calcular tiempo de riego
    unsigned long duracion = millis() - tiempoInicioRiego;
    tiempoTotalRiego += duracion;
    
    // Calcular consumos
    float minutos = duracion / 60000.0;
    float aguaConsumida = minutos * CAUDAL_SIMULADO;
    float energiaConsumida = (minutos / 60.0) * POTENCIA_SIMULADA;
    
    aguaConsumidaDiaria += aguaConsumida;
    energiaConsumidaDiaria += energiaConsumida;
    
    Serial.println("\n💧 CONSUMO DEL CICLO:");
    Serial.println("   Duración: " + String(minutos, 2) + " min");
    Serial.println("   Agua: " + String(aguaConsumida, 2) + " L");
    Serial.println("   Energía: " + String(energiaConsumida, 2) + " Wh");
    Serial.println("   ACUMULADO HOY:");
    Serial.println("   Agua total: " + String(aguaConsumidaDiaria, 2) + " L");
    Serial.println("   Energía total: " + String(energiaConsumidaDiaria, 2) + " Wh\n");
  }
  
  digitalWrite(PIN_RIEGO, LOW);
  digitalWrite(LED_INTERNO, LOW);
  riegoActivo = false;
  Blynk.virtualWrite(V0, 0);
}

// ===== LOOP PRINCIPAL =====
void loop() {
  Blynk.run();
  
  leerSensores();
  enviarDatosBlynk();
  evaluarRiego();
  
  if (millis() - ultimoEnvio >= INTERVALO_ENVIO) {
    enviarGoogleSheets();
    ultimoEnvio = millis();
  }
  
  delay(5000);
}