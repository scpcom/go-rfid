
/*
 * Copy the RFID card data into variables and then 
 * scan the second empty card to copy all the data
 * ----------------------------------------------------------------------------
 * Example sketch/program which will try the most used default keys listed in 
 * https://code.google.com/p/mfcuk/wiki/MifareClassicDefaultKeys to dump the
 * block 0 of a MIFARE RFID card using a RFID-RC522 reader.
 * 
 * Typical pin layout used:
 * ------------------------------------------
 *             MFRC522      Odroid
 *             Reader/PCD   Go
 * Signal      Pin          Pin
 * ------------------------------------------
 * RST/Reset   RST          5 IO4  RST
 * SPI SS      SDA(SS)      4 IO15 SDA(SS)
 * SPI MOSI    MOSI         8 IO23 MOSI
 * SPI MISO    MISO         7 IO19 MISO
 * SPI SCK     SCK          2 IO18 SCK
 *             GND          1 GND
 *             3.3V         6 3,3V
 *
 */

#include <odroid_go.h>
#include <MirrorGo.h>

//include <SPI.h>
#include <MFRC522.h>

#ifndef MirrorGo_h
#define MG GO
#endif

#define DARK_ORANGE 0xFC08

#define TITLE_DIM 32
#define TITLE_BG_COLOR     rgb565(96-TITLE_DIM,128-TITLE_DIM,208-TITLE_DIM)
#define TITLE_TEXT_COLOR   rgb565(200, 32, 64)
#define MENU_BG_COLOR      rgb565(96,128,208)
#define MENU_HILIGHT_COLOR rgb565(255,128,40)
#define MENU_TEXT_COLOR    WHITE
#define ACTION_TEXT_COLOR  WHITE
#define STATUS_TEXT_COLOR  WHITE

#ifndef PIN_BLUE_LED
#define PIN_BLUE_LED 2
#endif
#define LED_ACT_PIN PIN_BLUE_LED

#define RST_PIN         4           // Configurable, see typical pin layout above
#define SS_PIN          15          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

byte buffer[18];
byte block;
byte waarde[64][16];
byte theUid[16];
byte theUidSize = 4;
MFRC522::StatusCode status;
    
MFRC522::MIFARE_Key key;

