/*
 * KUKA Youbot microblx driver.
 *
 * (C) 2011-2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
 *     2010 Ruben Smits <ruben.smits@mech.kuleuven.be>
 *     2010 Steven Bellens <steven.bellens@mech.kuleuven.be>
 *
 *            Department of Mechanical Engineering,
 *           Katholieke Universiteit Leuven, Belgium.
 *
 * see UBX_MODULE_LICENSE_SPDX tag in source file for licenses.
 */

#include <math.h>

#define FIRMWARE_V2

/* firmware bug workaround, limit max current in software
 * don't deactivate unless you know what you are doing. */
#define LIMIT_CURRENT_SOFT

/* Option to get legacy behavior, if defined, pos, vel and effort will
 * be output in a struct motionctrl_jnt_state, otherwise via
 * individual ports of type double[5]. */
#undef USE_ARM_JNT_STATE_STRUCT


/* youbot driver information */

#define YOUBOT_SYSCONF_BASE_ONLY		1
#define YOUBOT_SYSCONF_BASE_ONE_ARM		2
#define YOUBOT_SYSCONF_BASE_TWO_ARM		3

#define YOUBOT_NR_OF_WHEELS			4
#define YOUBOT_NR_OF_JOINTS			5
#define YOUBOT_NR_OF_BASE_SLAVES		(YOUBOT_NR_OF_WHEELS + 1) //wheels + 1 power boards
#define YOUBOT_NR_OF_ARM_SLAVES			(YOUBOT_NR_OF_JOINTS + 1) //arm joints, 1 power boards

#define YOUBOT_MOTOR_DEFAULT_MAX_CURRENT	8000 /* [mA] */
#define YOUBOT_MOTOR_DEFAULT_NOMINAL_SPEED	100  /* [r/s] */

#define YOUBOT_WHEEL_PWRB_NAME			"KR-845"
#define YOUBOT_WHEEL_CTRL_NAME			"TMCM-174"
#define YOUBOT_WHEEL_CTRL_NAME2			"TMCM-1632" /* newer version */

#define YOUBOT_ARM_PWRB_NAME			"KR-843"
#define YOUBOT_ARM_CTRL_NAME			"TMCM-KR-841"
#define YOUBOT_ARM_CTRL_NAME2			"TMCM-1610" /* newer version */

#define YOUBOT_WHEELRADIUS			0.052
#define YOUBOT_MOTORTRANSMISSION		26
#define YOUBOT_JOINT_INIT_VALUE			0
#define YOUBOT_FRONT_TO_REAR_WHEEL		0.47
#define YOUBOT_LEFT_TO_RIGHT_WHEEL		0.3

#define YOUBOT_TICKS_PER_REVOLUTION		4096
#define YOUBOT_WHEEL_CIRCUMFERENCE		(YOUBOT_WHEELRADIUS*2*M_PI)

/* cartesian [ m/s ] to motor [ rpm ] velocity */
#define YOUBOT_CARTESIAN_VELOCITY_TO_RPM	(YOUBOT_MOTORTRANSMISSION*60)/(YOUBOT_WHEEL_CIRCUMFERENCE)

/* angular [ rad/s ] to wheel [ m/s] velocity */
#define YOUBOT_ANGULAR_TO_WHEEL_VELOCITY	(YOUBOT_FRONT_TO_REAR_WHEEL+YOUBOT_LEFT_TO_RIGHT_WHEEL)/2
#define PWM_VALUE_MAX				1799

#define GRIPPER_ENC_WIDTH			67000 /* encoder width of gripper */

/* timeout in seconds after which base is stopped when no new twist is received. */
#define BASE_TIMEOUT				2

/* types that are sent via ports are defined here */
#include "types/youbot_control_modes.h"
#include "types/motionctrl_jnt_state.h"
#include "../std_types/kdl/types/kdl.h"

static const uint8_t ROR			= 1;
static const uint8_t ROL			= 2;
static const uint8_t MST			= 3;
static const uint8_t MVP			= 4;
static const uint8_t SAP			= 5;
static const uint8_t GAP			= 6;
static const uint8_t STAP			= 7;
static const uint8_t RSAP			= 8;
static const uint8_t SGP			= 9;
static const uint8_t GGP			= 10;
static const uint8_t STGP			= 11;
static const uint8_t RSGP			= 12;
static const uint8_t FIRMWARE_VERSION		= 136;

/* Mailbox errors */
#define YB_MBX_STAT_OK				100
#define YB_MBX_STAT_INVALID_CMD			2
#define YB_MBX_STAT_WRONG_TYPE			3
#define YB_MBX_STAT_EINVAL			4
#define YB_MBX_STAT_EEPROM_LOCKED		5
#define YB_MBX_STAT_CMD_UNAVAILABLE		6
#define YB_MBX_STAT_PARAM_PWDPROT		8


