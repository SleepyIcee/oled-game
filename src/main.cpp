#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;

int buttonPin = 4;

typedef struct {
  float x;
  float y;
  int width;
  int height;
  bool active;
} Rectangle;

enum GameState {
  START,
  PLAY,
  LOST
};

enum GameState gameState; 

const int rectWidth = 8;
const int rectsInRow = 16;

Rectangle player;
const float playerSpeed = 5;

const int objsCapacity = 10;
Rectangle objs[objsCapacity];
float objsSpeed = 30;

const int bulletsCapacity = 10;
Rectangle bullets[bulletsCapacity];
const float bulletSpeed = 20;

unsigned long previousTime = 0;
float timeToSpawnObj;
int maxSpawningTime = 2;
float timeBetweenBullets = 0.2f;
float timeToSpawnBullet = timeBetweenBullets;
const float buttonPressTime = 0.5;
float buttonPressTimer = 0;

int score = 0;
const int obsticlesCanGoThroughNum = 3;
int obsticlesCanGoThrough = obsticlesCanGoThroughNum;

void load();
void update();
void draw();
void updateGame(float deltaTime);
float getDeltaTime();
bool detectCollision(Rectangle rect1, Rectangle rect2);

void setup() {
  Serial.begin(9600);

  if (!mpu.begin()) {
    Serial.println("Sensor init failed");
    // while (1)
    //   yield();
  }
  Serial.println("Found a MPU-6050 sensor");

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000);

  randomSeed(analogRead(21));
  timeToSpawnObj = random(10);

  pinMode(buttonPin, INPUT_PULLUP);

  load();
}

void loop() {
  update();
  draw();
}

void load() {
  gameState = START;
  player = {(float)display.width()/2 - 2, (float)display.height() - 5, rectWidth, rectWidth, true};
  timeToSpawnBullet = 1;

  for (int i = 0; i < objsCapacity; i++) {
    objs[i].active = false;
  }

  for (int i = 0; i < bulletsCapacity; i++) {
    bullets[i].active = false;
  }
}

bool isButtonJustPressed() {
  if (digitalRead(buttonPin) == LOW && buttonPressTimer <= 0) {
    buttonPressTimer = buttonPressTime;
    return true;
  }
  
  return false;
}

void update() {
  float deltaTime = getDeltaTime();

  switch (gameState) {
    case LOST:
      if (isButtonJustPressed()) {
        for (int i = 0; i < objsCapacity; i++) {
          objs[i].active = false;
        }

        for (int i = 0; i < bulletsCapacity; i++) {
          bullets[i].active = false;
        }

        score = 0;
        obsticlesCanGoThrough = obsticlesCanGoThroughNum;
        gameState = START;

        timeToSpawnObj = random(10);
      }

      break;
    case START:
      if (isButtonJustPressed())
        gameState = PLAY;
      break;
    case PLAY:
      updateGame(deltaTime);
      break;
    default:
      break;
  }

  if (buttonPressTimer > 0) {
    buttonPressTimer -= deltaTime;
  }
}

float getDeltaTime() {
  unsigned long currentTime = millis();
  float deltaTime = (currentTime - previousTime) / 1000.0f;
  previousTime = currentTime;

  return deltaTime;
}

void moveObsticles(float deltaTime) {
  for (int i = 0; i < objsCapacity; i++) {
    if (!objs[i].active)
      continue;

    objs[i].y += objsSpeed * deltaTime;

    if (objs[i].y > display.height() + rectWidth) {
      objs[i].active = false;
      obsticlesCanGoThrough--;

      if (obsticlesCanGoThrough < 1)
        gameState = LOST;
    }

    if (detectCollision(player, objs[i])) {
      gameState = LOST;
    }
  }
}

void handleTimers(float deltaTime) {
  if (timeToSpawnObj < 0) {
    for (int i = 0; i < objsCapacity; i++) {
      if (!objs[i].active) {
        objs[i] = {
          (float)random(rectsInRow) * rectWidth,
          -rectWidth,
          rectWidth,
          rectWidth,
          true};
        break;
      }
    }
      
    timeToSpawnObj = random(1, maxSpawningTime); // max spawning time decreases to make the game harder
  } else {
    timeToSpawnObj -= 1 * deltaTime;
  }

  if (timeToSpawnBullet > 0) {
    timeToSpawnBullet -= deltaTime;
  }
}

void updateBullets(float deltaTime) {
  if (timeToSpawnBullet <= 0) {
    if (digitalRead(buttonPin) == LOW) {
      for (int i = 0; i < bulletsCapacity; i++) {
        if (!bullets[i].active) {
          bullets[i] = Rectangle {
            .x = player.x + player.width/4,
            .y = player.y,
            .width = player.width/2,
            .height = player.height/2,
            .active = true
          };

          break;
        }
      }
      
      timeToSpawnBullet = timeBetweenBullets;
    }
  }

  for (int i = 0; i < bulletsCapacity; i++) {
    if (!bullets[i].active)
      continue;

    bullets[i].y -= bulletSpeed * deltaTime;

    if (bullets[i].y < -5)
        bullets[i].active = false;

    for (int j = 0; j < objsCapacity; j++) {
      if (!objs[j].active)
        continue;
      
      if (detectCollision(bullets[i], objs[j])) {
        bullets[i].active = false;
        objs[j].active = false;
        score++;
      }
    }
  }
}

void updateGame(float deltaTime) {
  sensors_event_t acceleration, gyro, temp;
  mpu.getEvent(&acceleration, &gyro, &temp);

  // map gyro input to the player x position
  int targetX = map(acceleration.acceleration.y * 100, -980/3, 980/3, 0, display.width() - player.width);
  int constrainedX = constrain(targetX, 0, display.width() - player.width);
  player.x += (constrainedX - player.x) * playerSpeed * deltaTime;

  moveObsticles(deltaTime);
  updateBullets(deltaTime);
  handleTimers(deltaTime);
}

void drawLosingState() {
  display.setCursor(35, 10);
  display.write("You lost!");
  display.setCursor(35, 20);
  display.print("Score: ");
  display.print(score);
}

void drawStartState() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 10);
  display.write("Press to Play");
}

void drawPlayState() {
  for (int i = 0; i < bulletsCapacity; i++) {
    if (!bullets[i].active)
      continue;

    display.drawRect((int)bullets[i].x, (int)bullets[i].y, bullets[i].width, bullets[i].height, WHITE);
  }

  display.drawRect((int)player.x, (int)player.y, player.width, player.height, WHITE);
  // display.drawTriangle(player.x + player.width - player.x/2, )
  for (int i = 0; i < objsCapacity; i++) {
    if (!objs[i].active)
      continue;

    display.drawRect((int)objs[i].x, (int)objs[i].y, objs[i].width, objs[i].height, WHITE);
  }

  display.setCursor(0, 0);
  display.print(score);
}

void draw() {
  display.clearDisplay();

  switch (gameState) {
    case LOST:
      drawLosingState();
      break;
    case START:
      drawStartState();
      break;
    case PLAY:
      drawPlayState();
      break;
    default:
      break;
  }
  
  display.display();
}

bool detectCollision(Rectangle rect1, Rectangle rect2) {
    return rect1.x < rect2.x + rect2.width &&
           rect1.x + rect1.width > rect2.x &&
           rect1.y < rect2.y + rect2.height &&
           rect1.y + rect1.height > rect2.y;
}