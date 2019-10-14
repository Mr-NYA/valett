#include <Arduino.h>
#include <BLEPeripheral.h>

// Type de robot
char Device_name[255]="Valett-dev";
char Local_name[255]="Valett-dev";
unsigned char  manufacturerData []="Y  Valett-dev";

//custom boards may override default pin definitions with BLEPeripheral(PIN_REQ, PIN_RDY, PIN_RST)
BLEPeripheral blePeripheral = BLEPeripheral();

// create service
BLEService valettService = BLEService("4010");

// create switch characteristic
BLECharacteristic moteurCharacteristic = BLECharacteristic("4011", BLEWrite, 255);
BLEIntCharacteristic timeoutCharacteristic = BLEIntCharacteristic("4012", BLEWrite|BLERead);
BLEIntCharacteristic modeCharacteristic = BLEIntCharacteristic("4013", BLEWrite|BLERead);
BLECharacteristic ledCharacteristic = BLECharacteristic("4014", BLEWrite|BLERead, 3);
BLECharacteristic nameCharacteristic = BLECharacteristic("4015", BLEWrite|BLERead, 255);

// Moteur latéral gauche/droite
int Pin1 = 0; 
int Pin2 = 1; 
int Pin3 = 2; 
int Pin4 = 3; 

// Moteur de basculement avant/arrière
int Pin5 = 6; 
int Pin6 = 7; 
int Pin7 = 8; 
int Pin8 = 9; 

// Leds
int ledGreen = A0; 
int ledBlue = A1; 
int ledRed = AREF; 

// Détection de fin de course gauche et droite
boolean Detect = true;
// Capteurs de fin de course gauche et droite
int PinStopG = A4;
int PinStopD = A5;
// Valeur des capteurs en fin de cours
int SignalCapteur = 1;

// Etapes de chaque moteur
int step1 = 0;
int step2 = 0;

// Vitesse de déplacement de chaque moteur
int move1=0;
int move2=0;

// Mode pour les vitesses des steps (1 -> 8 steps ou 2 -> 4 steps ou 3 -> 2 steps)
unsigned int mode=1;

// Délai en ms entre chaque étape de déplacement qui sera divisé par la vitesse
// Un délai inférieur fait que le moteur ne tournera pas à vitesse 10 car delai/10 sera < 2ms
int delai=10; 
// Ratio de vitesse entre le moteur horizontal et le moteur vertical
int ratio=1;

// Nombre d'étapes pour chaque commande
// TESTER AVEC UNE VALEUR PLUS GRANDE COMME 80
int nbstep=8;

// Pour gérer le timeout, 0 pas de timeout
unsigned long the_time;
unsigned int the_timeout=1;

// Buffers
char readbuf[256] ;
char printbuf[256] ;
char Led[4];
char rouge[]="100";
char vert[]="010";
char bleu[]="001";
char noir[]="000";

// Commandes de déplacement d'un moteur pas à pas
// Respecter un délai de 2 ms entre chaque étape
void stepper(int *step, int move, int P1, int P2, int P3, int P4);
void stepper4(int *step, int move, int P1, int P2, int P3, int P4);
void stop(int P1, int P2, int P3, int P4);
void setled(char *led,boolean);

// Traitement des commandes des moteurs
void cmdMoteur(char *array);
void mvtMoteur(void);

void setup() {
//initialisation de serial Port
//Serial.begin(115200);
  Serial.begin(9600);
  // set advertised local name and service UUID
  blePeripheral.setLocalName(Local_name); 
  blePeripheral.setDeviceName(Device_name);
  blePeripheral.setManufacturerData(manufacturerData,sizeof(manufacturerData)-1);
  blePeripheral.setAppearance(true);
  blePeripheral.setAdvertisedServiceUuid(valettService.uuid());

  // add service and characteristic
  blePeripheral.addAttribute(valettService);
  blePeripheral.addAttribute(moteurCharacteristic);
  blePeripheral.addAttribute(timeoutCharacteristic);
  timeoutCharacteristic.setValue(the_timeout);
  blePeripheral.addAttribute(modeCharacteristic);
  modeCharacteristic.setValue(mode);
  blePeripheral.addAttribute(ledCharacteristic);

  //blePeripheral.addAttribute(nameCharacteristic);
  //nameCharacteristic.setValue((unsigned char *)Local_name,strlen(Local_name));

  // begin initialization
  blePeripheral.begin();

  sprintf(printbuf,"BLE <%s> Peripheral",Local_name);   
  Serial.println(printbuf);
  // Moteur de basculement avant/arrière
  pinMode(Pin1, OUTPUT); 
  pinMode(Pin2, OUTPUT); 
  pinMode(Pin3, OUTPUT); 
  pinMode(Pin4, OUTPUT); 
  // Moteur latéral gauche/droite
  pinMode(Pin5, OUTPUT); 
  pinMode(Pin6, OUTPUT); 
  pinMode(Pin7, OUTPUT); 
  pinMode(Pin8, OUTPUT); 
  // Capteurs de fin de course gauche et droite
  if (Detect) {
      pinMode(PinStopG, INPUT); 
      pinMode(PinStopD, INPUT); 
  }
  // Led 
  pinMode(ledBlue, OUTPUT); 
  pinMode(ledGreen, OUTPUT); 
  pinMode(ledRed, OUTPUT);
  
  delay(500);
  setled(rouge,true);
  }

