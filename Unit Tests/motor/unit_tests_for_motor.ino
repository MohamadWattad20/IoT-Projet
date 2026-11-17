#include <AccelStepper.h>

// --- Hardware Pin Definitions ---
const int DIR_PIN = 19;    
const int STEP_PIN = 18;   
const int ENABLE_PIN = 23; 

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Feeder parameters
const unsigned long INTERVAL = 10000; // 10 seconds
const long STEPS_PER_DISPENSE = 600; 

unsigned long previousMillis = 0;

// ==========================================
// CORE LOGIC FUNCTION (Extracted for Testing)
// ==========================================
// This function encapsulates the timing check so we can test it with simulated time.
bool evaluateDispenseTrigger(unsigned long currentMillis, unsigned long &prevMillis, unsigned long interval) {
    if (currentMillis - prevMillis >= interval) {
        prevMillis = currentMillis; // Update state
        return true;
    }
    return false;
}

// ==========================================
// UNIT TEST SUITE (X Test Functions)
// ==========================================

// Test 1: Verify it does NOT trigger before the interval elapses
bool test_doesNotDispenseBeforeInterval() {
    unsigned long fakePrevMillis = 1000;
    unsigned long fakeCurrentMillis = 5000; // Only 4 seconds elapsed
    
    bool triggered = evaluateDispenseTrigger(fakeCurrentMillis, fakePrevMillis, INTERVAL);
    
    // Assertion: should be false, and fakePrevMillis should remain unchanged
    bool passed = (!triggered && fakePrevMillis == 1000);
    Serial.print("Test 1 (Before Interval): ");
    Serial.println(passed ? "PASSED" : "FAILED");
    return passed;
}

// Test 2: Verify it triggers exactly when the 10-second interval is met
bool test_dispensesAtExactInterval() {
    unsigned long fakePrevMillis = 1000;
    unsigned long fakeCurrentMillis = 11000; // Exactly 10,000ms elapsed
    
    bool triggered = evaluateDispenseTrigger(fakeCurrentMillis, fakePrevMillis, INTERVAL);
    
    // Assertion: should be true, and fakePrevMillis should update to 11000
    bool passed = (triggered && fakePrevMillis == 11000);
    Serial.print("Test 2 (Exact Interval): ");
    Serial.println(passed ? "PASSED" : "FAILED");
    return passed;
}

// Test 3: Edge Case - Rapid consecutive calls within the interval window
bool test_ignoresRapidSubsequentCalls() {
    unsigned long fakePrevMillis = 0;
    
    // Call at t = 3000 (No trigger)
    bool t1 = evaluateDispenseTrigger(3000, fakePrevMillis, INTERVAL);
    // Call at t = 9999 (No trigger)
    bool t2 = evaluateDispenseTrigger(9999, fakePrevMillis, INTERVAL);
    // Call at t = 10000 (Trigger!)
    bool t3 = evaluateDispenseTrigger(10000, fakePrevMillis, INTERVAL);
    // Immediate call at t = 10200 (Should NOT trigger again)
    bool t4 = evaluateDispenseTrigger(10200, fakePrevMillis, INTERVAL);

    bool passed = (!t1 && !t2 && t3 && !t4 && fakePrevMillis == 10000);
    Serial.print("Test 3 (Rapid Subsequent Calls Edge Case): ");
    Serial.println(passed ? "PASSED" : "FAILED");
    return passed;
}

// Test 4: Edge Case - millis() Rollover / Overflow simulation
bool test_millisRolloverEdgeCase() {
    // Simulate previousMillis near the max value of an unsigned long (4,294,967,295)
    unsigned long fakePrevMillis = 4294957295UL; 
    
    // Simulate clock rolling over to 5000ms. 
    // Unsigned math: (5000 - 4294957295) correctly calculates 15,000ms elapsed (> 10,000ms interval)
    unsigned long fakeCurrentMillis = 5000UL; 

    bool triggered = evaluateDispenseTrigger(fakeCurrentMillis, fakePrevMillis, INTERVAL);

    bool passed = (triggered && fakePrevMillis == 5000UL);
    Serial.print("Test 4 (Millis Rollover Edge Case): ");
    Serial.println(passed ? "PASSED" : "FAILED");
    return passed;
}

// ==========================================
// ARDUINO SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Allow serial monitor to connect

  Serial.println("\n--- STARTING ESP32 PET FEEDER UNIT TESTS ---");
  
  int passedCount = 0;
  int totalTests = 4;

  // Run all test functions
  if (test_doesNotDispenseBeforeInterval()) passedCount++;
  if (test_dispensesAtExactInterval()) passedCount++;
  if (test_ignoresRapidSubsequentCalls()) passedCount++;
  if (test_millisRolloverEdgeCase()) passedCount++;

  Serial.println("---------------------------------------------");
  Serial.print("Test Results: ");
  Serial.print(passedCount);
  Serial.print("/");
  Serial.print(totalTests);
  Serial.println(" tests passed successfully.\n");

  if (passedCount == totalTests) {
    Serial.println("All unit tests passed! Initializing hardware...");
    
    // Initialize hardware configuration for actual pet feeder operation
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, LOW); // Enable driver
    stepper.setMaxSpeed(1500.0);    
    stepper.setAcceleration(800.0);
    Serial.println("Pet feeder is now running normally in loop()...");
  } else {
    Serial.println("ERROR: Some unit tests failed! Halting execution.");
    while (true); // Halt if tests fail
  }
}

void loop() {
  // Production runtime loop using real hardware millis()
  if (evaluateDispenseTrigger(millis(), previousMillis, INTERVAL)) {
    Serial.println("Dispensing food portion (Live execution)...");
    stepper.move(STEPS_PER_DISPENSE);
  }

  stepper.run();
}