#include <AccelStepper.h>

// Define pin connections based on our wiring
const int DIR_PIN = 19;    // D pin on A4988
const int STEP_PIN = 18;   // S pin on A4988
const int ENABLE_PIN = 23; // E pin on A4988

// Define the stepper mode: 1 means Driver mode (Step/Dir interface)
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Timing variables for the 10-second interval
unsigned long previousMillis = 0;
const long interval = 5000; // 10 seconds between dispenses

// Increased duration: 600 steps = 3 full rotations (adjust as needed for your portion size)
const int STEPS_PER_DISPENSE = 900; 

void setup() {
  Serial.begin(115200);
  
  // Configure Enable pin
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); // LOW enables the A4988 driver

  // Increased speed: Set higher maximum speed and acceleration (steps per second)
  stepper.setMaxSpeed(1500.0);    // Increased from 800 to 1500 steps/sec
  stepper.setAcceleration(800.0); // Increased acceleration to match higher speed
  
  Serial.println("Pet feeder initialized with higher speed and portion size...");
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if 10 seconds have passed since the last dispense
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Reset the timer
    
    Serial.println("Dispensing larger portion...");
    
    // Add steps to the current target position
    stepper.move(STEPS_PER_DISPENSE); 
  }

  // This function must be called continuously in the loop to generate pulses smoothly
  stepper.run();
}