void setled(char *led,boolean bo) {
  if (led[0]=='0') digitalWrite(ledRed, LOW); else digitalWrite(ledRed, HIGH); 
  if (led[1]=='0') digitalWrite(ledGreen, LOW); else digitalWrite(ledGreen, HIGH);
  if (led[2]=='0') digitalWrite(ledBlue, LOW); else digitalWrite(ledBlue, HIGH);
  ledCharacteristic.setValue(led);
  if (bo) strcpy(Led,led);
}

void loop() {
  boolean connected=false;
  BLECentral central = blePeripheral.central();
  if (central) {                   
    while (central.connected()) {      
      if (not connected) {
        setled(vert,true);
        connected=true;
      }
    // Données bluetooth disponibles 
      if (moteurCharacteristic.written()) {   
        setled(bleu,false);
        // lire les données envoyées par bluetooth de l'appareil
        strncpy(readbuf,(char*)moteurCharacteristic.value(),moteurCharacteristic.valueLength());
        readbuf[moteurCharacteristic.valueLength()]=0;
        //sprintf(printbuf,"Moteur <%s> %d",readbuf,moteurCharacteristic.valueLength());   
        //Serial.println(printbuf);
        //delay(250);    
        //faire appel à la méthode de traitement de la commande
        cmdMoteur(readbuf);   
        setled(Led,false);
      }
      else if (timeoutCharacteristic.written()) {
        the_timeout=timeoutCharacteristic.value();
        sprintf(printbuf,"Timeout %d",the_timeout);   
        Serial.println(printbuf);
        timeoutCharacteristic.setValue(the_timeout);
      } 
      else if (modeCharacteristic.written()) {  
        int m;
        m=modeCharacteristic.value();
        if (m >=1 && m<=8) {
          mode=modeCharacteristic.value();
          sprintf(printbuf,"Mode %d",mode);   
          Serial.println(printbuf);
          timeoutCharacteristic.setValue(the_timeout);
        }
      else {
          sprintf(printbuf,"Mode %d error value",modeCharacteristic.value());   
          Serial.println(printbuf);
        }
      } 
      else if (ledCharacteristic.written()) {  
        strncpy(readbuf,(char*)ledCharacteristic.value(),nameCharacteristic.valueLength());
        readbuf[3]=0;
        sprintf(printbuf,"Led <%s>",readbuf);   
        Serial.println(printbuf);
        setled(readbuf,true);
      } 
      else if (nameCharacteristic.written()) {  
        strncpy(readbuf,(char*)nameCharacteristic.value(),nameCharacteristic.valueLength());
        readbuf[nameCharacteristic.valueLength()]=0;
        sprintf(printbuf,"Nom <%s> %d",readbuf,nameCharacteristic.valueLength());   
        Serial.println(printbuf);
        strcpy(Local_name,readbuf);
        //blePeripheral = BLEPeripheral();
        //setup();
        //blePeripheral.setAdvertisedServiceUuid(valettService.uuid());
        nameCharacteristic.setValue((unsigned char *)Local_name,strlen(Local_name));
      } 
    mvtMoteur();
    }
  // central disconnected  
  sprintf(printbuf,"Disconnected from <%s> ",Local_name);   
  Serial.print(printbuf);
  Serial.println(central.address());  
  setled(rouge,true);
  connected=false;
  }  
}

