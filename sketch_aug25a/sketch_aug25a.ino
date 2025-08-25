#include "StateMachineLib.h"
#include <AsyncTaskLib.h>
#include <DHT.h>

// ======= Pines / DHT =======
const uint8_t DHT_PIN = 4;      // DHT11 en digital 
const uint8_t LDR_PIN = 34;     // LDR en ADC
const uint8_t LED_ALERTA = 2;   // LED para estado Alerta
const uint8_t LED_ALARMA = 5;   // LED para estado Alarma
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ======= Estados e Inputs =======
enum State {
    Inicio = 0,
    Monitoreo_th = 1,
    Alerta = 2,
    Monitoreo_luz = 3,
    Alarma = 4
};

enum Input {
    A = 1,
    B = 3,
    Sign_Timeout = 4,
    Unknown = 5
};

// ======= Máquina =======
StateMachine stateMachine(5, 9);
Input input = Input::Unknown;

// ======= Variables =======
float currentTemp = NAN;
int currentLuz = -1;

// ======= Timeouts =======
AsyncTask timeoutInicio(5000, false, []() { input = Input::Sign_Timeout; });
AsyncTask timeoutMonThToLuz(7000, false, []() { input = Input::Sign_Timeout; });
AsyncTask timeoutAlertaToMonTh(4000, false, []() { input = Input::Sign_Timeout; });
AsyncTask timeoutMonLuzToTh(2000, false, []() { input = Input::Sign_Timeout; });
AsyncTask timeoutAlarmaToLuz(3000, false, []() { input = Input::Sign_Timeout; });

// ======= Lecturas =======
AsyncTask readDHTTask(2000, false, []() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    currentTemp = t;
    if (isnan(h) || isnan(t)) {
        Serial.println("Estado: Monitoreo_th | Temp: N/A | Hum: N/A");
    } else {
        Serial.print("Estado: Monitoreo_th | Temp: ");
        Serial.print(t);
        Serial.print("C | Hum: ");
        Serial.print(h);
        Serial.println("%");
    }
});

AsyncTask readLDRTask(500, false, []() {
    int val = analogRead(LDR_PIN);
    currentLuz = val;
    Serial.print("Estado: Monitoreo_luz | Luz: ");
    Serial.println(val);
});

// ======= LEDs =======
// LED Alarma: 200 ms ON / 400 ms OFF
AsyncTask blinkAlarmaTask(200, true, []() {
    static bool ledState = false;
    ledState = !ledState;
    digitalWrite(LED_ALARMA, ledState);
    blinkAlarmaTask.SetIntervalMillis(ledState ? 200 : 400);
});

// LED Alerta: 100 ms ON / 300 ms OFF
AsyncTask blinkAlertaTask(100, true, []() {
    static bool ledState = false;
    ledState = !ledState;
    digitalWrite(LED_ALERTA, ledState);
    blinkAlertaTask.SetIntervalMillis(ledState ? 100 : 300);
});

// ======= Timeout helper =======
void setTimeout(AsyncTask& task, unsigned long interval) {
    task.SetIntervalMillis(interval);
    task.Reset();
    task.Start();
}

// ======= Funciones de salida en pantalla =======
void outputInicio()
{
  Serial.println("Inicio   MonitoreoTyH   MonitoreoLuz   Alerta   Alarma");
  Serial.println("  X                                                   ");
  Serial.println();
}

void outputMonitoreoTyH()
{
  Serial.println("Inicio   MonitoreoTyH   MonitoreoLuz   Alerta   Alarma");
  Serial.println("               X                                      ");
  Serial.println();
}

void outputMonitoreoLuz()
{
  Serial.println("Inicio   MonitoreoTyH   MonitoreoLuz   Alerta   Alarma");
  Serial.println("                             X                        ");
  Serial.println();
}

void outputAlerta()
{
  Serial.println("Inicio   MonitoreoTyH   MonitoreoLuz   Alerta   Alarma");
  Serial.println("                                        X            ");
  Serial.println();
}

void outputAlarma()
{
  Serial.println("Inicio   MonitoreoTyH   MonitoreoLuz   Alerta   Alarma");
  Serial.println("                                                  X  ");
  Serial.println();
}

