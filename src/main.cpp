#include <Arduino.h>
#include <ModbusMasterPzem017.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BluetoothSerial.h"
#include "ArcFaultDetector3Class.h"
using namespace Eloquent::ML::Port;
#include "FeatureExtractor.h"


BluetoothSerial SerialBT;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static uint8_t pzemSlaveAddr = 0x01;
ModbusMaster node;


float PZEMVoltage_raw = 0;
float PZEMCurrent = 0;
float PZEMPower = 0;
float PZEMEnergy = 0;

FeatureExtractor myFeatureExtractor;
ArcFaultDetector3Class myArcDetector;

void resetPzemEnergy(uint8_t slaveAddr);
void setShunt(uint8_t slaveAddr, uint16_t shuntValue);

void printCenteredTitle(String text) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 0);
    display.println(text);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial2.begin(9600, SERIAL_8N1);

    SerialBT.begin("ESP32_Arc_Detector");
    Serial.println("Bluetooth device is ready to pair.");

    Serial.println("Inisialisasi OLED...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("Alokasi SSD1306 gagal."));
        for (;;);
    }
    display.display();
    delay(500);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    printCenteredTitle("BOOTING UP...");

    display.display();
    delay(1000);

    Serial.println("Inisialisasi PZEM-017...");
    node.begin(pzemSlaveAddr, Serial2);
    delay(1000);

    uint8_t testRead = node.readInputRegisters(0x0000, 1);
    if (testRead == node.ku8MBSuccess) {
        Serial.println("Komunikasi PZEM-017 awal OK.");
    } else {
        Serial.print("Komunikasi PZEM-017 awal GAGAL, error: 0x");
        Serial.println(testRead, HEX);
    }
}

void loop() {
    uint8_t result = node.readInputRegisters(0x0000, 6);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    printCenteredTitle("ARC FAULT DETECTION");
    display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

    if (result == node.ku8MBSuccess) {
        PZEMVoltage_raw = (float)node.getResponseBuffer(0x0000) / 100.0f;
        PZEMCurrent = (float)node.getResponseBuffer(0x0001) / 100.0f;
        // Gabungkan dua register 16-bit untuk Energi (Wh)
        uint32_t energy_raw = ((uint32_t)node.getResponseBuffer(4) << 16) | node.getResponseBuffer(5);
        PZEMEnergy = energy_raw;

        PZEMPower = PZEMVoltage_raw * PZEMCurrent;
        display.setCursor(5, 15);
        display.print("V: "); display.print(PZEMVoltage_raw, 2); display.println(" V");
        display.setCursor(5, 25);
        display.print("I: "); display.print(PZEMCurrent, 8); display.println(" A");
        
        myFeatureExtractor.addSample(PZEMVoltage_raw, PZEMCurrent);
        
        if (myFeatureExtractor.isReady()) {
            float mean_v, std_dev_v, mean_c, std_dev_c;
            myFeatureExtractor.extractFeatures(mean_v, std_dev_v, mean_c, std_dev_c);

            float features_to_predict[] = {mean_v, std_dev_v, mean_c, std_dev_c};
            int predicted_label_numeric = myArcDetector.predict(features_to_predict);

            String predicted_label_text;
           
            if (predicted_label_numeric == 0) {
                predicted_label_text = "NO CONTACT";
            } else if (predicted_label_numeric == 1) {
                predicted_label_text = "NORMAL";
            } else if (predicted_label_numeric == 2) {
                predicted_label_text = "ARC FLASH \u26A0";
            } else {
                predicted_label_text = "UNKNOWN";
            }

            if (predicted_label_numeric == 0) {
                display.setTextSize(1);
            } else {
                display.setTextSize(2); 
            }
            
            display.setCursor(0, 48);
            display.print(" ");
            display.println(predicted_label_text);
    
            Serial.println("---[ DATA BARU ]---");
            Serial.print("Tegangan (V) Asli: "); Serial.println(PZEMVoltage_raw, 2);
            Serial.print("Arus (A): "); Serial.println(PZEMCurrent, 8);
            Serial.print("Daya (W): "); Serial.println(PZEMPower, 2);
            Serial.print("Energi (Wh): "); Serial.println(PZEMEnergy, 0);
            Serial.println("  --- Fitur ---");
            Serial.print("  Mean V: "); Serial.println(mean_v);
            Serial.print("  Std Dev V: "); Serial.println(std_dev_v);
            Serial.print("  Mean I: "); Serial.println(mean_c);
            Serial.print("  Std Dev I: "); Serial.println(std_dev_c);
            Serial.println("  --- Prediksi ---");
            Serial.print("  Label Numerik: "); Serial.println(predicted_label_numeric);
            Serial.print("  Hasil: "); Serial.println(predicted_label_text);
            Serial.println("====================\n");

    
            String dataToSend = String(PZEMVoltage_raw, 2) + "," + String(PZEMCurrent, 8) + "," + predicted_label_text + "," ;
            SerialBT.println(dataToSend);

        } else {
    
            display.setTextSize(1);
            display.setCursor(0, 48); 
            display.println("Collecting Data...");
            display.setCursor(0, 56);
            display.print(myFeatureExtractor.getCurrentSampleCount()); 
            display.print("/"); 
            display.print(FE_WINDOW_SIZE);

         
            String dataToSend = String(PZEMVoltage_raw, 2) + "," + String(PZEMCurrent, 3) + ",Collecting";
            SerialBT.println(dataToSend);
            
      
            Serial.print("Mengumpulkan data... ");
            Serial.print(myFeatureExtractor.getCurrentSampleCount());
            Serial.print("/");
            Serial.println(FE_WINDOW_SIZE);
        }
    } else {
     
        display.setCursor(0, 25);
        display.println("Modbus Read FAIL!");
        display.setCursor(0, 35);
        display.print("Error: 0x");
        display.println(result, HEX);
      
        Serial.print("Modbus Read FAIL! Error: 0x");
        Serial.println(result, HEX);
    }

    display.display();
    delay(50);
}


