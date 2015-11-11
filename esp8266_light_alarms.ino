/*10-11 ноября - улучшения
 * 22 окт - добавил ESP8266 и wifi ntp синхронизацию
 * 18 окт. 2015
 Программа для ежедневного включения/выключения светодиодов для подсветки рассады
 Включение - каждый день в 7:00
 Выключение - каждый день , причем длина светового светодиодного* дня увеличивается
 от lightDuration_min до lightDuration_max
 Программа рассчитана на (lightDurationDays_max)=90 дней подсветки
*/
//#define ARDUINO_UNO
#define ESP8266
#include <Time.h>
#include <TimeAlarms.h>
#include <EEPROM.h>

//RelayType тип реле - включается нулем или единицей
#define RELAY_1 13  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
//int ledPin = 13;
//Reverse relay
#define RELAY_ON 0  // GPIO value to write to turn on attached relay
#define RELAY_OFF 1 // GPIO value to write to turn off attached relay

/*-------- Настройка ESP8266 ----------*/
//#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
char ssid[] = "Home 32";  //  your network SSID (name)
char pass[] = "farmen111";       // your network password
//---------------- NTP Servers:-------------
//IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
const int timeZone = 5;     // Ekaterinburg
//const int timeZone = 8;  // test
//unsigned int localPort = 8888;  // local port to listen for UDP packets
unsigned int localPort = 2390;      // local port to listen for UDP packets

//#ifdef ESP8266
WiFiUDP udp;
//#endif
/*-------- Инициализация алгоритма подсветки ----------*/

uint32_t lightDuration_min = 12 * SECS_PER_HOUR; //min световой день в начале досветки в сек.
uint32_t lightDuration_max = 15 * SECS_PER_HOUR; //макс. световой день в конце через 2-3 месяца
int lightDurationDays_max = 90; //кол-во дней подсветки
uint32_t lightDuration, lightDuration_test; //продолжительность светодиодного светового дня в сек.
//потом будет увеличиваться на t_increase/в день
int t_increase = (lightDuration_max - lightDuration_min) / lightDurationDays_max;
time_t offAlarmTime; //время выключения
byte lightDay;   //световой день номер..
byte calendarDay;// какое календарное число было до выключения
byte morningHour = 7; //во сколько утром включаемся, часов

/*-------- setup ----------*/
void setup()
{
  EEPROM.begin(10);
  /*-------- setup ntp ----------*/
  // time sinhronise
  //setTime(6, 59, 40, 14, 1, 15); // set time to Saturday 8:29:00am Jan 1 2015

  Serial.begin(9600);
  #ifdef ESP8266
  delay(500);
  Serial.begin(115200);
  #endif
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  //узнаем время из интернета
   //Обозначили функцию для автосинхронизации времени
  while (year() ==1970 ) {setSyncProvider(getNtpTime); delay(2000);};
  setSyncInterval(86400);//и интревал час между синхронизациями

  Serial.print("starting program in ");
  digitalClockDisplay();// print real time
  checkLightDay(); //подсчитаем какой сегодня световой день
  lightDuration_test = lightDuration_min + (lightDay - 1) * t_increase;// считаем длину светового дня в сек.
  //lightDuration= 20;  //test
  pinMode(RELAY_1, OUTPUT);
  digitalWrite(RELAY_1, RELAY_OFF); //выключаем нагрузку
  // create the alarms
  Alarm.alarmRepeat(morningHour, 00, 0, LightOn); // 7:00 am every day
 // Alarm.alarmRepeat(morningHour, 00, 0, LightOn); // 7:00 am every day
  //отчет в начале программы
  Serial.print("Today is the ");
  Serial.print(lightDay);
  Serial.println(" day of program");
  Serial.print("1) lightDuration_min (sec)= ");
  Serial.println(lightDuration_min);
  Serial.print("2) lightDuration_max (sec)= ");
  Serial.println(lightDuration_max);
  Serial.print("3) lightDurationDays_max (days)= ");
  Serial.println(lightDurationDays_max);
  Serial.print("4) t_increase (sec)= ");
  Serial.println(t_increase);
  Serial.print("5) todays lightDuration (sec)= ");
  Serial.println(lightDuration);
  digitalClockDisplay();
  //если световой день еще не закончен то
  if (((hour()*SECS_PER_HOUR + minute()*SECS_PER_MIN + second()) - morningHour * SECS_PER_HOUR) < lightDuration_test)
  { Serial.println(" Program resumed on light day ->> Light will go On");
    LightOn();
  }
}

