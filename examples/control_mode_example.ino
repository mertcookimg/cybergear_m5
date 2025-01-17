#include <Arduino.h>
#include <math.h>
#include <mcp_can.h>
#include <M5Stack.h>
#include "cybergear_driver.hh"

#define INC_POSITION  20.0
#define INC_VELOCITY  0.4
#define INC_TORQUE    0.04


/**
 * @brief Init can interface
 */
void init_can();

/**
 * @brief Draw display
 *
 * @param mode current, speed, position, motion mode
 * @param is_mode_change mode change flag
 */
void draw_display(uint8_t mode, bool is_mode_change = false);

/**
 * @brief Get the color and mode
 *
 * @param mode target mode
 * @param color mode color
 * @param mode_str mode string
 */
void get_color_and_mode_str(uint8_t mode, uint16_t & color, String & mode_str);


// init MCP_CAN object
#define CAN0_INT 15  // Set INT to pin 2
MCP_CAN CAN0(12);    // Set CS to pin 10

// setup master can id and motor can id (default cybergear can id is 0x7F)
uint8_t MASTER_CAN_ID = 0x00;
uint8_t MOT_CAN_ID = 0x7F;

// init cybergeardriver
CybergearDriver driver = CybergearDriver(MASTER_CAN_ID, MOT_CAN_ID);
MotorStatus motor_status;

// init sprite for display
TFT_eSprite sprite = TFT_eSprite(&sprite);

uint8_t mode = MODE_POSITION;   //!< current mode
float target_pos = 0.0;         //!< motor target position
float target_vel = 0.0;         //!< motor target velocity
float target_torque = 0.0;      //!< motor target torque
float dir = 1.0f;               //!< direction for motion mode
float default_kp = 50.0f;       //!< default kp for motion mode
float default_kd = 1.0f;        //!< default kd for motion mode

void setup()
{
  M5.begin();

  // init sprite
  sprite.setColorDepth(8);
  sprite.setTextSize(3);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());

  // init cybergear driver
  init_can();
  driver.init(&CAN0);
  driver.init_motor(mode);
  driver.set_limit_speed(30.0f);

  // display current status
  draw_display(mode, true);
}

void draw_display(uint8_t mode, bool is_mode_change)
{
  uint16_t bg_color;
  String mode_str;
  get_color_and_mode_str(mode, bg_color, mode_str);

  if (is_mode_change) M5.Lcd.fillScreen(bg_color);
  sprite.fillScreen(bg_color);
  sprite.setCursor(0, 0);
  sprite.setTextColor(TFT_WHITE, bg_color);

  sprite.setTextSize(4);
  sprite.println(mode_str);

  sprite.setTextSize(2);
  sprite.println("");
  sprite.println("=== Target ===");
  sprite.print("Position:");
  sprite.print(target_pos);
  sprite.println(" rad");
  sprite.print("Velocity:");
  sprite.print(target_vel);
  sprite.println(" rad/s");
  sprite.print("Current : ");
  sprite.print(target_torque);
  sprite.println(" A");
  sprite.println("");

  sprite.println("=== Current ===");
  sprite.print("Position:");
  sprite.print(motor_status.position);
  sprite.println(" rad");
  sprite.print("Velocity:");
  sprite.print(motor_status.velocity);
  sprite.println(" rad/s");
  sprite.print("Effort : ");
  sprite.print(motor_status.effort);
  sprite.println(" Nm");

  sprite.pushSprite(0, 0);
}

void get_color_and_mode_str(uint8_t mode, uint16_t & color, String & mode_str)
{
  switch (mode)
  {
    case MODE_POSITION:
      color = RED;
      mode_str = String("Position");
      break;
    case MODE_SPEED:
      color = GREEN;
      mode_str = String("Speed");
      break;
    case MODE_CURRENT:
      color = BLUE;
      mode_str = String("Current");
      break;
    case MODE_MOTION:
      color = BLACK;
      mode_str = String("Motion");
      break;
  }
}

void loop()
{
  // update m5 satatus
  M5.update();

  // check mode change
  if(M5.BtnB.wasPressed()) {
    mode = (mode + 1) % MODE_CURRENT + 1;
    driver.init_motor(mode);
    target_pos = 0.0;
    target_vel = 0.0;
    target_torque = 0.0;
    draw_display(mode, true);

  } else if (M5.BtnC.wasPressed()) {
    if (mode == MODE_POSITION) {
      target_pos += INC_POSITION / 180.0f * M_PI;

    } else if (mode == MODE_SPEED) {
      target_vel += INC_VELOCITY;

    } else if (mode == MODE_CURRENT) {
      target_torque += INC_TORQUE;
    }
    draw_display(mode);

  } else if (M5.BtnA.wasPressed()) {
    if (mode == MODE_POSITION) {
      target_pos -= INC_POSITION / 180.0f * M_PI;

    } else if (mode == MODE_SPEED) {
      target_vel -= INC_VELOCITY;

    } else if (mode == MODE_CURRENT) {
      target_torque -= INC_TORQUE;
    }
    draw_display(mode);
  }

  if (driver.get_run_mode() == MODE_POSITION) {
    driver.set_position_ref(target_pos);
  }
  else if (driver.get_run_mode() == MODE_SPEED) {
    driver.set_speed_ref(target_vel);
  }
  else if (driver.get_run_mode() == MODE_CURRENT) {
    driver.set_current_ref(target_torque);
  }
  else {
    target_pos += dir * 10.0 / 180.0 * M_PI;
    if (target_pos > P_MAX) { dir = -1.0; target_pos = P_MAX; }
    else if (target_pos < P_MIN) { dir = 1.0; target_pos = P_MIN; }
    driver.motor_control(target_pos, dir * target_vel, dir * target_torque, default_kd, default_kd);
  }

  // update and get motor data
  if ( driver.process_can_packet() ) {
    motor_status = driver.get_motor_status();
    draw_display(mode);
  }

  delay(200);
}

void init_can()
{
  if (CAN0.begin(MCP_ANY, CAN_1000KBPS, MCP_8MHZ) == CAN_OK) {
      sprite.printf("MCP2515 Initialized Successfully!\n");

  } else {
      sprite.printf("Error Initializing MCP2515...");
  }
  CAN0.setMode(MCP_NORMAL);  // Set operation mode to normal so the MCP2515 sends acks to received data.
  pinMode(CAN0_INT, INPUT);  // Configuring pin for /INT input
}
