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
const float distancia = 11;//Distância em centimetros (10E-2) do S.Ultrassônico até o obstáculo de apoio.
unsigned long int tempoDeMedicao = 7000;// sete segundos
bool AcessoLiberado  = false;//diz se podemos utilizar as outras funções


//Variáveis para ajustes de tempo nas funções:
  //Debounce dos botões na interrupção:
    unsigned long int ultimaInterrupcao = 0;
    const int tempoDebounce = 50;
  /*Controlar o tempo de atualização do LCD para não sobrecarregá-lo(Não se preocupem muito com isso, 
  se vacilarmos com isso, o máximo que acontece é o display não conseguir escrever nada pq ta atualizando
  rapido demais, não possui o risco de queimar, nem nada sério)*/
    unsigned long int ultimaAtualizacaoLCD = 0;
    const int tempoDeSeguranca = 1500;
  /*Controlar o tempo de exibição de uma mensagem no display após acionamento de um novo modo de operação.
  A gente faz isso pq se usarmos uma função de "pausa" como o delay(), ele para o programa completamente 
  durante o tempo selecionado, aqui não, a gente só verifica se o tempo selecionado já passou sem pausar o 
  programa:*/
    unsigned long int momentoAtivacao = 0;
    const int tempoMensagem = 4000;//Ativa a mensagem por 4 segundos



//Funções para funcionamento do dispositivo:---------------------------------------------------------------------------------
void select();//- altera o valor da variável (modo) com uma interrupção
void modoOperacao();//- Verifica a variável (modo) e altera o modo de operação do dispositivo
void medirSom();//- Modo de operação: Medir Velocidade do som!
void medirDistancia();//- Modo de operação: Medir distância de um objeto!
void medirVelocidade();//- Modo de operação: Medir Velocidade de um objeto!
void modoRadar();//- Modo de operação: Modo radar!
int radarCalcularDistancia();//Calcular a distancia no modo radar


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
      momentoAtivacao = millis();    
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
    
    static float duracao;
    duracao = pulseIn(ultraEchoPin,HIGH);//mede tempo que o pulso estava ligado (em microsegundos)

    //Armazenar a velocidade do som:
    VelocidadeSom = (20000.0*distancia/duracao);
    
  }else{
    AcessoLiberado = true;
    radarMotor.write(90);
  }


  if((millis() - momentoAtivacao)<=tempoMensagem){
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){
      
      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Modo 1:");
      lcd.setCursor(0, 1); 
      lcd.print("Medir Som...      ");
    }
    
  }else{
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){

      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Velocidade som:");
      lcd.setCursor(0, 1);
      lcd.print(VelocidadeSom);  lcd.print(" m/s");
    }

  }
}

void medirDistancia(){

  if((millis() - momentoAtivacao) < tempoDeMedicao){
    AcessoLiberado = false;
    //Vira o sensor para frente
    radarMotor.write(90);
    static float duracao;
    static float distanciaObjeto;

    //Ativar sensor ultrassônico
    delayMicroseconds(2);
    digitalWrite(ultraTrigPin,HIGH);//Envia pulso
    delayMicroseconds(10);
    digitalWrite(ultraTrigPin,LOW);
    
    
    duracao = pulseIn(ultraEchoPin,HIGH);//mede tempo que o pulso estava ligado (em microsegundos)

    
    distanciaObjeto = (VelocidadeSom * duracao/20000.0);
  }else{
    AcessoLiberado = true;
  }

  if((millis() - momentoAtivacao)<=tempoMensagem){
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){
      
      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Modo 2: ");
      lcd.setCursor(0, 1);
      lcd.print("Medir Distancia           ");
    }
    
  }else{
    if((millis() - ultimaAtualizacaoLCD)>tempoDeSeguranca){
      Serial.println(distanciaObjeto);

      ultimaAtualizacaoLCD = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Distancia:");
      lcd.setCursor(0, 1);
      lcd.print(distanciaObjeto);  lcd.print(" cm");
    }

  }

}

//Funcoes modo radar
int radarCalcularDistancia(){ 
  
  digitalWrite(ultraTrigPin, LOW); // << Modificado
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(ultraTrigPin, HIGH); // << Modificado
  delayMicroseconds(10);
  digitalWrite(ultraTrigPin, LOW); // << Modificado
  
  long duration = pulseIn(ultraEchoPin, HIGH); // << Modificado
  int distance = duration*0.034/2;
  return distance;
}

void modoRadar(){
  // Variáveis estáticas para guardar o estado do radar (só existem dentro desta função)
  static unsigned long ultimoMovimentoRadar = 0;
  static int anguloRadar = 15;
  static int passoRadar = 1;   // Move 1 grau de cada vez
  
  // Define o intervalo de tempo (em milissegundos) entre cada passo
  const long intervaloPasso = 30; 
  
  // Atualiza o display LCD para informar o modo
  lcd.setCursor(0, 0);
  lcd.print("Modo 4: Radar   ");
  lcd.setCursor(0, 1);
  lcd.print("Enviando dados..");

  // Verifica se já passou o tempo de 30ms desde o último movimento
  unsigned long tempoAtual = millis();
  if (tempoAtual - ultimoMovimentoRadar > intervaloPasso) {
    
    // Reseta o "cronômetro"
    ultimoMovimentoRadar = tempoAtual;

    // 1. Move o servo para o ângulo atual
    radarMotor.write(anguloRadar);
    
    // 2. Calcula a distância
    int distance = radarCalcularDistancia(); // Chama a função que copiamos

    // 3. Envia os dados pela Serial (para o Processing)
    // Exatamente no mesmo formato que o Main.c fazia
    Serial.print(anguloRadar);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");

    // 4. Atualiza o ângulo para o próximo ciclo
    anguloRadar = anguloRadar + passoRadar;

    // 5. Verifica se chegou nos limites (165 ou 15 graus) e inverte a direção
    if (anguloRadar >= 165) {
      anguloRadar = 165;
      passoRadar = -1; // Inverte para começar a descer
    } else if (anguloRadar <= 15) {
      anguloRadar = 15;
      passoRadar = 1;  // Inverte para começar a subir
    }
  }
}

void medirVelocidade(){
  lcd.setCursor(0, 0);
  lcd.print("Modo 3: ");
  lcd.setCursor(0, 1);
  lcd.print("Medir Velocidade      ");

  static float duracao;
  static float duracao2;
  static float distanciaObjeto;
  static float distanciaObjeto2;
  static float velocidadeObj;

  //Ativar sensor ultrassônico
  delayMicroseconds(2);
  digitalWrite(ultraTrigPin,HIGH);//Envia pulso
  delayMicroseconds(10);
  digitalWrite(ultraTrigPin,LOW);
    
    
  duracao = pulseIn(ultraEchoPin,HIGH);//mede tempo que o pulso estava ligado (em microsegundos)

    
  distanciaObjeto = (VelocidadeSom * duracao/20000.0);

  delayMicroseconds(1000);

  //Ativar sensor ultrassônico
  delayMicroseconds(2);
  digitalWrite(ultraTrigPin,HIGH);//Envia pulso
  delayMicroseconds(10);
  digitalWrite(ultraTrigPin,LOW);
    
    
  duracao2 = pulseIn(ultraEchoPin,HIGH);//mede tempo que o pulso estava ligado (em microsegundos)

    
  distanciaObjeto2 = (VelocidadeSom * duracao2/20000.0);

  velocidadeObj = (distanciaObjeto-distanciaObjeto2)/((duracao+duracao2+1000)/1000);

  lcd.setCursor(0, 0);
  lcd.print(velocidadeObj + "m/s");

}