void resetPzemEnergy(uint8_t slaveAddr) {
    uint8_t functionCode = 0x42;
    uint16_t u16CRC = 0xFFFF;
    u16CRC = crc16_update(u16CRC, slaveAddr);
    u16CRC = crc16_update(u16CRC, functionCode);
    
    while(Serial2.available()) Serial2.read(); 

    Serial.println("Mengirim perintah Reset Energy ke PZEM-017...");
    Serial2.write(slaveAddr);
    Serial2.write(functionCode);
    Serial2.write(lowByte(u16CRC));  
    Serial2.write(highByte(u16CRC)); 
    Serial2.flush();

    delay(200);

    Serial.print("Respons dari PZEM setelah Reset Energy: ");
    String responseStr = "";
    unsigned long startTime = millis();
    int bytesRead = 0;
    while ((millis() - startTime < 500) && bytesRead < 4) {
        if (Serial2.available()) {
            uint8_t byteRead = Serial2.read();
            Serial.print(byteRead, HEX);
            Serial.print(" ");
            responseStr += String(byteRead, HEX) + " ";
            bytesRead++;
        }
    }
    if (responseStr.length() == 0) {
        Serial.println("(Tidak ada respons atau timeout)");
    } else {
        Serial.println();
    }
}

void setShunt(uint8_t slaveAddr, uint16_t shuntValue) {
    static uint8_t functionCode = 0x06;
    static uint16_t registerAddress = 0x0001;
    uint16_t u16CRC = 0xFFFF;
    u16CRC = crc16_update(u16CRC, slaveAddr);
    u16CRC = crc16_update(u16CRC, functionCode);
    u16CRC = crc16_update(u16CRC, highByte(registerAddress));
    u16CRC = crc16_update(u16CRC, lowByte(registerAddress));
    u16CRC = crc16_update(u16CRC, highByte( shuntValue));
    u16CRC = crc16_update(u16CRC, lowByte(shuntValue));

    Serial.println("Mengirim perintah untuk mengubah shunt setting ke PZEM...");
    Serial.print("     Slave: 0x"); Serial.print(slaveAddr, HEX);
    Serial.print(", Func: 0x"); Serial.print(functionCode, HEX);
    Serial.print(", RegAddr: 0x"); Serial.print(registerAddress, HEX);
    Serial.print(", Value: 0x"); Serial.println(shuntValue, HEX);
    
    Serial2.write(slaveAddr);
    Serial2.write(functionCode);
    Serial2.write(highByte(registerAddress));
    Serial2.write(lowByte(registerAddress));
    Serial2.write(highByte(shuntValue));
    Serial2.write(lowByte(shuntValue));
    Serial2.write(lowByte(u16CRC));
    Serial2.write(highByte(u16CRC));
    Serial2.flush();
    delay(50);

    Serial.print("     Respons dari PZEM setelah setShunt: ");
    String responseStr = "";
    unsigned long startTime = millis();
    int bytesRead = 0;
    while ((millis() - startTime < 500) && bytesRead < 4) {
        if (Serial2.available()) {
            uint8_t byteRead = Serial2.read();
            Serial.print(byteRead, HEX);
            Serial.print(" ");
            responseStr += String(byteRead, HEX) + " ";
            bytesRead++;
        }
    }
    if (responseStr.length() == 0) {
        Serial.println("(Tidak ada respons atau timeout)");
    } else {
        Serial.println();
    }
}
