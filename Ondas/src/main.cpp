#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PinChangeInterrupt.h>



//Entradas e conexões(modificar de acordo com as conexões escolhidas no arduino)
const int servoPin = 3;
const int ultraEchoPin = 12;
const int ultraTrigPin = 13;
const int button1Pin = 7;//Amarelo
const int button2Pin = 8;//Azul
const int button3Pin = 9;//Verde
const int button4Pin = 10;//Vermelho
//const int displaySCLPin; não é necessário especificar
//const int displaySDAPin; não é necessário especificar


//Variáveis para controle do programa:----------------------------------------------------------------
Servo radarMotor;//Cria um objeto para controle do servomotor
LiquidCrystal_I2C lcd(0x27, 16, 2);//Cria objeto para controle de um display LCD formato -> (16:2)
int modo = 0;//Guarda o modo de operação selecionado
float VelocidadeSom = 0.0;//Guarda a Velocidade do Som que vamos usar no código
const float distancia = 12;//Distância em centimetros (10E-2) do S.Ultrassônico até o obstáculo de apoio.
long int tempoDeMedicao = 7000;// sete segundos
bool AcessoLiberado  = false;//diz se podemos utilizar as outras funções


//Variáveis para ajustes de tempo nas funções:
  //Debounce dos botões na interrupção:
    long int ultimaInterrupcao = 0;
    const int tempoDebounce = 50;
  /*Controlar o tempo de atualização do LCD para não sobrecarregá-lo(Não se preocupem muito com isso, 
  se vacilarmos com isso, o máximo que acontece é o display não conseguir escrever nada pq ta atualizando
  rapido demais, não possui o risco de queimar, nem nada sério)*/
    long int ultimaAtualizacaoLCD = 0;
    const int tempoDeSeguranca = 1500;
  /*Controlar o tempo de exibição de uma mensagem no display após acionamento de um novo modo de operação.
  A gente faz isso pq se usarmos uma função de "pausa" como o delay(), ele para o programa completamente 
  durante o tempo selecionado, aqui não, a gente só verifica se o tempo selecionado já passou sem pausar o 
  programa:*/
    long int momentoAtivacao = 0;
    const int tempoMensagem = 4000;//Ativa a mensagem por 4 segundos



//Funções para funcionamento do dispositivo:---------------------------------------------------------------------------------
void select();//- altera o valor da variável (modo) com uma interrupção
void modoOperacao();//- Verifica a variável (modo) e altera o modo de operação do dispositivo
void medirSom();//- Modo de operação: Medir Velocidade do som!
void medirDistancia();//- Modo de operação: Medir distância de um objeto!
void medirVelocidade();//- Modo de operação: Medir Velocidade de um objeto!
void modoRadar();//- Modo de operação: Modo radar!



/*O SetUp() serve para fazer todas as inicializações e configurações necessárias para funcionamento do programa.
Ele roda primeiro que o loop e roda apenas UMA vez. Necessário para comunicação com o arduino!!!
*/
void setup() {
  //Comunicação serial (Para debugar)
  Serial.begin(9600);

  //Inicializa o Servomotor
  radarMotor.attach(servoPin);

  //inicializar botões
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(button4Pin, INPUT_PULLUP);
  
  // Configura os pinos do sensor ultrassônico
  pinMode(ultraTrigPin, OUTPUT);
  pinMode(ultraEchoPin, INPUT);

  //Apresentação(Mensagem ao ligar):
  lcd.init();//Inicializa o display
  lcd.backlight();//Liga a luz de fundo do display
  lcd.setCursor(0, 0);// Poe na 1ª linha
  lcd.print("Ondas Sonoras");
  lcd.setCursor(0, 1);// Põe na 2ª linha
  lcd.print("Fis exp A...");
  delay(2500);//Aguarda 2,5 segundos e troca a mensagem:
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aperte amarelo");
  lcd.setCursor(0, 1);
  lcd.print("para medir V.som");
  

  //Ativa a interrupção do código a partir dos botões e chama a função select:
  attachInterrupt(digitalPinToPCINT(button1Pin), select, RISING);//Acionar interrupção
  attachInterrupt(digitalPinToPCINT(button2Pin), select, RISING);//Acionar interrupção
  attachInterrupt(digitalPinToPCINT(button3Pin), select, RISING);//Acionar interrupção
  attachInterrupt(digitalPinToPCINT(button4Pin), select, RISING);//Acionar interrupção

  
}

