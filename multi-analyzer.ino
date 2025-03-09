#include <ModbusMaster.h>
#include <math.h>
#include <SoftwareSerial.h>

#include "MeterConfig.h";

#define RXPin        16  // Serial Receive pin
#define TXPin        17  // Serial Transmit pin

#if ___POLARSTAR == 1
#include "POLARSTAR.h";
// Serial.println("POLARSTAR");
#endif

#if ___LOVATO == 1
#include "LOVATO.h";
// Serial.println("LOVATO");
#endif

SoftwareSerial RS485Serial(RXPin, TXPin);  // RX, TX pins for SoftwareSerial (GPIO16 and GPIO17 in this example)


int ConnectedMeters[][4]    = {{_POLARSTAR, 1, 9600, SWSERIAL_8N1}, {_LOVATO, 2, 9600, SWSERIAL_8N1} };  // { {Meter Model, Meter ID, Baud Rate, Serial Config (databits, parity, stop bit)} , .... }

int NumberOfMeters = 0;

#define NumberOfMeters (sizeof(ConnectedMeters) / sizeof(int)) / 4
float Acquired_Data[NumberOfMeters][140];
#define ParameterFailureValue 77.7

ModbusMaster nodePolarstar, nodeLovato;

// Function to convert binary to Decimal
int binaryToDecimal(int n) {
    int dec_value = 0;
    int base = 1;
    int temp = n;
    while (temp) {
        int last_digit = temp % 10;
        temp = temp / 10;
        dec_value += last_digit * base;
        base = base * 2;
    }
    return dec_value;
}

// Function to convert BCD to Decimal
uint32_t BCDToDecimal(uint32_t nDecimalValue) {
    uint32_t nResult = 0;
    int ncnt;
    int anHexValueStored[8];
    uint16_t unflag = 0;

    // Extract each hexadecimal value from the BCD value
    for (ncnt = 7; ncnt >= 0; ncnt--) {
        anHexValueStored[ncnt] = nDecimalValue & (0x0000000F << 4 * (7 - ncnt));  // Mask each 4-bit segment
        anHexValueStored[ncnt] = anHexValueStored[ncnt] >> 4 * (7 - ncnt);  // Shift it to the right

        // Check for invalid BCD values (values greater than 9)
        if (anHexValueStored[ncnt] > 9)
            unflag = 1;
    }

    // If invalid BCD value is detected, return 0
    if (unflag == 1) {
        return 0;
    } else {
        // Convert BCD digits to decimal
        for (ncnt = 0; ncnt < 8; ncnt++) {
            nResult = nResult + anHexValueStored[ncnt] * round(pow(10, (7 - ncnt)));  // Scale each digit
        }
        return nResult;
    }
}
    
void setup() {
  Serial.begin(115200);  // Initialize Serial Communication for Debugging
  RS485Serial.begin(9600, SWSERIAL_8N1, 16, 17);  // Set up SoftwareSerial communication at 9600 baud rate
  // Initialize Polarstar meter (ID 1)
  //nodePolarstar.begin(1, RS485Serial);
  // Serial.println("Polarstar communication initialized.");

    
  // Initialize Lovato meter (ID 2)
  // nodeLovato.begin(2, RS485Serial);
  // Serial.println("Lovato communication initialized.");
}

void ReadPolarstar() {
  Serial.println("POLARSTAR:  ");  // Print meter name before data


  int NumberOfParameters = sizeof(POLARSTAR) / sizeof(int);
  int MeterNo = 0;
  uint8_t j, result;
  int16_t datas[2];
  String dataString = "";
  nodePolarstar.begin(1, RS485Serial);

  for (int param = 0; param < NumberOfParameters; param++) {
    if (POLARSTAR[param] == 0xFFFF) {
      Acquired_Data[MeterNo][param] = ParameterFailureValue;
    } else {
      result = nodePolarstar.readInputRegisters(POLARSTAR[param], 2);  // Fetch two registers

      if (result == nodePolarstar.ku8MBSuccess) {
        // Serial.print("POLARSTAR: ");

        // Store the values from the response buffer
        for (uint8_t j = 0; j < 2; j++) {
          datas[j] = nodePolarstar.getResponseBuffer(j);
        }

        // Convert the  register to BCD and scale it
        Acquired_Data[MeterNo][param] = BCDToDecimal(datas[0]) * pow(10, binaryToDecimal(datas[1]));
      }

      if (isnan(Acquired_Data[MeterNo][param])) {
        Acquired_Data[MeterNo][param] = ParameterFailureValue;
      }
    }

    // If there's a response timeout, break out of the loop
    if (result == nodePolarstar.ku8MBResponseTimedOut) {
      Serial.println("Response Timeout...");
      break;  // Exit the loop on timeout
    }
    else {
      
      Serial.print(F(" "));
      Serial.print(Acquired_Data[MeterNo][param]);
      dataString += String(Acquired_Data[MeterNo][param]);
      
      if (param < NumberOfParameters - 1) {
        dataString += ",";
      }
    }

    delay(10);  // Short delay before reading next parameter
  }
  Serial.println();
}

void ReadLovato() {
  Serial.println("LOVATO:  ");  // Print meter name before data


  int NumberOfParameters = 0;
  NumberOfParameters = sizeof(parameters) / sizeof(int);

  
  int MeterNo = 0;
  uint8_t j, result;
  int16_t datas[2];
  uint16_t high, low;
  uint32_t value;
  nodeLovato.begin(2, RS485Serial);

  String dataString = "";  // Clear previous data

  for (int i = 0; i < sizeof(parameters) / sizeof(parameters[0]); i++) {
    result = nodeLovato.readInputRegisters(parameters[i].registerAddress, parameters[i].words);

    if (result == nodeLovato.ku8MBSuccess) {
      high = nodeLovato.getResponseBuffer(0);
      low = nodeLovato.getResponseBuffer(1);
      value = ((uint32_t)high << 16) | low;

      dataString += String(value / parameters[i].scaleFactor);
    } else {
      dataString += String(ParameterFailureValue); // Print failure value instead of error
    }

    if (i < sizeof(parameters) / sizeof(parameters[0]) - 1) {
      dataString += " ";  // Add comma except for last value
    }
  }

  Serial.println(dataString);  // Print all values in one line
}



void loop() {
    ReadPolarstar();  // Fetch data from Polarstar
    delay(500);
    ReadLovato();  // Fetch data from Lovato
    delay(500);
}

