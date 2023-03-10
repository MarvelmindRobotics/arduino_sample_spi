/*
  The circuit:
 *HC-SR04 Distance Sensor Trig pin to digital pin 2
 *HC-SR04 Distance Sensor Echo pin to digital pin 3
 * LCD RS pin to digital pin 8
 * LCD Enable pin to digital pin 9
 * LCD D4 pin to digital pin 4
 * LCD D5 pin to digital pin 5
 * LCD D6 pin to digital pin 6
 * LCD D7 pin to digital pin 7
 * LCD BL pin to digital pin 10
 * HEDGEHOG SPI CS to digital pin 1 (connect to hedgehog through resistor divider to provide +3.3V level on hedgehog from +5V Arduino output)
 * HEDGEHOG SPI MISO to digital pin 12
 * HEDGEHOG SPI SCK to digital pin 13 
 *Vcc pin to  +5
 */

#include <stdlib.h>
#include <LiquidCrystal.h>
#include <SPI.h>

#define SERIAL_MONITOR_PRINT

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//  MARVELMIND HEDGEHOG RELATED PART

long hedgehog_x, hedgehog_y;// coordinates of hedgehog (X,Y), mm
long hedgehog_z;// height of hedgehog, mm
int hedgehog_pos_updated;// flag of new data from hedgehog received

int64_t pos_timestamp;

bool high_resolution_mode;
bool realtime_timestamps;

///

//#define DISTANCE_SENSOR_ENABLED

#define HEDGEHOG_CS_PIN 1

#define HEDGEHOG_BUF_SIZE 60 
#define HEDGEHOG_PACKET_LOWRES_SIZE 23
#define HEDGEHOG_PACKET_HIGHRES_SIZE 29
#define HEDGEHOG_PACKET_NT_HIGHRES_SIZE 33
byte hedgehog_spi_buf[HEDGEHOG_BUF_SIZE];

const int hedgehog_packet_lowres_header[5]= {0xff, 0x47, 0x01, 0x00, 0x10};
const int hedgehog_packet_highres_header[5]= {0xff, 0x47, 0x11, 0x00, 0x16};
const int hedgehog_packet_nt_highres_header[5]= {0xff, 0x47, 0x81, 0x00, 0x00};

typedef union {byte b[2]; unsigned int w;int wi;} uni_8x2_16;
typedef union {byte b[4];float f;unsigned long v32;long vi32;} uni_8x4_32;
typedef union {byte b[8];int64_t vi64;} uni_8x8_64;

//    Marvelmind hedgehog support initialize
void setup_hedgehog() 
{
  #ifdef SERIAL_MONITOR_PRINT
  Serial.begin(500000);
  #endif
  
  // start the SPI library
  SPI.begin();

  pinMode(HEDGEHOG_CS_PIN,OUTPUT);// set chip select for hedgehog  
  digitalWrite(HEDGEHOG_CS_PIN, HIGH);

  hedgehog_pos_updated= 0;

  SPI.setClockDivider(SPI_CLOCK_DIV8);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
}

