#define DEFAULT_PATTERN_INDEX 0 // Pattern to use when first powered on.

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
pin_value_t easeSin(const millis_t, const millis_t, const pin_value_t, const pin_value_t);

struct Easing {
    millis_t delay, duration;
    easing_function_t function;
};

struct Pattern {
    Easing in, out;
    pin_value_t minPinValue, maxPinValue;
    size_t statesCount;
    uint8_t states[10];
};

const Pattern patterns[] = {
    { { 0,   0, easeLinear }, {   0,   0, easeLinear }, MIN_PIN_VALUE, MAX_PIN_VALUE, 0, {} },        // All off.
    { { 0, 100, easeLinear }, { 100, 100, easeLinear }, MIN_PIN_VALUE, MAX_PIN_VALUE, 1, { 0xFF } },  // All on.
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

    return index;
}

void updateStatus(const pin_index_t pin, const size_t patternIndex, const int onDuration,
                  const int offDuration, const int blankDuration)
{
    static millis_t resetTimestamp = millis();

    static size_t previousPatternIndex = DEFAULT_PATTERN_INDEX;
    if (patternIndex != previousPatternIndex) {
        resetTimestamp = millis();
    }

    const millis_t cycleDuration = ((patternIndex+1) * (onDuration + offDuration)) - offDuration + blankDuration;
    static pin_value_t pinValue = LOW;
    const pin_value_t newValue = ((resetTimestamp % cycleDuration) < onDuration) ? HIGH : LOW;
    if (pinValue != newValue) {
        digitalWrite(pin, pinValue = newValue);
    }
}

void updateLights(const Pattern &pattern)
{
    // Calculate the periodicity of the pattern.
    const millis_t perStateDuration = pattern.out.delay + max(pattern.out.duration, pattern.in.delay + pattern.in.duration);
    const millis_t totalDuration = pattern.statesCount * perStateDuration;

    // Calculate which states we're in / transitioning between.
    const millis_t now = millis();
    const size_t startState = (now % totalDuration) / perStateDuration;
    const size_t endState = (startState +
        ((now % perStateDuration) > pattern.in.delay + pattern.out.delay) ? 1 : 0) % pattern.statesCount;

    // Update each of the affected output pins.
    const size_t outputPinCount = sizeof(outputPins)/sizeof(outputPins[0]);
    for (size_t index = 0; index < outputPinCount; ++index) {
        const millis_t easeInElapsed = (now % perStateDuration) - pattern.in.delay;
        const millis_t easeOutElapsed = (now % perStateDuration) - pattern.in.delay - pattern.out.delay;

        const bool isInStartState = bitRead(pattern.states[startState], index);
        const bool isInEndState = (endState == startState) ? false : bitRead(pattern.states[endState], index);

        const pin_value_t easeInValue = (isInStartState && (easeInElapsed > 0) && (easeInElapsed < pattern.in.duration))
            ? pattern.in.function(easeInElapsed, pattern.in.duration, pattern.minPinValue, pattern.maxPinValue)
            : pattern.minPinValue;

        const pin_value_t easeOutValue = (isInEndState && (easeOutElapsed > 0) && (easeOutElapsed < pattern.out.duration))
            ? pattern.out.function(easeOutElapsed, pattern.out.duration, pattern.minPinValue, pattern.maxPinValue)
            : pattern.minPinValue;

        analogWrite(outputPins[index], max(easeInValue, easeOutValue));
    }
}

void loop()
{
    const size_t patternIndex = getPatternIndex(buttonPin);
    updateStatus(statusPin, patternIndex, 100, 100, 100);
    updateLights(patterns[patternIndex]);
}

pin_value_t easeLinear(const millis_t elapsed, const millis_t duration, const pin_value_t startValue, const pin_value_t endValue)
{
    return startValue + (endValue - startValue) * elapsed / duration;
}

/*pin_value_t easeLog(const millis_t elapsed, const millis_t duration, const pin_value_t startValue, const pin_value_t endValue)
{
    return 0; // pow or sqrt?
    //return startValue + (endValue - startValue) * elapsed / duration;
}*/

pin_value_t easeSin(const millis_t elapsed, const millis_t duration, const pin_value_t startValue, const pin_value_t endValue)
{
    return startValue + sin(M_PI_2 * elapsed / duration) * (endValue - startValue) + 0.5;
}
