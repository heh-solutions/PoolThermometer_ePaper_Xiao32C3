// display_view.cpp – ePaper Darstellung (WeAct 1.54" 200x200)

#include "display_view.h"
#include "pins.h"
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// WeAct 1.54" 200x200 = SSD1681 (= GDEH0154D67 kompatibel)
// BUSY auf -1 = kein Hardware-BUSY, Library nutzt Software-Delays
static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT>
    display(GxEPD2_154_D67(EPAPER_CS, EPAPER_DC, EPAPER_RES, -1 /*BUSY deaktiviert*/));

// Hilfsfunktion: Temperatur als String ("--.-" wenn ungültig)
static void tempToStr(char* buf, float temp, bool compact) {
    if (temp <= -126.0f) {
        strcpy(buf, compact ? "--.-" : " --.-");
    } else {
        dtostrf(temp, compact ? 4 : 5, 1, buf);
    }
}

void display_init() {
    // CS nicht an SPI.begin übergeben – GxEPD2 steuert CS selbst per digitalWrite
    SPI.begin(EPAPER_SCK, -1, EPAPER_MOSI, -1);
    display.init(115200, true, 50, false);  // 50ms reset pulse
    display.setRotation(1);  // 90° gedreht
    display.setTextColor(GxEPD_BLACK);
    Serial.println("ePaper: init done");
}

void display_update(float poolTemp, float vlTemp, float rlTemp, float batteryV, int batteryPct) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);

        char buf[16];

        // === Pool-Temperatur MAXIMAL GROSS (obere ~95px) ===
        // °C Einheit oben rechts (12pt)
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(175, 18);
        display.print("C");
        display.drawCircle(170, 4, 3, GxEPD_BLACK);

        // Pool-Temp in 24pt mit TextSize 2 → effektiv ~48pt
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(2);
        tempToStr(buf, poolTemp, true);
        display.setCursor(10, 80);
        display.print(buf);
        display.setTextSize(1);

        // Trennlinie
        display.drawLine(0, 95, 200, 95, GxEPD_BLACK);

        // === VL und RL (nebeneinander, y=97-172) ===

        // --- VL links (x=0-98) ---
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(5, 112);
        display.print("VL");
        // °C rechtsbündig in Label-Zeile
        display.setCursor(80, 112);
        display.print("C");
        display.drawCircle(76, 98, 2, GxEPD_BLACK);

        display.setFont(&FreeSansBold24pt7b);
        tempToStr(buf, vlTemp, true);
        display.setCursor(0, 160);
        display.print(buf);

        // Vertikale Trennlinie
        display.drawLine(100, 98, 100, 172, GxEPD_BLACK);

        // --- RL rechts (x=102-200) ---
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(107, 112);
        display.print("RL");
        // °C rechtsbündig in Label-Zeile
        display.setCursor(182, 112);
        display.print("C");
        display.drawCircle(178, 98, 2, GxEPD_BLACK);

        display.setFont(&FreeSansBold24pt7b);
        tempToStr(buf, rlTemp, true);
        display.setCursor(102, 160);
        display.print(buf);

        // Trennlinie vor Batterie
        display.drawLine(0, 175, 200, 175, GxEPD_BLACK);

        // === Batterie-Balken (immer anzeigen) ===
        {
            int bx = 10, by = 180, bw = 160, bh = 18;
            // Batterie-Gehäuse (doppelter Rand)
            display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
            display.drawRect(bx + 1, by + 1, bw - 2, bh - 2, GxEPD_BLACK);
            // Batterie-Nase rechts
            display.fillRect(bx + bw, by + 4, 5, 10, GxEPD_BLACK);

            // Füllstand
            int pct = (batteryPct > 100) ? 100 : ((batteryPct < 0) ? 0 : batteryPct);
            int fillW = (int)((bw - 6) * pct / 100.0f);
            if (fillW > 0) {
                display.fillRect(bx + 3, by + 3, fillW, bh - 6, GxEPD_BLACK);
            }

            // Prozent-Zahl eingebettet
            display.setFont(&FreeMonoBold9pt7b);
            char pctBuf[8];
            if (batteryPct >= 0) {
                sprintf(pctBuf, "%d%%", batteryPct);
            } else {
                strcpy(pctBuf, "--%");
            }
            int16_t tx = bx + bw / 2 - 18;
            int16_t ty = by + 14;
            if (pct > 50) {
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.setTextColor(GxEPD_BLACK);
            }
            display.setCursor(tx, ty);
            display.print(pctBuf);
            display.setTextColor(GxEPD_BLACK);
        }

    } while (display.nextPage());

    display.hibernate();
}