// Number of known default keys (hard-coded)
// NOTE: Synchronize the NR_KNOWN_KEYS define with the defaultKeys[] array
#define NR_KNOWN_KEYS   8
// Known keys, see: https://code.google.com/p/mfcuk/wiki/MifareClassicDefaultKeys
byte knownKeys[NR_KNOWN_KEYS][MFRC522::MF_KEY_SIZE] =  {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, // FF FF FF FF FF FF = factory default
    {0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5}, // A0 A1 A2 A3 A4 A5
    {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5}, // B0 B1 B2 B3 B4 B5
    {0x4d, 0x3a, 0x99, 0xc3, 0x51, 0xdd}, // 4D 3A 99 C3 51 DD
    {0x1a, 0x98, 0x2c, 0x7e, 0x45, 0x9a}, // 1A 98 2C 7E 45 9A
    {0xd3, 0xf7, 0xd3, 0xf7, 0xd3, 0xf7}, // D3 F7 D3 F7 D3 F7
    {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, // AA BB CC DD EE FF
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // 00 00 00 00 00 00
};

bool dataRead = false;
bool actionInProgress = false;
uint32_t actionWaitMS = 2000;

int drawIndex = 0;
int menuIndex = 0;
int menuMax = 4;
int lastBatPercentage = 0;
bool doUpdate = false;

bool rfidOK = false;
bool sdOK = false;
bool wifiOK = false;

extern uint8_t token_img[];
extern uint8_t pinout_img[];


uint16_t rgb565(float r, float g, float b) {
    return (((int)r) & 0xF8) << 8 | (((int)g) & 0xFC) << 3 | ((int)b) >> 3;
}

void menuBat() {
    int batx = MG.lcd.width()-MG.lcd.textWidth("100%");
    if (GO.battery.getPercentage() != lastBatPercentage) {
        lastBatPercentage = GO.battery.getPercentage();
        MG.lcd.setTextColor(TITLE_TEXT_COLOR, WHITE);
        MG.lcd.setCursor(batx, 0);
        MG.lcd.print(lastBatPercentage); MG.lcd.print("%");
        MG.lcd.setCursor(0, 0);
        MG.lcd.setTextColor(TITLE_TEXT_COLOR, TITLE_TEXT_COLOR);
    }
}

void menuCls() {

    MG.lcd.clear();
    MG.lcd.fillScreen(MENU_BG_COLOR);
    MG.lcd.drawJpg(token_img, 6412, 160, 0);
    MG.lcd.fillRect(0, 0, MG.lcd.width()/2, MG.lcd.fontHeight(1), TITLE_BG_COLOR);
    lastBatPercentage = 0;
}

void menuClear() {
    MG.lcd.fillRect(0, 0, MG.lcd.width()/2, MG.lcd.height(), MENU_BG_COLOR);
}


//void menuItem(const char *text) {
void menuItem(int SelectedIndex, String text) {
    if (SelectedIndex == drawIndex) { MG.lcd.setTextColor(MENU_HILIGHT_COLOR); } else { MG.lcd.setTextColor(MENU_TEXT_COLOR); }
    MG.lcd.println(text);
    drawIndex++;
}

void menuHead() {
    MG.lcd.setCursor(0, 0);

    MG.lcd.setTextColor(TITLE_TEXT_COLOR, WHITE);
    menuBat();
    MG.lcd.setTextColor(TITLE_TEXT_COLOR);
    MG.lcd.println("    Menu    ");
    MG.lcd.println("");

    drawIndex = 0;
}

void menu() {
    menuHead();
    
    menuItem(menuIndex, " 0. Info   ");
    menuItem(menuIndex, " 1. Read   ");
    menuItem(menuIndex, " 2. Test   ");
    menuItem(menuIndex, " 3. Write  ");
    menuItem(menuIndex, " 4. Pinout ");
    if (menuMax > 4)
        menuItem(menuIndex, " 5. From file ");
    if (menuMax > 5)
        menuItem(menuIndex, " 6. To file   ");
    if (menuMax > 6)
        menuItem(menuIndex, " 7. WiFi Web  ");

    MG.lcd.setCharCursor(0, 12);

    MG.lcd.setTextColor(STATUS_TEXT_COLOR);

    MG.lcd.print("  RFID: ");
    MG.lcd.print((rfidOK) ? "ok" : "--");
    MG.lcd.println("");
    MG.lcd.print("   SD : ");
    MG.lcd.print((sdOK) ? "ok" : "--");
    MG.lcd.println("");
#ifdef _WiFiWeb_H_
    MG.lcd.print("  WiFi: ");
    MG.lcd.print((wifiOK) ? "ok" : "--");
#endif
    
}

int file_drawIndex = 0;
int file_menuIndex = 0;
int file_menuMax = 0;
int file_index = 0;
int file_count = 0;
bool file_doUpdate = false;
String *file_list;

#ifdef _WiFiWeb_H_
extern String file_size(int64_t bytes);
#else
String file_size(int bytes) {
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes) + " B";
  else if (bytes < (1024 * 1024))      fsize = String(bytes / 1024.0, 2) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) fsize = String(bytes / 1024.0 / 1024.0, 2) + " MB";
  else                              fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 2) + " GB";
  return fsize;
}
#endif

void file_menu(String directory) {
    menuHead();

    while (drawIndex <= file_menuMax) {
        menuItem(file_menuIndex, file_list[file_index+drawIndex]);
    }

    MG.lcd.setTextColor(STATUS_TEXT_COLOR);
}

int file_menu_loop(String directory) {
    MG.update();
    menuBat();

    doUpdate = false;
    if (MG.JOY_Y.isAxisPressed() == 2) {
        file_menuIndex--;
        doUpdate = true;
    } else if (MG.JOY_Y.isAxisPressed() == 1) {
        file_menuIndex++;
        doUpdate = true;
    } else if (MG.BtnA.isPressed() == 1) {
        return file_menuIndex;
    } else if (MG.BtnB.isPressed() == 1) {
        return -1;
    } else if (MG.BtnMenu.isPressed() == 1) {
        return -1;
    } else if (MG.BtnSelect.isPressed() == 1) {
        return file_menuIndex;
    } else if (MG.BtnStart.isPressed() == 1) {
        return file_menuIndex;
    }

    if (file_menuIndex < 0) {
        if (file_index > 0) {
            file_index--;
            file_menuIndex = 0;
            menuClear();
        } else
            file_menuIndex = file_menuMax;
    } else if (file_menuIndex > file_menuMax) {
        if (file_index+file_menuIndex < file_count) {
            file_index++;
            file_menuIndex = file_menuMax;
            menuClear();
        } else
            file_menuIndex = 0;
    }

    if (doUpdate)
        file_menu(directory);
        
    delay(50);

    return -2;
}