time_t prevDisplay = 0; // when the digital clock was displayed
int delayTime = 600;// время между показыванием времени в ком порт


void loop() {
  //время каждые 600 сек.
  if (timeStatus() != timeNotSet) {
    if (now() == (prevDisplay + delayTime)) { //update the display only if left 10 sec
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
  Alarm.delay(1000); // wait one second between clock display
}

//включение светодиодов
void LightOn() {
  digitalWrite(RELAY_1, RELAY_ON); //включаем нагрузку

  //подсчитаем какой сегодня световой день
  checkLightDay();
  // считаем длину светового дня в сек.
  lightDuration = lightDuration_min + (lightDay - 1) * t_increase;
  //lightDuration= 20;  //test
  //посчитаем сколько осталось до конца светового дня, ато вдруг программа включилась в середине дня
  uint32_t todayDuration = lightDuration - (hour() * SECS_PER_HOUR + minute() * SECS_PER_MIN + second() - morningHour * SECS_PER_HOUR);
  //и поставили будильник на отключение
  // Alarm.timerOnce(todayDuration, LightOff);
  //посчитаем лучше в какое время надо отключаться
  offAlarmTime = now() + todayDuration;
  Alarm.alarmOnce(hour(offAlarmTime), minute(offAlarmTime), second(offAlarmTime), LightOff);

  Serial.print("Led ON  in ");
  digitalClockDisplay();// print real time
  // print сколько осталось времени светить
  Serial.print(" in ");
  Serial.print(hour(offAlarmTime)); Serial.print(" : ");
  Serial.print(minute(offAlarmTime)); Serial.print(" : ");
  Serial.print(second(offAlarmTime)); 
  Serial.print(" will LedOff after ");
    Serial.print(hour(todayDuration)); Serial.print(" hrs, ");
  Serial.print(minute(todayDuration)); Serial.print(" minutes, ");
  Serial.print(second(todayDuration)); Serial.println(" seconds");
  //
  Serial.print("Todays lightDuration (sec)= ");
  Serial.println(lightDuration);
 // Serial.print("TEST lightDuration_test (sec)= ");
 // Serial.println(lightDuration_test);
}
void LightOff() {
  digitalWrite(RELAY_1, RELAY_OFF); //выключаем нагрузку
  Serial.println("Led Off in ");
  digitalClockDisplay();// print real time
}
// считаем какой сегодня по счету световой день
void checkLightDay() {

  //световой день номер.
  lightDay = EEPROM.read(1);
  // какое календарное число было до выключения
  calendarDay = EEPROM.read(2);
  //если до выключения было другое число, то прибавляем световой день
  if (calendarDay != day()) {
    lightDay++;
    if (lightDay > 90) lightDay = 90; //чтоб длина светового дня не увеличивалась до бесконечности
    calendarDay = day();
    EEPROM.write(1, lightDay);
    EEPROM.write(2, calendarDay);
    EEPROM.commit();
    lightDuration_test = lightDuration + t_increase;//увеличили длину светового дня в сек
    Serial.print("lightDay writed to ");
    Serial.println(lightDay);
    Serial.print("calendarDay writed to ");
    Serial.println(calendarDay);
  }
  Serial.print(" lightDay= ");
  Serial.print( lightDay);
  Serial.print(",  calendarDay= ");
  Serial.println( calendarDay);
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}
void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
time_t getNtpTime() //запрос времени нтп
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  WiFi.hostByName(ntpServerName, timeServerIP); 
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  uint32_t beginWait = millis();
  while (millis() - beginWait < 3000) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      prevDisplay = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}
// send an NTP request to the time server at the given address
//void sendNTPpacket(IPAddress &address)
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