void mvtMoteur(void) {
  if (move1 != 0 || move2 != 0) { // Dernière commande de déplacement
    // timeout
    if (the_timeout !=0 && (millis() - the_time >= the_timeout*1000L)) {
        sprintf(printbuf,"Time out %d seconds",the_timeout);
        Serial.println(printbuf);
        move1=move2=0;
        step1=step2=0;
        stop(Pin1,Pin2,Pin3,Pin4);
        stop(Pin5,Pin6,Pin7,Pin8);
    }
    // test fin de course à droite ou à gauche
    if (Detect && move2 != 0) {
        int val;
        if (move2 < 0) val=digitalRead(PinStopG);
        else val=digitalRead(PinStopD);
        if (val == SignalCapteur) {
            move2 = 0;  // pas de déplacement à droite
            step2=0;
            stop(Pin5,Pin6,Pin7,Pin8);
        }
    }

    if (move1 != 0 || move2 != 0) {
        int rat=ratio;
        //Serial.print(".");
        for (int x=0;x<nbstep;x++) {
            if (move1 != 0) {
              if (mode==4) stepper4(&step1,move1,Pin1,Pin2,Pin3,Pin4);
              else stepper(&step1,move1,Pin1,Pin2,Pin3,Pin4);
            }
            if (move2 != 0 and rat==1) {
              if (mode==4) stepper4(&step2,move2,Pin5,Pin6,Pin7,Pin8);
              else stepper(&step2,move2,Pin5,Pin6,Pin7,Pin8);
            }
            if (rat==1) rat=ratio; else rat--;
            if (move1 != 0 and move2 == 0)
                delay(delai/abs(move1));
            else if (move2 != 0 and move1 == 0)
                delay(delai/abs(move2));
            else
                // A améliorer pour vitesses indépendantes
                delay(delai/max(abs(move1),abs(move2)));
          }
      }
  }
}

void cmdMoteur(char * array){
  unsigned int i;
  move1=move2=0;
  // première valeur : Vitesse basculement avant/arrière
    move1=atoi(array);
    if (move1 < -10) move1=-10;
    else if (move1 > 10) move1=10;
    // seconde valeur : Vitesse latérale gauche/droite
    for (i=0;i < strlen(array) && array[i] != ' ';i++);
    if (i < strlen(array)) {
      move2=atoi(array+i);
      if (move2 < -10) move2=-10;
      else if (move2 > 10) move2=10;
    }
    //sprintf(printbuf,"Move %d %d",move1,move2);   
    //Serial.println(printbuf);
    if (move1==0) { stop(Pin1,Pin2,Pin3,Pin4);step1=0;}
    if (move2==0) { stop(Pin5,Pin6,Pin7,Pin8);step2=0;}
    if (move1 || move2) {
      the_time=millis();
      //mvtMoteur();
    }
}

void stop(int P1, int P2, int P3, int P4) {
  digitalWrite(P1, LOW); 
  digitalWrite(P2, LOW);
  digitalWrite(P3, LOW);
  digitalWrite(P4, LOW);
}

void stepper(int *step, int move,int P1, int P2, int P3, int P4) {
    switch(*step){
      case 0:
        digitalWrite(P1, LOW);
        digitalWrite(P2, LOW);
        digitalWrite(P3, LOW);
        digitalWrite(P4, HIGH);
      break; 
      case 1:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, HIGH);
        digitalWrite(P4, HIGH);
      break; 
      case 2:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, HIGH);
        digitalWrite(P4, LOW);
      break; 
      case 3:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, HIGH);
        digitalWrite(P3, HIGH);
        digitalWrite(P4, LOW);
      break; 
      case 4:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, HIGH);
        digitalWrite(P3, LOW);
        digitalWrite(P4, LOW);
      break; 
      case 5:
        digitalWrite(P1, HIGH); 
        digitalWrite(P2, HIGH);
        digitalWrite(P3, LOW);
        digitalWrite(P4, LOW);
      break; 
      case 6:
        digitalWrite(P1, HIGH); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, LOW);
        digitalWrite(P4, LOW);
      break; 
      case 7:
        digitalWrite(P1, HIGH); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, LOW);
        digitalWrite(P4, HIGH);
      break;
      default:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, LOW);
        digitalWrite(P4, LOW);
      break; 
    }
  if (move < 0) (*step)+=mode; else (*step)-=mode;
  if(*step>7) *step=0;
  if(*step<0) *step=7;
}

void stepper4(int *step, int move,int P1, int P2, int P3, int P4) {
  switch(*step){
    case 0:  // 1010
       digitalWrite(P1, HIGH);
       digitalWrite(P2, LOW);
       digitalWrite(P3, HIGH);
       digitalWrite(P4, LOW);
    break;
    case 1:  // 0110
       digitalWrite(P1, LOW);
       digitalWrite(P2, HIGH);
       digitalWrite(P3, HIGH);
       digitalWrite(P4, LOW);
    break;
    case 2:  //0101
       digitalWrite(P1, LOW);
       digitalWrite(P2, HIGH);
       digitalWrite(P3, LOW);
       digitalWrite(P4, HIGH);
    break;
    case 3:  //1001
       digitalWrite(P1, HIGH);
       digitalWrite(P2, LOW);
       digitalWrite(P3, LOW);
       digitalWrite(P4, HIGH);
    break;
    default:
        digitalWrite(P1, LOW); 
        digitalWrite(P2, LOW);
        digitalWrite(P3, LOW);
        digitalWrite(P4, LOW);
      break; 
    }
  if (move < 0) (*step)++; else (*step)--;
  if(*step>3) *step=0;
  if(*step<0) *step=3;
}