void run_file_menu(String directory) {
    String entryName = "";
    String tree = "";
    bool emptyFolder = true;
    int ret;

    file_count = 0;
    file_index = 0;
    file_menuMax = 0;

    menuCls();

    File dir = SD.open(directory);
    if (!dir) {
        Serial.println("Failed to open directory");
        MG.lcd.println("Failed to open directory");
        return;
    }

    while (true) {
        File entry =  dir.openNextFile();

        if (! entry) {
            // no more files
            // return to the first file in the directory
            dir.rewindDirectory();
            break;
        }
        
        file_count += 1;
        
        entry.close();
    }

    if (file_count < 0) {
        Serial.println("Empty directory");
        MG.lcd.clear();
        MG.lcd.setCursor(0, 0);
        MG.lcd.setTextWrap(false);
        MG.lcd.setTextColor(ACTION_TEXT_COLOR);
        MG.lcd.println("Empty directory");
        return;
    }

    file_list = new String[file_count];

    while (true) {
        File entry =  dir.openNextFile();

        if (! entry) {
            // no more files
            // return to the first file in the directory
            dir.rewindDirectory();
            break;
        }

        entryName = entry.name();
        entryName.replace(directory + "/", "");

        if (entry.isDirectory()) {
            emptyFolder = false;
        } else if (file_index < file_count) {
            file_list[file_index] = entryName;
            file_index += 1;
            emptyFolder = false;
        }
        entry.close();
    }

    file_menuMax = file_index;
    if (file_menuMax > 9)
        file_menuMax = 9;
    
    file_index = 0;
    file_menuIndex = 0;
    file_menu(directory);

    ret = -2;
    while (ret == -2) {
      ret = file_menu_loop(directory);
    }

    if (ret < 0) {
        actionWaitMS = 0;
        return;
    }

    MG.lcd.clear();
    MG.lcd.setCursor(0, 0);
    MG.lcd.setTextWrap(false);
    MG.lcd.setTextColor(ACTION_TEXT_COLOR);

    Serial.println("");
    Serial.print("Opening: ");
    MG.lcd.print("Opening: ");
    Serial.println(file_list[file_index+ret]);
    MG.lcd.println(file_list[file_index+ret]);

    File rfidFile = SD.open(directory+"/"+file_list[file_index+ret]);
    delete [] file_list;
    file_list = NULL;
    if (!rfidFile) {
        Serial.println("Failed to open file for reading");
        MG.lcd.println("Failed to open file for reading");
        return;
    }
    Serial.println(file_size(rfidFile.size()));
    MG.lcd.println(file_size(rfidFile.size()));
    Serial.println("Read from file: ");
    MG.lcd.println("Read from file: ");

    theUidSize = 0;

    if (rfidFile.size() < 1024) {
        rfidFile.close();
        return;
    }
    
    for(byte block = 0; block < 64; block++){

        // Read block
        byte byteCount = 16;
        byteCount = rfidFile.readBytes((char*)buffer, byteCount);

        // Dump block data
        Serial.print(F("Block ")); Serial.print(block); Serial.print(F(":"));
        dump_byte_array1(buffer, 16); //omzetten van hex naar ASCI
        Serial.println();
        
            for (int p = 0; p < 16; p++) //De 16 bits uit de block uitlezen
            {
                waarde [block][p] = buffer[p];
                Serial.print(waarde[block][p]);
                Serial.print(" ");
            }
    }
    Serial.println();
    
        for (byte i = 0; i < 16; i++) {
            theUid[i] = waarde[0][i];
        }

    if (rfidFile.size() < 1024+1) {
        rfidFile.close();
        return;
    }

    mfrc522.uid.size = rfidFile.read();
    if (mfrc522.uid.size > 10)
        mfrc522.uid.size = 10;

    if (rfidFile.size() >= 1024+mfrc522.uid.size) {
        rfidFile.readBytes((char*)mfrc522.uid.uidByte, mfrc522.uid.size);
    }
    
    if (rfidFile.size() >= 1024+mfrc522.uid.size) {
        mfrc522.uid.sak = rfidFile.read();
    }

    theUidSize = mfrc522.uid.size;

    MG.lcd.print(F("UID:"));
    go_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    MG.lcd.println("\n");

    dataRead = true;
    
    rfidFile.close();
}