static const uint8_t TARGET_POS			= 0;
static const uint8_t ACTUAL_POS			= 1;
static const uint8_t TARGET_SPEED		= 2;
static const uint8_t ACTUAL_SPEED		= 3;
static const uint8_t MAX_RAMP_VEL		= 4;
static const uint8_t PWM_LIMIT			= 5;
static const uint8_t MAX_CURRENT		= 6;
static const uint8_t MAX_VEL_SETPOS		= 7;
static const uint8_t VEL_PID_THRES		= 8;
static const uint8_t CLR_TARGET_DIST		= 9;
static const uint8_t MAX_DIST_SETPOS		= 10;
static const uint8_t ACCEL			= 11;
static const uint8_t POS_PID_THRES		= 12;
static const uint8_t RAMP_GEN_SPEED		= 13;
static const uint8_t INIT_BLDC			= 15;

static const uint8_t CLEAR_I2T			= 29;
/* .. */
static const uint8_t CLR_EC_TIMEOUT		= 158;
static const uint8_t COMMUTATION_MODE		= 159;
/* .. more to be added on demand... */

/* Output buffer protocol */
typedef struct __attribute__((__packed__)) {
	int32_t value;			/* Reference (Position, Velocity, PWM or Current) */
	uint8_t controller_mode;	/* Controller Mode */
	/* 0: Motor stop
	   1: Positioning mode
	   2: Velocity mode
	   3: no more action
	   4: set position to reference
	   5: PWM mode
	   6: Current mode
	   7: Initialize */
} out_motor_t;

/* Input buffer protocol */
typedef struct __attribute__((__packed__)) {
	int32_t position;	/* Actual position, 32-bit Up-Down Counter, 4096 ticks/revolution */
	int32_t current;	/* Actual current in mA */
	int32_t velocity;	/* Actual velocity rpm motor axis */
	uint32_t error_flags;	/* Error flags */
	/* bit 0: OverCurrent
	   bit 1: UnderVoltage
	   bit 2: OverVoltage
	   bit 3: OverTemperature
	   bit 4: Motor Halted
	   bit 5: Hall-Sensor error
	   bit 6: Encoder error
	   bit 7: Sine commutation initilization error
	   bit 8: PWM mode active
	   bit 9: Velocity mode active
	   bit 10: Position mode active
	   bit 11: Current mode active
	   bit 12: Emergency stop
	   bit 13: Freerunning
	   bit 14: Position end
	   bit 15: Module initialized
	   bit 16: EtherCAT timeout (reset with SAP 158)
	   bit 17: I2t exceeded (reset with SAP 29) */
#ifdef FIRMWARE_V2
	int32_t pwm;
#else
	uint16_t temperature; /* Driver Temperature (on-board NTC), 0..4095 */
	/* 25C: 1.34V..1.43V..1.52V (Has to be configured)
	   1C: 4mV..4.3mV..4.6mV
	   Temp: 25.0 + ( ADC * 3.3V / 4096 - 1.43 ) / 0.0043 */
#endif
} in_motor_t;


#define OVERCURRENT        1<<0
#define UNDERVOLTAGE       1<<1
#define OVERVOLTAGE        1<<2
#define OVERTEMP           1<<3
#define MOTOR_HALTED       1<<4
#define HALL_ERR           1<<5
#define ENCODER_ERR        1<<6
#define SINE_COMM_INIT_ERR 1<<7
#define PWM_MODE_ACT       1<<8
#define VEL_MODE_ACT       1<<9
#define POS_MODE_ACT       1<<10
#define CUR_MODE_ACT       1<<11
#define EMERGENCY_STOP     1<<12
#define FREERUNNING        1<<13
#define POSITION_END       1<<14
#define MODULE_INIT        1<<15
#define EC_TIMEOUT         1<<16
#define I2T_EXCEEDED       1<<17


static const int YOUBOT_ARM_JOINT_GEAR_RATIOS[YOUBOT_NR_OF_JOINTS] = {156, 156, 100, 71, 71};
static const double YOUBOT_ARM_JOINT_TORQUE_CONSTANTS[YOUBOT_NR_OF_JOINTS] = {0.0335, 0.0335, 0.0335, 0.051, 0.049}; /* Nm/A */

static const double arm_calib_ref_pos[YOUBOT_NR_OF_JOINTS] = {2.8793, 1.1414, 2.50552, 1.76662, 2.8767};
static const int32_t arm_calib_cur_high[YOUBOT_NR_OF_JOINTS] = {1000, 1500, 1000, 500, 400};

static const double_t arm_calib_move_in_vel[YOUBOT_NR_OF_JOINTS] = {0.1, 0.1, 0.1, 0.1, 0.1 };
static const double arm_calib_move_out_vel[YOUBOT_NR_OF_JOINTS] = {-0.02, -0.02, -0.02, -0.02, -0.02 };

#define ARM_TICKS_TO_POS	(2 * M_PI / (YOUBOT_ARM_JOINT_GEAR_RATIOS[i] * YOUBOT_TICKS_PER_REVOLUTION))
#define ARM_RPM_TO_VEL		(2 * M_PI / (60 * YOUBOT_ARM_JOINT_GEAR_RATIOS[i]))
#define ARM_CUR_TO_EFF		(YOUBOT_ARM_JOINT_TORQUE_CONSTANTS[i] * YOUBOT_ARM_JOINT_GEAR_RATIOS[i] / 1000)