// Marvelmind hedgehog service loop
void loop_hedgehog()
{int incoming_byte;
 int i, ofs;
 int packet_received;
 int packet_size= 23;
 const int *header= &hedgehog_packet_lowres_header[0];
 uni_8x2_16 un16;
 uni_8x4_32 un32;
 uni_8x8_64 un64;

  packet_received= 1;

  digitalWrite(HEDGEHOG_CS_PIN, LOW);// hedgehog chip select 
  delayMicroseconds(50);

  for(i=0;i<packet_size;i++)
    {
      incoming_byte= SPI.transfer(0x00);// read byte from hedgehog  

      if (i == 2)
        {
          if (incoming_byte == hedgehog_packet_lowres_header[i]) 
            {
              packet_size= HEDGEHOG_PACKET_LOWRES_SIZE;
              header= &hedgehog_packet_lowres_header[0];
              high_resolution_mode= false;
              realtime_timestamps= false;
            }
           else if (incoming_byte == hedgehog_packet_highres_header[i]) 
            {
              packet_size= HEDGEHOG_PACKET_HIGHRES_SIZE;
              header= &hedgehog_packet_highres_header[0];
              high_resolution_mode= true;    
              realtime_timestamps= false;   
            }       
          else 
            {
              packet_size= HEDGEHOG_PACKET_NT_HIGHRES_SIZE;
              header= &hedgehog_packet_nt_highres_header[0];
              high_resolution_mode= true;
              realtime_timestamps= true;                                                         
            }
        }

      hedgehog_spi_buf[i]= incoming_byte;  
      
      // check first 4 bytes for constant value
      if (i<4)
        {
          if (incoming_byte != header[i]) 
            {
              packet_received= 0;// packet header error
            }
        }     
    }     

  delayMicroseconds(50);
  digitalWrite(HEDGEHOG_CS_PIN, HIGH);// hedgehog chip unselect   

  if (packet_received)  
    {
      hedgehog_set_crc16(&hedgehog_spi_buf[0], packet_size);// calculate CRC checksum of packet
      if ((hedgehog_spi_buf[packet_size] == 0)&&(hedgehog_spi_buf[packet_size+1] == 0))
        {// checksum success
          if (!high_resolution_mode)
            {
              // coordinates of hedgehog (X,Y), cm ==> mm
              un16.b[0]= hedgehog_spi_buf[9];
              un16.b[1]= hedgehog_spi_buf[10];
              hedgehog_x= 10*long(un16.wi);

              un16.b[0]= hedgehog_spi_buf[11];
              un16.b[1]= hedgehog_spi_buf[12];
              hedgehog_y= 10*long(un16.wi);
              
              // height of hedgehog, cm==>mm (FW V3.97+)
              un16.b[0]= hedgehog_spi_buf[13];
              un16.b[1]= hedgehog_spi_buf[14];
              hedgehog_z= 10*long(un16.wi);
              
              hedgehog_pos_updated= 1;// flag of new data from hedgehog received
            }
           else
            {
              if (!realtime_timestamps) {
                un32.b[0]= hedgehog_spi_buf[5];
                un32.b[1]= hedgehog_spi_buf[6];
                un32.b[2]= hedgehog_spi_buf[7];
                un32.b[3]= hedgehog_spi_buf[8];
                pos_timestamp= un32.vi32;

                ofs= 9;
              } else {
                un64.b[0]= hedgehog_spi_buf[5];
                un64.b[1]= hedgehog_spi_buf[6];
                un64.b[2]= hedgehog_spi_buf[7];
                un64.b[3]= hedgehog_spi_buf[8];
                un64.b[4]= hedgehog_spi_buf[9];
                un64.b[5]= hedgehog_spi_buf[10];
                un64.b[6]= hedgehog_spi_buf[11];
                un64.b[7]= hedgehog_spi_buf[12];
                pos_timestamp= un64.vi64;

                ofs= 13;
              }
              
              // coordinates of hedgehog (X,Y), mm
              un32.b[0]= hedgehog_spi_buf[ofs+0];
              un32.b[1]= hedgehog_spi_buf[ofs+1];
              un32.b[2]= hedgehog_spi_buf[ofs+2];
              un32.b[3]= hedgehog_spi_buf[ofs+3];
              hedgehog_x= un32.vi32;

              un32.b[0]= hedgehog_spi_buf[ofs+4];
              un32.b[1]= hedgehog_spi_buf[ofs+5];
              un32.b[2]= hedgehog_spi_buf[ofs+6];
              un32.b[3]= hedgehog_spi_buf[ofs+7];
              hedgehog_y= un32.vi32;
              
              // height of hedgehog, mm 
              un32.b[0]= hedgehog_spi_buf[ofs+8];
              un32.b[1]= hedgehog_spi_buf[ofs+9];
              un32.b[2]= hedgehog_spi_buf[ofs+10];
              un32.b[3]= hedgehog_spi_buf[ofs+11];
              hedgehog_z= un32.vi32;

              hedgehog_pos_updated= 1;// flag of new data from hedgehog received
            }
        }
    }
}