void strcat_byte_array(char* str, byte *buffer, byte bufferSize) {
    char hex[2];
    for (byte i = 0; i < bufferSize; i++) {
        sprintf(hex, "%02hhX", buffer[i]);
        strcat(str, hex);
    }
}

void run_file_write(String directory) {
    char outFile[32] = "";

    MG.lcd.clear();
    MG.lcd.setCursor(0, 0);
    MG.lcd.setTextWrap(false);
    MG.lcd.setTextColor(ACTION_TEXT_COLOR);

    if (!mfrc522.uid.size) {
        Serial.println("No data to write");
        MG.lcd.println("No data to write");
        return;
    }
    
    strcpy(outFile, "");
    strcat_byte_array(outFile, mfrc522.uid.uidByte, mfrc522.uid.size);
    strcat(outFile, ".bin");

    File dir = SD.open(directory);
    if (!dir) {
        Serial.println("Failed to open directory");
        MG.lcd.println("Failed to open directory");
        return;
    }

    Serial.println("");
    Serial.print("Opening: ");
    MG.lcd.print("Opening: ");
    Serial.println(outFile);
    MG.lcd.println(outFile);

    File rfidFile = SD.open(directory+"/"+outFile, FILE_WRITE);
    if (!rfidFile) {
        Serial.println("Failed to open file for writing");
        MG.lcd.println("Failed to open file for writing");
        return;
    }
    Serial.println("Write to file: ");
    MG.lcd.println("Write to file: ");

    for(byte block = 0; block < 64; block++){

            for (int p = 0; p < 16; p++) //De 16 bits uit de block uitlezen
            {
                buffer[p] = waarde [block][p];
                Serial.print(waarde[block][p]);
                Serial.print(" ");
            }

        // Dump block data
        Serial.print(F("Block ")); Serial.print(block); Serial.print(F(":"));
        dump_byte_array1(buffer, 16); //omzetten van hex naar ASCI
        Serial.println();
        
        // Write block
        byte byteCount = 16;
        byteCount = rfidFile.write(buffer, byteCount);
    }
    Serial.println();

    rfidFile.write(mfrc522.uid.size);

    rfidFile.write(mfrc522.uid.uidByte, mfrc522.uid.size);
    
    rfidFile.write(mfrc522.uid.sak);

    rfidFile.close();
}

#ifndef _WiFiWeb_H_
bool sd_init() {
  MG.lcd.clear();
  MG.lcd.setCursor(0, 0);
  MG.lcd.setTextWrap(false);

  Serial.println("");

  Serial.print("Initializing SD card...");
  MG.lcd.println("Initializing SD card...");

  if (!SD.begin()) {
    MG.lcd.println("Card Mount Failed");
    Serial.println("Card Mount Failed");
    return false;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    MG.lcd.println("No SD card attached");
    Serial.println("No SD card attached");
    return false;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    MG.lcd.println("MMC");
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    MG.lcd.println("SDSC");
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    MG.lcd.println("SDHC");
    Serial.println("SDHC");
  } else {
    MG.lcd.println("UNKNOWN");
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  MG.lcd.printf("SD Card Size: %lluMB\n", cardSize);

  MG.lcd.println(" ");
  Serial.println();

  return true;
}
#endif

bool rfid_init() {
  mfrc522.PCD_Init();         // Init MFRC522 card
  //mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
    
  // Get the MFRC522 firmware version
  byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("Firmware Version: 0x"));
  Serial.print(v, HEX);
  // Lookup which version
  switch(v) {
    case 0x88: Serial.println(F(" = (clone)"));  break;
    case 0x90: Serial.println(F(" = v0.0"));     break;
    case 0x91: Serial.println(F(" = v1.0"));     break;
    case 0x92: Serial.println(F(" = v2.0"));     break;
    case 0x12: Serial.println(F(" = counterfeit chip"));     break;
    default:   Serial.println(F(" = (unknown)"));
  }
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    return false;
  }

  return true;
}

