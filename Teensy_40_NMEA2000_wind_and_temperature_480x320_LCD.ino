



#include "skp_lvgl.h"
#include <lvgl.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>

extern lv_obj_t * label_wind_speed; 
extern lv_obj_t * gauge1;
extern lv_obj_t * lvneedle;
extern lv_obj_t * label_temperature;

char buff[20];

typedef struct {
  unsigned long PGN;
  void (*Handler)(const tN2kMsg &N2kMsg); 
} tNMEA2000Handler;

void SystemTime(const tN2kMsg &N2kMsg);
void Wind(const tN2kMsg &N2kMsg);
void Temperature(const tN2kMsg &N2kMsg);

tNMEA2000Handler NMEA2000Handlers[]={
  {126992L,&SystemTime},
  {130312L,&Temperature},
  {130306L,&Wind},
  {0,0}
};

Stream *OutputStream;

void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);

void setup() {
  delay(1000);

  Serial.begin(115200); 
  Serial.println("Teensy 4.0 NMEA2000 display www.skpang.co.uk 12/20");
  
  OutputStream=&Serial;
   
  // NMEA2000.SetN2kCANReceiveFrameBufSize(50);
  // Do not forward bus messages at all
  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
  NMEA2000.SetForwardStream(OutputStream);
  // Set false below, if you do not want to see messages parsed to HEX withing library
  NMEA2000.EnableForward(false);
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
  //  NMEA2000.SetN2kCANMsgBufSize(2);
  NMEA2000.Open();
  OutputStream->print("Running...");

  skp_lvgl_init();   // Start LVGL
  
}

//*****************************************************************************
template<typename T> void PrintLabelValWithConversionCheckUnDef(const char* label, T val, double (*ConvFunc)(double val)=0, bool AddLf=false ) {
  OutputStream->print(label);
  if (!N2kIsNA(val)) {
    if (ConvFunc) { OutputStream->print(ConvFunc(val)); } else { OutputStream->print(val); }
  } else OutputStream->print("not available");
  if (AddLf) OutputStream->println();
}

//*****************************************************************************
void SystemTime(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    uint16_t SystemDate;
    double SystemTime;
    tN2kTimeSource TimeSource;
    
    if (ParseN2kSystemTime(N2kMsg,SID,SystemDate,SystemTime,TimeSource) ) {
      PrintLabelValWithConversionCheckUnDef("System time: ",SID,0,true);
      PrintLabelValWithConversionCheckUnDef("  days since 1.1.1970: ",SystemDate,0,true);
      PrintLabelValWithConversionCheckUnDef("  seconds since midnight: ",SystemTime,0,true);
                        OutputStream->print("  time source: "); PrintN2kEnumType(TimeSource,OutputStream);
    } else {
      OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
    }
}

//*****************************************************************************
void Wind(const tN2kMsg &N2kMsg) {
   double pi = 3.14;

   unsigned char SID;
   double WindSpeed;
   double WindAngle; 

   double WindAngle_degree;
   
   tN2kWindReference WindReference;
    
   if(ParseN2kWindSpeed(N2kMsg,SID,  WindSpeed, WindAngle, WindReference)) 
   {
      Serial.print("Wind speed ");
      Serial.print(WindSpeed);
      Serial.print(" ");
   
      sprintf(buff,"%2.2f",WindSpeed);
      lv_label_set_text(label_wind_speed,buff);    // Update wind speed on LCD
   
      WindAngle_degree = WindAngle * (180/pi);
      Serial.print("Wind direction ");
      Serial.println(WindAngle_degree);
   
      lv_img_set_angle(  lvneedle, WindAngle_degree*10);   // Update needle on LCD
  
    } else {
      OutputStream->print("Failed to parse PGN: "); OutputStream->println(N2kMsg.PGN);
    }
}
void Temperature(const tN2kMsg &N2kMsg) {
    unsigned char SID;
    unsigned char TempInstance;
    tN2kTempSource TempSource;
    double ActualTemperature;
    double SetTemperature;  
    double temperature;

    if (ParseN2kTemperature(N2kMsg,SID,TempInstance,TempSource,ActualTemperature,SetTemperature) ) 
    {
        temperature = ActualTemperature - 273.15;   // Covert K to C
        
        Serial.print("Temperature ");
        Serial.println(temperature);
        sprintf(buff,"%2.1f",temperature);
        lv_label_set_text(label_temperature, buff);   // Update temperature on LCD

    } else {
      OutputStream->print("Failed to parse PGN: ");  OutputStream->println(N2kMsg.PGN);
    }
}
//*****************************************************************************
void printLLNumber(Stream *OutputStream, unsigned long long n, uint8_t base=10)
{
  unsigned char buf[16 * sizeof(long)]; // Assumes 8-bit chars.
  unsigned long long i = 0;

  if (n == 0) {
    OutputStream->print('0');
    return;
  }

  while (n > 0) {
    buf[i++] = n % base;
    n /= base;
  }

  for (; i > 0; i--)
    OutputStream->print((char) (buf[i - 1] < 10 ?
      '0' + buf[i - 1] :
      'A' + buf[i - 1] - 10));
}

//*****************************************************************************
void BinaryStatusFull(const tN2kMsg &N2kMsg) {
    unsigned char BankInstance;
    tN2kBinaryStatus BankStatus;

    if (ParseN2kBinaryStatus(N2kMsg,BankInstance,BankStatus) ) {
      OutputStream->print("Binary status for bank "); OutputStream->print(BankInstance); OutputStream->println(":");
      OutputStream->print("  "); //printLLNumber(OutputStream,BankStatus,16);
      for (uint8_t i=1; i<=28; i++) {
        if (i>1) OutputStream->print(",");
        PrintN2kEnumType(N2kGetStatusOnBinaryStatus(BankStatus,i),OutputStream,false);
      }
      OutputStream->println();
    }
}

//*****************************************************************************
void BinaryStatus(const tN2kMsg &N2kMsg) {
    unsigned char BankInstance;
    tN2kOnOff Status1,Status2,Status3,Status4;

    if (ParseN2kBinaryStatus(N2kMsg,BankInstance,Status1,Status2,Status3,Status4) ) {
      if (BankInstance>2) { // note that this is only for testing different methods. MessageSender.ini sends 4 status for instace 2
        BinaryStatusFull(N2kMsg);
      } else {
        OutputStream->print("Binary status for bank "); OutputStream->print(BankInstance); OutputStream->println(":");
        OutputStream->print("  Status1=");PrintN2kEnumType(Status1,OutputStream,false);
        OutputStream->print(", Status2=");PrintN2kEnumType(Status2,OutputStream,false);
        OutputStream->print(", Status3=");PrintN2kEnumType(Status3,OutputStream,false);
        OutputStream->print(", Status4=");PrintN2kEnumType(Status4,OutputStream,false);
        OutputStream->println();
      }
    }
}



//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {
  int iHandler;
  
  // Find handler
  //OutputStream->print("In Main Handler: "); OutputStream->println(N2kMsg.PGN);
  for (iHandler=0; NMEA2000Handlers[iHandler].PGN!=0 && !(N2kMsg.PGN==NMEA2000Handlers[iHandler].PGN); iHandler++);
  
  if (NMEA2000Handlers[iHandler].PGN!=0) {
    NMEA2000Handlers[iHandler].Handler(N2kMsg); 
  }
}

//*****************************************************************************
void loop() 
{ 
  NMEA2000.ParseMessages();
  lv_task_handler(); /* let the GUI do its work */
}
