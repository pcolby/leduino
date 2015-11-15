#define BLUE_A      bit(1)
#define BLUE_B      bit(2)
#define BLUE_ORANGE bit(3)
#define GREEN_RED   bit(4)

#define DEFAULT_PATTERN_INDEX 3 // Pattern to use when first powered on.
#define MIN_PIN_VALUE 0
#define MAX_PIN_VALUE 0xFF

// Map some Arduino types, mostly just to improve semantics.
typedef uint8_t pin_index_t;    ///< Type used to identify pins.
typedef uint8_t pin_value_t;    ///< Type used to specify pin values.
typedef unsigned long millis_t; ///< Type used for millisecond timestamps.

// Map the input and output pin numbers.
const pin_index_t buttonPin = 2; // Just the first usable non-PWM pin.
const pin_index_t outputPins[] = { 3, 5, 6, 9, 10, 11 }; // Arduino Uno's PWM pins.
const pin_index_t statusPin = 13; // Arduino Uno's built-in LED pin.

// Declare easing functions.
typedef pin_value_t (*easing_function_t)(const millis_t elapsed, const millis_t duration,
                                         const pin_value_t startValue, const pin_value_t endValue);
pin_value_t easeLinear(const millis_t, const millis_t, const pin_value_t, const pin_value_t);
pin_value_t easeSine(const millis_t, const millis_t, const pin_value_t, const pin_value_t);

struct Easing {
    millis_t duration;
    easing_function_t function;
};

struct Pattern {
    millis_t onDuration, transitionDuration;
    Easing in, out;
    pin_value_t minPinValue, maxPinValue;
    size_t statesCount;
    uint8_t states[10];
};

const Pattern patterns[] = {
    { 1000,   0, { 0, NULL }, {  0, NULL }, MIN_PIN_VALUE, MAX_PIN_VALUE, 0, {} },        // All off.
    { 1000,   0, { 0, NULL }, {  0, NULL }, MIN_PIN_VALUE, MAX_PIN_VALUE, 1, { 0xFF } },  // All on.
    {   50, 2000, { 1000, easeSine }, { 1000, easeSine }, 20, MAX_PIN_VALUE, 1, { 0xFF } },  // Gentle pulse all.
    {  500, 1000, {  750, easeLinear }, { 750, easeLinear }, MIN_PIN_VALUE, MAX_PIN_VALUE, 4, { BLUE_A, BLUE_B, BLUE_ORANGE, GREEN_RED } }, // Cross-fade.
};

void setup()
{
    // Initialise input pins.
    pinMode(buttonPin, INPUT);

    // Initialise output pins.
    const size_t outputPinCount = sizeof(outputPins)/sizeof(outputPins[0]);
    for (size_t index = 0; index < outputPinCount; ++index) {
        pinMode(outputPins[index], OUTPUT);
    }
    pinMode(statusPin, OUTPUT);
}

size_t getPatternIndex(const pin_index_t pin, const millis_t debounceDuration = 100)
{
    static size_t index = DEFAULT_PATTERN_INDEX;
    static millis_t lastPressedTimestamp = 0;

    const millis_t now = millis();
    if (((now - lastPressedTimestamp) > debounceDuration) && (digitalRead(pin) == HIGH)) {
        const size_t patternsCount = sizeof(patterns)/sizeof(patterns[0]);
        index = (index+1) % patternsCount;
        lastPressedTimestamp = now;
    }

    return DEFAULT_PATTERN_INDEX;//index;
}

void updateStatus(const pin_index_t pin, const size_t patternIndex, const int onDuration,
                  const int offDuration, const int blankDuration)
{
    static millis_t resetTimestamp = millis();

    static size_t previousPatternIndex = DEFAULT_PATTERN_INDEX;
    if (patternIndex != previousPatternIndex) {
        resetTimestamp = millis();
    }

    const millis_t onOffDuration = onDuration + offDuration;
    const millis_t cycleDuration = (patternIndex * onOffDuration) + blankDuration;
    const millis_t elapsed = (millis() - resetTimestamp) % cycleDuration;
    static pin_value_t pinValue = LOW;
    const pin_value_t newValue = (((elapsed / onOffDuration) < patternIndex) && ((elapsed % onOffDuration) < onDuration)) ? HIGH : LOW;
    if (pinValue != newValue) {
        digitalWrite(pin, pinValue = newValue);
    }
}

void updateLights(const Pattern &pattern)
{
    // Calculate the periodicity of the pattern.
    const millis_t perStateDuration = pattern.onDuration +  pattern.transitionDuration;
    const millis_t totalDuration = pattern.statesCount * perStateDuration;

    // Calculate which states we're in / transitioning between.
    const millis_t now = millis();
    const size_t startState = (now % totalDuration) / perStateDuration;
    const size_t endState = (startState + 1) % pattern.statesCount;

    const millis_t elapsed = now % perStateDuration;

    // Update each of the affected output pins.
    const size_t outputPinCount = sizeof(outputPins)/sizeof(outputPins[0]);
    for (size_t index = 0; index < outputPinCount; ++index) {
        const bool isInStartState = bitRead(pattern.states[startState], index);
        const bool isInEndState = bitRead(pattern.states[endState], index);

        if (elapsed < pattern.onDuration) {
            analogWrite(outputPins[index], isInStartState ? pattern.maxPinValue : pattern.minPinValue);
        } else if (elapsed < pattern.onDuration + pattern.transitionDuration) {
            const pin_value_t easeInValue = ((isInEndState) && (elapsed > pattern.onDuration + pattern.transitionDuration - pattern.in.duration))
                ? pattern.out.function(elapsed - pattern.onDuration - pattern.transitionDuration + pattern.in.duration, pattern.in.duration, pattern.minPinValue, pattern.maxPinValue)
                : pattern.minPinValue;

            const pin_value_t easeOutValue = ((isInStartState) && (elapsed < pattern.onDuration + pattern.out.duration))
                ? pattern.out.function(pattern.out.duration - elapsed + pattern.onDuration, pattern.out.duration, pattern.minPinValue, pattern.maxPinValue)
                : pattern.minPinValue;

            analogWrite(outputPins[index], max(easeInValue, easeOutValue));
        } else {
            analogWrite(outputPins[index], isInEndState ? pattern.maxPinValue : pattern.minPinValue);
        }
    }
}

void loop()
{
    const size_t patternIndex = getPatternIndex(buttonPin);
    updateStatus(statusPin, patternIndex, 150, 300, 750);
    updateLights(patterns[patternIndex]);
}

pin_value_t easeLinear(const millis_t elapsed, const millis_t duration, const pin_value_t startValue, const pin_value_t endValue)
{
    return startValue + (endValue - startValue) * elapsed / duration;
}

pin_value_t easeSine(const millis_t elapsed, const millis_t duration, const pin_value_t startValue, const pin_value_t endValue)
{
    return round(startValue + sin(HALF_PI * elapsed / duration) * (endValue - startValue));
}
