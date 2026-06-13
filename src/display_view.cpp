// display_view.cpp – ePaper Darstellung (WeAct 1.54" 200x200)

#include "display_view.h"
#include "pins.h"
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// WeAct 1.54" 200x200 = SSD1681 (= GDEH0154D67 kompatibel)
static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>
    display(GxEPD2_154_D67(EPAPER_CS, EPAPER_DC, EPAPER_RES, EPAPER_BUSY));

void display_init() {
    SPI.begin(EPAPER_SCK, -1, EPAPER_MOSI, EPAPER_CS);
    display.init(115200, true, 2, false);
    display.setRotation(0);
    display.setTextColor(GxEPD_BLACK);
}

void display_update(float poolTemp, float vlTemp, float rlTemp, float batteryV, int batteryPct) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        // Titel
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(30, 18);
        display.print("Pool-Thermometer");

        // Trennlinie
        display.drawLine(0, 24, 200, 24, GxEPD_BLACK);

        // Pooltemperatur (gross)
        display.setFont(&FreeSansBold24pt7b);
        char buf[16];
        dtostrf(poolTemp, 4, 1, buf);
        display.setCursor(20, 72);
        display.print(buf);
        display.setFont(&FreeSansBold12pt7b);
        display.print(" C");

        // Label
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(65, 88);
        display.print("Pool");

        // Trennlinie
        display.drawLine(0, 96, 200, 96, GxEPD_BLACK);

        // Vorlauf / Rücklauf
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(5, 116);
        display.print("VL:");
        display.setFont(&FreeSansBold12pt7b);
        dtostrf(vlTemp, 4, 1, buf);
        display.setCursor(35, 118);
        display.print(buf);
        display.setFont(&FreeSansBold9pt7b);
        display.print(" C");

        display.setCursor(5, 142);
        display.print("RL:");
        display.setFont(&FreeSansBold12pt7b);
        dtostrf(rlTemp, 4, 1, buf);
        display.setCursor(35, 144);
        display.print(buf);
        display.setFont(&FreeSansBold9pt7b);
        display.print(" C");

        // Trennlinie
        display.drawLine(0, 156, 200, 156, GxEPD_BLACK);

        // Batterie
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(5, 176);
        display.printf("Akku: %d%%  (%.2fV)", batteryPct, batteryV);

        // Batterie-Icon (einfach)
        int barWidth = (int)(batteryPct / 100.0f * 40);
        display.drawRect(150, 164, 44, 16, GxEPD_BLACK);
        display.fillRect(194, 168, 3, 8, GxEPD_BLACK);
        display.fillRect(152, 166, barWidth, 12, GxEPD_BLACK);

    } while (display.nextPage());

    display.hibernate();
}