void setup() {
    // put your setup code here, to run once:
    // Serial and SPI will be initialized by MG.begin()
    //Serial.begin(115200);       // Initialize serial communications with the PC
    //while (!Serial);            // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
    //SPI.begin();                // Init SPI bus

    MG.begin(115200);

    GO.Speaker.setVolume(0);
    pinMode(25, OUTPUT);
    digitalWrite(25, LOW);

    GO.battery.setProtection(true);
    delay(100);

    sdOK = sd_init();
    if (sdOK)
        menuMax += 2;
    delay(100);

    Serial.print("Setup: Check heap before RFID: ");
    if (heap_caps_check_integrity_all(true)) {
        Serial.println("OK");
    } else {
        Serial.println("Failed!");
    }

    rfidOK = rfid_init();

    Serial.println(F("Try the most used default keys to print block 0 to 63 of a MIFARE PICC."));
    Serial.println("0.Card info\n1.Read card \n2.Write to card \n3.Copy the data.");

    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

#ifdef _WiFiWeb_H_
/*
    wifiweb = wifiweb_init();
    if (sdOK && wifiweb)
*/
    if (sdOK) {
        wifiOK = setupWiFiWeb("/odroid/data/rfid");
        menuMax++;
    }
#endif

    Serial.print("Setup: Check heap before menu: ");
    if (heap_caps_check_integrity_all(true)) {
        Serial.println("OK");
    } else {
        Serial.println("Failed!");
    }

    MG.lcd.setTextFont(1);
    MG.lcd.setTextSize(2);
    
    menuCls();
    menu();

#ifdef _LCDMIRROR_H_
    MG.lcd.Save();
#else

    pinMode(PIN_BLUE_LED, OUTPUT);
    digitalWrite(LED_ACT_PIN, LOW);
#endif
}

void runAction() {
    static uint32_t ms;
    
    if (actionInProgress)
        return;
        
    actionInProgress = true;        
    actionWaitMS = 2000;
    
    MG.lcd.clear();
    MG.lcd.setCursor(0, 0);
    MG.lcd.setTextColor(ACTION_TEXT_COLOR);

    //digitalWrite(PIN_BLUE_LED, HIGH);

    if (menuIndex == 0) {
        MG.lcd.println("Information");
        keuze0();
    } else if (menuIndex == 1) {
        MG.lcd.println("Reading");
        keuze1();
    } else if (menuIndex == 2) {
        MG.lcd.println("Testing");
        keuze2();
    } else if (menuIndex == 3) {
        MG.lcd.println("Writing");
        keuze3();
    } else if (menuIndex == 4) {
        actionWaitMS = 0;
        showPinout();
    } else if (menuIndex == 5) {
        run_file_menu("/odroid/data/rfid");
    } else if (menuIndex == 6) {
        run_file_write("/odroid/data/rfid");
#ifdef _WiFiWeb_H_
    } else if (menuIndex == 7) {
        actionWaitMS = 0;
        MG.lcd.setTextSize(1);
        wifiOK = showWiFiWeb();
        MG.lcd.setTextSize(2);
#endif
    }

    ms = millis();
    actionWaitMS = ms+actionWaitMS;
    while (ms < actionWaitMS) {
        MG.update();
        delay(50);
        ms = millis();
    }

    //digitalWrite(PIN_BLUE_LED, LOW);
    
    actionInProgress = false;

    menuCls();
    menu();
}