// ======= Setup de la máquina =======
void setupStateMachine()
{
    // Transiciones
    stateMachine.AddTransition(Inicio, Monitoreo_th, []() { return input == Input::Sign_Timeout; });
    stateMachine.AddTransition(Monitoreo_th, Monitoreo_luz, []() { return input == Input::Sign_Timeout; });
    stateMachine.AddTransition(Monitoreo_th, Alerta, []() { return (isnan(currentTemp) ? false : (currentTemp > 27.0)); });
    stateMachine.AddTransition(Alerta, Monitoreo_th, []() { return input == Input::Sign_Timeout; });
    stateMachine.AddTransition(Alerta, Inicio, []() { return input == Input::A; });
    stateMachine.AddTransition(Monitoreo_luz, Monitoreo_th, []() { return input == Input::Sign_Timeout; });
    stateMachine.AddTransition(Monitoreo_luz, Alarma, []() { return (currentLuz >= 0 && currentLuz > 1024); });
    stateMachine.AddTransition(Alarma, Monitoreo_luz, []() { return input == Input::Sign_Timeout; });
    stateMachine.AddTransition(Alarma, Inicio, []() { return input == Input::B; });

    // On Enter
    stateMachine.SetOnEntering(Inicio, []() {
        Serial.println("Estado: Inicio");
        outputInicio();
        setTimeout(timeoutInicio, 5000);
        digitalWrite(LED_ALERTA, LOW);
        digitalWrite(LED_ALARMA, LOW);
    });

    stateMachine.SetOnEntering(Monitoreo_th, []() {
        currentTemp = NAN;
        Serial.println("Estado: Monitoreo_th");
        outputMonitoreoTyH();
        setTimeout(timeoutMonThToLuz, 7000);
        readDHTTask.Start();
        digitalWrite(LED_ALERTA, LOW);
        digitalWrite(LED_ALARMA, LOW);
    });

    stateMachine.SetOnEntering(Alerta, []() {
        Serial.println("Estado: Alerta");
        outputAlerta();
        setTimeout(timeoutAlertaToMonTh, 4000);
        blinkAlertaTask.Start();
    });

    stateMachine.SetOnEntering(Monitoreo_luz, []() {
        currentLuz = -1;
        Serial.println("Estado: Monitoreo_luz");
        outputMonitoreoLuz();
        setTimeout(timeoutMonLuzToTh, 2000);
        readLDRTask.Start();
        digitalWrite(LED_ALERTA, LOW);
        digitalWrite(LED_ALARMA, LOW);
    });

    stateMachine.SetOnEntering(Alarma, []() {
        Serial.println("Estado: Alarma");
        outputAlarma();
        setTimeout(timeoutAlarmaToLuz, 3000);
        blinkAlarmaTask.Start();
    });

    // On Leaving
    stateMachine.SetOnLeaving(Inicio, []() {
        input = Input::Unknown;
        timeoutInicio.Stop();
    });

    stateMachine.SetOnLeaving(Monitoreo_th, []() {
        input = Input::Unknown;
        timeoutMonThToLuz.Stop();
        readDHTTask.Stop();
    });

    stateMachine.SetOnLeaving(Alerta, []() {
        input = Input::Unknown;
        timeoutAlertaToMonTh.Stop();
        blinkAlertaTask.Stop();
        digitalWrite(LED_ALERTA, LOW);
    });

    stateMachine.SetOnLeaving(Monitoreo_luz, []() {
        input = Input::Unknown;
        timeoutMonLuzToTh.Stop();
        readLDRTask.Stop();
    });

    stateMachine.SetOnLeaving(Alarma, []() {
        input = Input::Unknown;
        timeoutAlarmaToLuz.Stop();
        blinkAlarmaTask.Stop();
        digitalWrite(LED_ALARMA, LOW);
    });
}

// ======= Setup / Loop =======
void setup()
{
    Serial.begin(9600);
    dht.begin();
    pinMode(LED_ALERTA, OUTPUT);
    pinMode(LED_ALARMA, OUTPUT);
    digitalWrite(LED_ALERTA, LOW);
    digitalWrite(LED_ALARMA, LOW);
    setupStateMachine();
    stateMachine.SetState(Inicio, false, true);
}

void loop()
{
    // Actualizar timeouts
    timeoutInicio.Update();
    timeoutMonThToLuz.Update();
    timeoutAlertaToMonTh.Update();
    timeoutMonLuzToTh.Update();
    timeoutAlarmaToLuz.Update();

    // Actualizar tareas
    readDHTTask.Update();
    readLDRTask.Update();
    blinkAlarmaTask.Update();
    blinkAlertaTask.Update();

    // Leer entrada
    if (input == Input::Unknown) {
        input = static_cast<Input>(readInput());
    }

    // Actualizar máquina
    stateMachine.Update();
}

// ======= Lectura serial =======
int readInput()
{
    if (Serial.available())
    {
        char c = Serial.read();
        switch (c)
        {
            case 'A': return Input::A;
            case 'B': return Input::B;
            default: return Input::Unknown;
        }
    }
    return Input::Unknown;
}