// Blit Sheep

// Fire2012 from https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino

#include <Arduino.h>
#include <FastLED.h>

#define BUILD_WOKWI false

#ifdef BUILD_WOKWI
#define USE_MESH false
#define BUTTON_PIN  7
#define LED_PIN     9
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define MAX_BRIGHTNESS  200
#else
#define USE_MESH true
#define BUTTON_PIN  14
#define LED_PIN     12
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define MAX_BRIGHTNESS  150
#endif

#if USE_MESH
#include <painlessMesh.h>
#define MESH_SSID "Blit Sheep"
#define MESH_PASS "XXXX"
painlessMesh mesh;
#else
#include <TaskScheduler.h>
#endif

#define NUM_LEDS    60

#define FRAMES_PER_SECOND 60

CRGB leds[NUM_LEDS];
unsigned long button_last_press = 0;
#define DEBOUNCE_DELAY 750

Scheduler scheduler;
bool isController = false;
void next_frame();
Task task_next_frame( 15 * TASK_MILLISECOND , TASK_FOREVER, &next_frame );

void Fire2012();
// How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100 
#define FIRE2012_COOLING  55

// What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define FIRE2012_SPARKING 100

// Creates a simple pulsing effect of a single color
void RedGlow();
int red_glow_cur_level = 0;
int red_glow_direction = 1;
int red_glow_speed = 2;
int red_glow_min_glow = 50;

// Randomly adds a new segment of light that quickly fades
void Sparkle();

// Rate at which new sparkles are generated. Range 1 to 1000
#define SPARKLE_NEW_RATE 450

// How quickly the sparkles fade. Suggested range 3-30
#define SPARKLE_FADE_RATE 15

// Each new sparkle will have a random LED width. This setting
// controls the narrowest the sparkle will be.
#define SPARKLE_MIN_WIDTH 1

// Each new sparkle will have a random LED width. This setting
// controls the widest the sparkle will be.
#define SPARKLE_MAX_WIDTH 4

struct animation {
  char name[16];
  void (*function)();
};

animation animations[] = {
  {"Fire", &Fire2012},
  {"Red Glow", &RedGlow},
  {"Sparkle", &Sparkle},
};

int current_animation=0;

#if USE_MESH
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void setIsController(bool val) {
  digitalWrite(LED_BUILTIN, !val); // low = on
  isController = val;
}

void changedConnectionCallback() {
  SimpleList<uint32_t> nodes;
  uint32_t myNodeID = mesh.getNodeId();
  uint32_t lowestNodeID = myNodeID;

  Serial.printf("Changed connections %s\n", mesh.subConnectionJson().c_str());

  nodes = mesh.getNodeList();
  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  for (SimpleList<uint32_t>::iterator node = nodes.begin(); node != nodes.end(); ++node) {
    Serial.printf(" %u", *node);
    if (*node < lowestNodeID) lowestNodeID = *node;
  }
  Serial.println();

  if (lowestNodeID == myNodeID) {
    Serial.printf("Node %u: I am the controller now\n", myNodeID);
    setIsController(true);
  } else {
    Serial.printf("Node %u is the controller now\n", lowestNodeID);
    setIsController(false);
  }
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println(F("Starting up..."));
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(MAX_BRIGHTNESS);

  #if USE_MESH
  mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE );
  mesh.init(MESH_SSID, MESH_PASS, &scheduler, 5555);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.initOTAReceive("Musician");

  pinMode(LED_BUILTIN, OUTPUT);
  // make this one the controller if there are no others on the mesh
  if (mesh.getNodeList().size() == 0) {
    Serial.printf("Empty network, assuming control.\n");
    setIsController(true);
  }
  #endif

  scheduler.addTask(task_next_frame);
  task_next_frame.enable();
}

void loop()
{
  #if USE_MESH
  mesh.update();
  #else
  scheduler.execute();
  #endif
}

void next_frame()
{
  animations[current_animation].function(); // generate animation frame
  FastLED.show(); // display this frame
  if (digitalRead(BUTTON_PIN) == 0 && (millis() - button_last_press) >= DEBOUNCE_DELAY ) {
    button_last_press = millis();
    current_animation++;
    size_t num_animations = sizeof(animations)/sizeof(animations[0]);
    if (current_animation >= num_animations) { current_animation = 0; }
    Serial.printf("Button press detected. Changing to %s\n", animations[current_animation].name);
  }
}


void RedGlow()
{
  if (red_glow_direction == 1) { // increasing
    red_glow_cur_level = red_glow_cur_level + red_glow_speed;
    if (red_glow_cur_level >= MAX_BRIGHTNESS) {
      red_glow_direction = 0;
    }
  } else { // decreasing
    red_glow_cur_level = red_glow_cur_level - red_glow_speed;
    if (red_glow_cur_level <= red_glow_min_glow) {
      red_glow_direction = 1;
    }
  }

  CRGB red  = CHSV( HUE_RED, 255, red_glow_cur_level);
  for( int j = 0; j < NUM_LEDS; j++) {
    leds[j] = red;
  }
}

// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: FIRE2012_COOLING (used in step 1 above), and FIRE2012_SPARKING (used
// in step 3 above).

void Fire2012()
{
// Array of temperature readings at each simulation cell
  static uint8_t heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for(int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((FIRE2012_COOLING * 10) / NUM_LEDS) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for(int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if(random8() < FIRE2012_SPARKING) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for(int j = 0; j < NUM_LEDS; j++) {
      CRGB color = HeatColor( heat[j]);
      leds[j] = color;
    }
}

void Sparkle()
{
  // Decay all previous sparkles
  fadeLightBy(leds, NUM_LEDS, SPARKLE_FADE_RATE);

  // Roll dice to see if we should add a new sparkle
  int new_sparkle = -1;
  if (random(1, 1000) <= SPARKLE_NEW_RATE) {
    new_sparkle = random(0, NUM_LEDS);
  }
  
  if (new_sparkle > 0) {
    // Add new sparkle at determined location
    leds[new_sparkle] = CRGB::White;

    // Determine new sparkle width
    uint8_t new_sparkle_width = random(SPARKLE_MIN_WIDTH, SPARKLE_MAX_WIDTH);

    // Spread out sparkle lighting, fading toward the edges
    for (uint8_t i=1; i < new_sparkle_width; i++) {
      CRGB val = blend(CRGB::White, CRGB::Black, 255/new_sparkle_width*i);
      if (new_sparkle + i < NUM_LEDS) { leds[new_sparkle + i] = val; }
      if (new_sparkle - i >= 0) { leds[new_sparkle - i] = val; }
    }
  }
}