/*O Loop() executa o que tem dentro dele infinitas vezes igual um While(1). Necessário para comunicação com o 
arduino!!!
*/
void loop() {
  modoOperacao();//Vai executar uma função do dispositivo de acordo com o valor da variável MODO
}


void select(){
  if ((millis() - ultimaInterrupcao) > tempoDebounce) {
    
    if (digitalRead(button1Pin) == LOW) {
      modo = 1;         
      momentoAtivacao = millis();
    } 
    else if (digitalRead(button2Pin) == LOW && VelocidadeSom>0.0) {
      modo = 2;         
    } 
    else if (digitalRead(button3Pin) == LOW && VelocidadeSom>0.0) {
      modo = 3;            
    } 
    else if (digitalRead(button4Pin) == LOW && VelocidadeSom>0.0) {
      modo = 4;         
      
    }
    
    ultimaInterrupcao = millis();
  }

}

void modoOperacao(){
  static int modoAnterior = 0;

  if(!modo){
    return;
  }else{

    //retirar dps(tODAS AS OPERAÇÕES COM O DISPLAY VAO FICAR DENTRO DAS FUNÇÕES)
    if(modoAnterior != modo){
      lcd.clear(); // Limpa o display
    }

    switch (modo) {
      case 1:
        medirSom();
        modoAnterior = 1;
        break;
      case 2:
        medirDistancia();
        modoAnterior = 2;
        break;
      case 3:
        medirVelocidade();
        modoAnterior = 3;
        break;
      case 4:
        modoRadar();
        modoAnterior = 4;
        break;
    }
  }
}

void medirSom(){
  if((millis() - momentoAtivacao) < tempoDeMedicao){
    radarMotor.write(180);

    AcessoLiberado = false;//Impede a troca de função

    //Ativar sensor ultrassônico
    delayMicroseconds(2);
    digitalWrite(ultraTrigPin,HIGH);//Envia pulso
    delayMicroseconds(10);
    digitalWrite(ultraTrigPin,LOW);
    
    static int duracao;
    duracao = pulseIn(ultraEchoPin,HIGH);//mede tempo que o pulso estava ligado (em microsegundos)
    
    //Armazenar a velocidade do som:
    VelocidadeSom = (20000.0*distancia/duracao);
  }else{
    AcessoLiberado = true;
    radarMotor.write(90);
  }


  if((millis() - momentoAtivacao)<=tempoMensagem){
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){
      
      Serial.print("Estou no loop 2. \n");
      
      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Modo 1:");
      lcd.setCursor(0, 1); 
      lcd.print("Medir Som...      ");
    }
    
  }else{
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){

      Serial.print("Estou no loop 3. \n");
      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Velocidade som:");
      lcd.setCursor(0, 1);
      lcd.print(VelocidadeSom);  lcd.print(" m/s");
    }

  }
}

void modoRadar(){
  lcd.setCursor(0, 0);
  lcd.print("Modo 4: ");
  lcd.setCursor(0, 1);
  lcd.print("Radar");

  /*AQUI NÃO É O CÓDIGO DO RADAR, COLOQUEI ESSE TRECHO DE CÓDIGO COMO TESTE AQUI SOMENTE PARA TESTAR O 
  FUNCIONAMENTO DOS BOTÕES! QUEM FOR POR O CÓDIGO DO RADAR, PODE COLOCAR AQUI E APAGAR ESSE!*/


  //faz o motor girar de um lado pro outroo
  
  static unsigned long ultimoMovimentoRadar = 0;
  static int anguloRadar = 0;
  static int passoRadar = 5;

  const long intervaloPasso = 70;
  unsigned long tempoAtual = millis();

  if((tempoAtual - ultimoMovimentoRadar) > intervaloPasso){
    ultimoMovimentoRadar = tempoAtual;
    radarMotor.write(anguloRadar);
    anguloRadar += passoRadar;

    if (anguloRadar >= 180) {
      anguloRadar = 180; 
      passoRadar = -5; 
    } else if (anguloRadar <= 0) {
      anguloRadar = 0;
      passoRadar = 5; 
    }

  }/**/
}

void medirDistancia(){
  lcd.setCursor(0, 0);
  lcd.print("Modo 2: ");
  lcd.setCursor(0, 1);
  lcd.print("Medir Distancia           ");

  
}

void medirVelocidade(){
  lcd.setCursor(0, 0);
  lcd.print("Modo 3: ");
  lcd.setCursor(0, 1);
  lcd.print("Medir Velocidade      ");
}

