#include <Arduino.h>
#include <Adafruit_MAX31865.h>

// 软件SPI
Adafruit_MAX31865 rtd = Adafruit_MAX31865(15, 23, 19, 18);

#define RREF      430.0
#define RNOMINAL  100.0

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("MAX31865 test");

    if (!rtd.begin(MAX31865_3WIRE))
    {
        Serial.println("MAX31865 init failed");
        while (1);
    }

    Serial.println("MAX31865 initialized");
}

void loop()
{
    uint16_t raw = rtd.readRTD();

    Serial.print("RTD raw: ");
    Serial.println(raw);

    float ratio = raw;
    ratio /= 32768;

    Serial.print("Ratio = ");
    Serial.println(ratio, 8);

    Serial.print("Resistance = ");
    Serial.println(RREF * ratio, 8);

    Serial.print("Temperature = ");
    Serial.println(rtd.temperature(RNOMINAL, RREF));

    uint8_t fault = rtd.readFault();

    if (fault)
    {
        Serial.print("Fault: 0x");
        Serial.println(fault, HEX);

        if (fault & MAX31865_FAULT_HIGHTHRESH) Serial.println("RTD High Threshold");
        if (fault & MAX31865_FAULT_LOWTHRESH) Serial.println("RTD Low Threshold");
        if (fault & MAX31865_FAULT_REFINLOW) Serial.println("REFIN- > 0.85 x VBIAS");
        if (fault & MAX31865_FAULT_REFINHIGH) Serial.println("REFIN- < 0.85 x VBIAS");
        if (fault & MAX31865_FAULT_RTDINLOW) Serial.println("RTD open");
        if (fault & MAX31865_FAULT_OVUV) Serial.println("Under/Over voltage");

        rtd.clearFault();
    }

    Serial.println();
    delay(1000);
}