void loop() {
    // put your main code here, to run repeatedly:
    MG.update();
    menuBat();

    doUpdate = false;
    if (MG.JOY_Y.isAxisPressed() == 2) {
        menuIndex--;
        doUpdate = true;
    } else if (MG.JOY_Y.isAxisPressed() == 1) {
        menuIndex++;
        doUpdate = true;
    } else if (MG.BtnA.isPressed() == 1) {
        runAction();
    } else if (MG.BtnB.isPressed() == 1) {
        if (dataRead)
            menuIndex = 3;
        else
            menuIndex = 1;
        runAction();
    } else if (MG.BtnMenu.isPressed() == 1) {
        menuIndex = 0;
        runAction();
    } else if (MG.BtnVolume.isPressed() == 1) {
        menuIndex = 2;
        runAction();
    } else if (MG.BtnSelect.isPressed() == 1) {
        menuIndex = 1;
        runAction();
    } else if (MG.BtnStart.isPressed() == 1) {
        menuIndex = 3;
        runAction();
    }

    if (menuIndex < 0)
        menuIndex = menuMax;
    else if (menuIndex > menuMax)
        menuIndex = 0;

    if (doUpdate)
        menu();
        
    delay(50);
}

void showPinout() {

    MG.lcd.clear();
    MG.lcd.drawJpg(pinout_img, 26065, 0, 0);
    
    while (true) {
        MG.update();

        if (MG.BtnMenu.isPressed() == 1)
            break;
            
        delay(50);
    }
}

void go_dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        MG.lcd.print(buffer[i] < 0x10 ? " 0" : " ");
        MG.lcd.print(buffer[i], HEX);
    }
}

//Via seriele monitor de bytes uitlezen in hexadecimaal
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

//Via seriele monitor de bytes uitlezen in ASCI
void dump_byte_array1(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.write(buffer[i]);
    }
}

void go_lcd_drawMenuButton(int32_t x0, int32_t y0) {
    int16_t w = MG.lcd.textWidth("W", 1);
    int16_t h = MG.lcd.fontHeight(1);
    int16_t o = (h-6)/2;
    MG.lcd.drawLine(x0,y0+o+0,x0+w,y0+o+0,ACTION_TEXT_COLOR);
    MG.lcd.drawLine(x0,y0+o+3,x0+w,y0+o+3,ACTION_TEXT_COLOR);
    MG.lcd.drawLine(x0,y0+o+6,x0+w,y0+o+6,ACTION_TEXT_COLOR);
}

bool waitForCard() {
    MG.lcd.println("Insert token...");
    MG.lcd.println("Press   Menu to cancel");
    go_lcd_drawMenuButton(MG.lcd.textWidth("Press ", 1),MG.lcd.fontHeight(1)*2);

    // Look for new cards
    while (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        MG.update();
        
        if (MG.BtnMenu.isPressed() == 1) {
            actionWaitMS = 0;
            return false;
        }

        delay(50);
    }

    return true;
}

/*
 * Try using the PICC (the tag/card) with the given key to access block 0 to 63.
 * On success, it will show the key details, and dump the block data on Serial.
 *
 * @return true when the given key worked, false otherwise.
 */
 
bool try_key(MFRC522::MIFARE_Key *key)
{
    bool result = false;
    
    for(byte block = 0; block < 64; block++){
      
        // Serial.println(F("Authenticating using key A..."));
        status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return false;
        }

        // Read block
        byte byteCount = sizeof(buffer);
        status = mfrc522.MIFARE_Read(block, buffer, &byteCount);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
        }
        else {
        // Successful read
        result = true;
        Serial.print(F("Success with key:"));
        dump_byte_array((*key).keyByte, MFRC522::MF_KEY_SIZE);
        Serial.println();
        
        // Dump block data
        Serial.print(F("Block ")); Serial.print(block); Serial.print(F(":"));
        dump_byte_array1(buffer, 16); //omzetten van hex naar ASCI
        Serial.println();
        
            for (int p = 0; p < 16; p++) //De 16 bits uit de block uitlezen
            {
                waarde [block][p] = buffer[p];
                Serial.print(waarde[block][p]);
                Serial.print(" ");
            }        
        }
    }
    Serial.println();

    if (result) {
        for (byte i = 0; i < 16; i++) {
            theUid[i] = waarde[0][i];
        }
    }
    
    Serial.println("0.Card info\n1.Read card \n2.Write to card \n3.Copy the data.");

    mfrc522.PICC_HaltA();       // Halt PICC
    mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
    return result;
}