/* software safety for arm */
#define YOUBOT_ARM_SOFT_LIMIT		0.8 /* legal yb soft limit range (%) */

/* the following tow values (in percent) define the scaling range in
 * which forces are scale from 100%->0 */
#define YOUBOT_ARM_SCALE_START		0.8  /* should be same as YOUBOT_ARM_SOFT_LIMIT */
#define YOUBOT_ARM_SCALE_END		0.85 /* somewhere between START and 1 */

/* Joint limits in (deg) (from youBot manual v.82 pg. 7)
 * static const double YOUBOT_ARM_LOWER_LIMIT[YOUBOT_NR_OF_JOINTS] = { -169, -90, -146, -102.5, -167.5 };
 * static const double YOUBOT_ARM_UPPER_LIMIT[YOUBOT_NR_OF_JOINTS] = { 169, 65, 151, 102.5, 167.5 };
 *
 * The following values are determined manually by moving the arm into
 * the limits:
 */

static const double youbot_arm_lower_limits[YOUBOT_NR_OF_JOINTS] = { -165.2, -86.2, -144.6, -99.8, -163.4 };
static const double youbot_arm_upper_limits[YOUBOT_NR_OF_JOINTS] = { 164.8, 65.1, 143.1, 100.1, 165.0 };


/*
 * driver local state
 */

/* statistics */
struct youbot_slave_stats {
	uint64_t overcurrent;
	uint64_t undervoltage;
	uint64_t overvoltage;
	uint64_t overtemp;
	uint64_t hall_err;
	uint64_t encoder_err;
	uint64_t sine_comm_init_err;
	uint64_t emergency_stop;
	uint64_t ec_timeout;
	uint64_t i2t_exceeded;
};

/* Holds the information about one motor (for both wheels and arm
 * axis).
 */
struct youbot_motor_info {
	uint32_t slave_idx;	/* the index into ec_slave */

	int32_t msr_pos;
	int32_t msr_vel;
	int32_t msr_cur;
	uint32_t error_flags;

#ifdef FIRMWARE_V2
	int32_t msr_pwm;
#else
	int16_t temperature;
#endif
	int32_t i2t_ex;		/* i2t currently exceeded? */
	int32_t cmd_val;	/* commanded value. unit depends on control mode */
	uint32_t max_cur;	/* cut-off currents larger than this */

	struct youbot_slave_stats stats;
};

/* Arm */
struct youbot_arm_info {
	uint32_t detected;		/* has been detected and is in use */
	uint32_t mod_init_stat;		/* bitfield, bit is set for each initialized slave */
	int8_t control_mode;		/* currently used control mode */
	struct youbot_motor_info jnt_inf[YOUBOT_NR_OF_JOINTS];

#ifdef USE_ARM_JNT_STATE_STRUCT
	struct motionctrl_jnt_state jnt_states;
	ubx_port_t *p_arm_state;
#else
	double pos[YOUBOT_NR_OF_JOINTS];
	double vel[YOUBOT_NR_OF_JOINTS];
	double eff[YOUBOT_NR_OF_JOINTS];

	ubx_port_t *p_msr_pos;
	ubx_port_t *p_msr_vel;
	ubx_port_t *p_msr_eff;
#endif
	/* calibration stuff */
	int calibrating;
	int axis_at_limit[YOUBOT_NR_OF_JOINTS];

	int jntlim_safety_disabled;

	ubx_port_t *p_events;
	ubx_port_t *p_control_mode;
	ubx_port_t *p_calibrate_cmd;
	ubx_port_t *p_cmd_pos;
	ubx_port_t *p_cmd_vel;
	ubx_port_t *p_cmd_cur;
	ubx_port_t *p_cmd_eff;
	ubx_port_t *p_arm_motorinfo;
	ubx_port_t *p_gripper;
};


/* Base */
struct youbot_base_info {
	uint32_t detected;		/* did we detect a base? */
	uint32_t mod_init_stat;		/* bitfield, bit is set for each initialized slave */
	int8_t control_mode;		/* currently used control mode */

	struct youbot_motor_info wheel_inf[YOUBOT_NR_OF_WHEELS];

	/* commanded information */
	struct timespec last_cmd;	/* timestamp when last command was received */
	struct kdl_twist cmd_twist;

	/* sensor information */
	struct kdl_frame msr_odom;
	int odom_started;
	int last_pos[YOUBOT_NR_OF_WHEELS];
	float odom_yaw;

	struct kdl_twist msr_twist;


	/* port cache */
	ubx_port_t *p_control_mode;
	ubx_port_t *p_cmd_twist;
	ubx_port_t *p_cmd_vel;
	ubx_port_t *p_cmd_cur;
	ubx_port_t *p_msr_odom;
	ubx_port_t *p_msr_twist;
	ubx_port_t *p_base_motorinfo;
};

struct youbot_info {
	uint8_t io_map[4096];

	struct youbot_base_info base;
	struct youbot_arm_info arm1;
	struct youbot_arm_info arm2;

	/* global stats */
	uint64_t pd_send_err;
	uint64_t pd_recv_err;
};