// Calculate CRC-16 of hedgehog packet
void hedgehog_set_crc16(byte *buf, byte size)
{uni_8x2_16 sum;
 byte shift_cnt;
 byte byte_cnt;

  sum.w=0xffffU;

  for(byte_cnt=size; byte_cnt>0; byte_cnt--)
   {
   sum.w=(unsigned int) ((sum.w/256U)*256U + ((sum.w%256U)^(buf[size-byte_cnt])));

     for(shift_cnt=0; shift_cnt<8; shift_cnt++)
       {
         if((sum.w&0x1)==1) sum.w=(unsigned int)((sum.w>>1)^0xa001U);
                       else sum.w>>=1;
       }
   }

  buf[size]=sum.b[0];
  buf[size+1]=sum.b[1];// little endian
}// hedgehog_set_crc16

//  END OF MARVELMIND HEDGEHOG RELATED PART
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////


#define CM 1      //Centimeter
#define INC 0     //Inch
#define TP 2      //Trig_pin
#define EP 3      //Echo_pin

LiquidCrystal lcd(8, 13, 9, 4, 5, 6, 7);

void print_out(const char *buf) 
{
  lcd.print(buf);
  #ifdef SERIAL_MONITOR_PRINT
  Serial.write(buf);
  #endif
}

void setup()
{
  lcd.clear(); 
  lcd.begin(16, 2);
  lcd.setCursor(0,0); 
  pinMode(TP,OUTPUT);       // set TP output for trigger  
  pinMode(EP,INPUT);        // set EP input for echo

  lcd.setCursor(0,0); 
  print_out("Trying");
  lcd.setCursor(0,1); 
  print_out("SPI read");
  #ifdef SERIAL_MONITOR_PRINT
  Serial.write("\r\n");
  #endif  


  setup_hedgehog();//    Marvelmind hedgehog support initialize
}

void loop()
{  byte lcd_coord_precision;
   char lcd_buf[12];

#ifdef DISTANCE_SENSOR_ENABLED
   long microseconds = TP_init();
   long distacne_cm = Distance(microseconds, CM);
   lcd.setCursor(11,0); 
   lcd.print("D="); 
   lcd.print(distacne_cm); 
   lcd.print("  "); 
#endif

   loop_hedgehog();// Marvelmind hedgehog service loop

   if (hedgehog_pos_updated)
     {// new data from hedgehog available
       hedgehog_pos_updated= 0;// clear new data flag 
       // output hedgehog position to LCD

       if (high_resolution_mode)
        {
          lcd_coord_precision= 3;
        }
       else
        {
          lcd_coord_precision= 2; 
        }
       
       lcd.setCursor(0,0); 
       print_out("X=");
       dtostrf(((float) hedgehog_x)/1000.0f, 4, lcd_coord_precision, lcd_buf);
       print_out(lcd_buf);
       print_out("   ");  
       
       lcd.setCursor(0,1);
       print_out("Y=");
       dtostrf(((float) hedgehog_y)/1000.0f, 4, lcd_coord_precision, lcd_buf);
       print_out(lcd_buf);  
       print_out("   ");  
       
       lcd.setCursor(9,1); 
       print_out("Z=");  
       dtostrf(((float) hedgehog_z)/1000.0f, 4, lcd_coord_precision, lcd_buf);
       print_out(lcd_buf);
       print_out("  ");  
       #ifdef SERIAL_MONITOR_PRINT
       Serial.write("\r\n");
       #endif
     }

  delay(20);
}

long Distance(long time, int flag)
{
  /*
  
  */
  long distacne;
  if(flag)
    distacne = time /29 / 2  ;     // Distance_CM  = ((Duration of high level)*(Sonic :340m/s))/2
                                   //              = ((Duration of high level)*(Sonic :0.034 cm/us))/2
                                   //              = ((Duration of high level)/(Sonic :29.4 cm/us))/2
  else
    distacne = time / 74 / 2;      // INC
  return distacne;
}

long TP_init()
{                     
  digitalWrite(TP, LOW);                    
  delayMicroseconds(2);
  digitalWrite(TP, HIGH);                 // pull the Trig pin to high level for more than 10us impulse 
  delayMicroseconds(10);
  digitalWrite(TP, LOW);
  long microseconds = pulseIn(EP,HIGH);   // waits for the pin to go HIGH, and returns the length of the pulse in microseconds
  return microseconds;                    // return microseconds
}