bool MIFARE_SetSector0(byte *newUid, byte uidSize, bool logErrors) {
  
  // UID + BCC byte can not be larger than 16 together
  if (!newUid || !uidSize || uidSize > 15) {
    if (logErrors) {
      Serial.println(F("New UID buffer empty, size 0, or size > 15 given"));
    }
    return false;
  }
  
  // Authenticate for reading
  MFRC522::MIFARE_Key key = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, (byte)1, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    
    if (status == MFRC522::STATUS_TIMEOUT) {
      // We get a read timeout if no card is selected yet, so let's select one
      
      // Wake the card up again if sleeping
//        byte atqa_answer[2];
//        byte atqa_size = 2;
//        PICC_WakeupA(atqa_answer, &atqa_size);
      
      if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        Serial.println(F("No card was previously selected, and none are available. Failed to set UID."));
        return false;
      }
      
      status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, (byte)1, &key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) {
        // We tried, time to give up
        if (logErrors) {
          Serial.println(F("Failed to authenticate to card for reading, could not set UID: "));
          Serial.println(mfrc522.GetStatusCodeName(status));
        }
        return false;
      }
    }
    else {
      if (logErrors) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
      }
      return false;
    }
  }
  
  // Read block 0
  byte block0_buffer[18];
  byte byteCount = sizeof(block0_buffer);
  status = mfrc522.MIFARE_Read((byte)0, block0_buffer, &byteCount);
  if (status != MFRC522::STATUS_OK) {
    if (logErrors) {
      Serial.print(F("MIFARE_Read() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      Serial.println(F("Are you sure your KEY A for sector 0 is 0xFFFFFFFFFFFF?"));
    }
    return false;
  }
  
  // Write new UID to the data we just read, and calculate BCC byte
  for (uint8_t i = 0; i < 16; i++) {
    block0_buffer[i] = newUid[i];
  }
  byte bcc = 0;
  for (uint8_t i = 0; i < uidSize; i++) {
    bcc ^= newUid[i];
  }
  
  // Write BCC byte to buffer
  block0_buffer[uidSize] = bcc;
  
  // Stop encrypted traffic so we can send raw bytes
  mfrc522.PCD_StopCrypto1();
  
  // Activate UID backdoor
  if (!mfrc522.MIFARE_OpenUidBackdoor(logErrors)) {
    if (logErrors) {
      Serial.println(F("Activating the UID backdoor failed."));
    }
    return false;
  }
  
  // Write modified block 0 back to card
  status = mfrc522.MIFARE_Write((byte)0, block0_buffer, (byte)16);
  if (status != MFRC522::STATUS_OK) {
    if (logErrors) {
      Serial.print(F("MIFARE_Write() failed: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    return false;
  }
  
  // Wake the card up again
  byte atqa_answer[2];
  byte atqa_size = 2;
  mfrc522.PICC_WakeupA(atqa_answer, &atqa_size);
  
  return true;
}

void keuze2(){ //Test waardes in blokken

    MG.lcd.print(F("UID:"));
    go_dump_byte_array(theUid, theUidSize);
    MG.lcd.println("\n");

    Serial.print(F("UID:"));
    dump_byte_array(theUid, theUidSize);
    Serial.println("\n");
    Serial.print(F("BCC:"));
    dump_byte_array(&theUid[theUidSize], 1);
    Serial.println("\n");
    Serial.print(F("Manufacturer Data:"));
    dump_byte_array(&theUid[theUidSize+1], 16-theUidSize-1);
    Serial.println("\n");
  
    for(block = 4; block <= 62; block++){
        if(block == 3 || block == 7 || block == 11 || block == 15 || block == 19 || block == 23 || block == 27 || block == 31 || block == 35 || block == 39 || block == 43 || block == 47 || block == 51 || block == 55 || block == 59){
            block ++;
        }
  
        Serial.print(F("Writing data into block ")); 
        Serial.print(block);
        Serial.println("\n");
  
        for(int j = 0; j < 16; j++){
            Serial.print(waarde[block][j]);
            Serial.print(" ");
        }
        
        Serial.println("\n");    
    }
  
    Serial.println("0.Card info\n1.Read card \n2.Write to card \n3.Copy the data.");
}

void keuze3(){ //Copy the data in the new card
    Serial.println("Insert new card...");
    if (!waitForCard())
        return;

#ifdef _LEDMIRROR_H_
    MG.led.on();
#else
    digitalWrite(LED_ACT_PIN, HIGH);
#endif

    MG.lcd.print(F("Card UID:"));
    go_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    MG.lcd.println("\n");

    // Show some details of the PICC (that is: the tag/card)
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));
    
    // Try the known default keys
    /*MFRC522::MIFARE_Key key;
    for (byte k = 0; k < NR_KNOWN_KEYS; k++) {
        // Copy the known key into the MIFARE_Key structure
        for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
            key.keyByte[i] = knownKeys[k][i];
        }
    }*/
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = 0xFF;
    }

    for(int i = 4; i <= 62; i++){ //De blocken 4 tot 62 kopieren, behalve al deze onderstaande blocken (omdat deze de authenticatie blokken zijn)
        if(i == 3 || i == 7 || i == 11 || i == 15 || i == 19 || i == 23 || i == 27 || i == 31 || i == 35 || i == 39 || i == 43 || i == 47 || i == 51 || i == 55 || i == 59){
            i++;
        }
        block = i;
    
        // Authenticate using key A
        Serial.println(F("Authenticating using key A..."));
        status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return;
        }
    
        // Authenticate using key B
        Serial.println(F("Authenticating again using key B..."));
        status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, block, &key, &(mfrc522.uid));
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            return;
        }
    
        // Write data to the block
        Serial.print(F("Writing data into block ")); 
        Serial.print(block);
        Serial.println("\n");
          
        dump_byte_array(waarde[block], 16); 
    
          
        status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(block, waarde[block], 16);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Write() failed: "));
            Serial.println(mfrc522.GetStatusCodeName(status));
        }
    
        
        Serial.println("\n");
    }

    if ( MIFARE_SetSector0(theUid, (byte)theUidSize, true) ) {
        Serial.println(F("Wrote new UID to card."));
        MG.lcd.println(F("Wrote new UID to card."));
    }
  
    mfrc522.PICC_HaltA();       // Halt PICC
    mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
  
    dataRead = false;

#ifdef _LEDMIRROR_H_
    MG.led.off();
#else
    digitalWrite(LED_ACT_PIN, LOW);
#endif

    Serial.println("0.Card info\n1.Read card \n2.Write to card \n3.Copy the data.");
}

void keuze1(){ //Read card
    Serial.println("Insert card...");
    if (!waitForCard())
        return;

#ifdef _LEDMIRROR_H_
    MG.led.on();
#else
    digitalWrite(LED_ACT_PIN, HIGH);
#endif

    MG.lcd.print(F("Card UID:"));
    go_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    MG.lcd.println("\n");

    // Show some details of the PICC (that is: the tag/card)
    Serial.print(F("Card UID:"));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));

    theUidSize = mfrc522.uid.size;
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (i < 15)
        theUid[i] = mfrc522.uid.uidByte[i];
    }
    
    // Try the known default keys
    MFRC522::MIFARE_Key key;
    for (byte k = 0; k < NR_KNOWN_KEYS; k++) {
        // Copy the known key into the MIFARE_Key structure
        for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
            key.keyByte[i] = knownKeys[k][i];
        }
        // Try the key
        if (try_key(&key)) {
            // Found and reported on the key and block,
            // no need to try other keys for this PICC
            dataRead = true;
            break;
        }
    }
    
#ifdef _LEDMIRROR_H_
    MG.led.off();
#else
    digitalWrite(LED_ACT_PIN, LOW);
#endif
}

void keuze0(){
    Serial.println("Insert card...");
    if (!waitForCard())
        return;

#ifdef _LEDMIRROR_H_
    MG.led.on();
#else
    digitalWrite(LED_ACT_PIN, HIGH);
#endif

    MG.lcd.print(F("Card UID:"));
    go_dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    MG.lcd.println("\n");
    MG.lcd.println("----------------------\n");

    mfrc522.PICC_DumpToSerial(&(mfrc522.uid));

#ifdef _LEDMIRROR_H_
    MG.led.off();
#else
    digitalWrite(LED_ACT_PIN, LOW);
#endif
  
    Serial.println("0.Card info\n1.Read card \n2.Write to card \n3.Copy the data.");
}


/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
