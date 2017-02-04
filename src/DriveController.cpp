/*
 * DriveController.cpp
 *
 *  Created on: Jan 9, 2017
 *      Author: Apes of Wrath
 */

#include <DriveController.h>

#define PI 3.1415926

using namespace std::chrono;

const double WHEEL_RADIUS = 4.0; //inches
const double TICKS_PER_ROT = 4096;

const double MAX_Y_RPM = 480;
double DYN_MAX_Y_RPM = 480;
const double MAX_X_RPM = 300; // Max RPM ACTUAL: 330
const double MAX_YAW_RATE = (17.8 / 508) * MAX_Y_RPM; //max angular velocity divided by the max rpm multiplied by set max rpm

const int DC_SLEEP_TIME = 10;

const int CAN_TALON_FRONT_LEFT = 37;
const int CAN_TALON_BACK_LEFT = 24;
const int CAN_TALON_BACK_RIGHT = 23;
const int CAN_TALON_FRONT_RIGHT = 21;
const int CAN_TALON_KICKER = 22;

double kick_error_vel;
double l_last_error = 0;
double r_last_error = 0;
double yaw_last_error = 0;
double kick_last_error = 0;

const double K_P_YAW_T = 60; // need to invert the yaw direction calculated value

const double K_P_YAW_AU = 70;
const double K_D_YAW_AU = 80;

const double K_P_YAW_HEADING = 0;

const double K_P_LEFT_VEL = 0.0008; // 0.0035 has oscillation on ground big
const double K_F_LEFT_VEL = 1.0 / 508.0; // 0.1 too low
double P_LEFT_VEL_T = 0; // T is for teleop
double P_LEFT_VEL_AU = 0; // AU is for auton

const double K_P_RIGHT_VEL = 0.0008; //0.004 too big wheels off the ground (250 RPM) / 0.001 too small / 0.003 too small still (300 RPM) / 0.0035 small (300) / 0.003 big
const double K_F_RIGHT_VEL = 1.0 / 508.0;
double P_RIGHT_VEL_T = 0;
double P_RIGHT_VEL_AU = 0;

const double K_P_KICK_VEL = .003;
const double K_F_KICK_VEL = 1.0 / 330.0;
double P_KICK_VEL_T = 0;
double P_KICK_VEL_AU = 0;

const double CONVERSION_DIVISION = 4096;
const double CONVERSION_MULTIPLICATION = 600;

const double K_P_RIGHT_DIS = 0.45;
const double K_P_LEFT_DIS = 0.45;
const double K_P_KICKER_DIS = 0;

const double K_I_RIGHT_DIS = 0.000;//5;
const double K_I_LEFT_DIS = 0.000;//5;
const double K_I_KICKER_DIS = 0;

const double K_D_RIGHT_DIS = 0.0;
const double K_D_LEFT_DIS = 0.0;
const double K_D_KICKER_DIS = 0;

double P_RIGHT_DIS = 0;
double I_RIGHT_DIS = 0;
double D_RIGHT_DIS = 0;

double d_right = 0;
double i_right = 0;

double d_yaw_dis = 0;

double d_left = 0;
double i_left = 0;

double P_LEFT_DIS = 0;
double I_LEFT_DIS = 0;
double D_LEFT_DIS = 0;

double P_HEADING = 0;

double l_error_vel_au = 0;
double l_error_dis_au = 0;

double r_error_vel_au = 0;
double r_error_dis_au = 0;

double l_error_vel_t = 0;
double l_error_dis_t = 0;

double r_error_vel_t = 0;
double r_error_dis_t = 0;

double timetokeep = 0.01;

double drive_ref = 0;

double acceptable_yaw_error = .22;

Timer *timerTeleop = new Timer();
Timer *timerAuton = new Timer();

DriveController::DriveController() {

	canTalonFrontLeft = new CANTalon(CAN_TALON_FRONT_LEFT);
	canTalonBackLeft = new CANTalon(CAN_TALON_BACK_LEFT);
	canTalonBackRight = new CANTalon(CAN_TALON_BACK_RIGHT);
	canTalonFrontRight = new CANTalon(CAN_TALON_FRONT_RIGHT);
	canTalonKicker = new CANTalon(CAN_TALON_KICKER);

	ahrs = new AHRS(SPI::Port::kMXP);

	kickerPiston = new DoubleSolenoid(4, 5, 6);

}

void DriveController::HDrive(Joystick *JoyThrottle, Joystick *JoyWheel, //finds targets
		bool is_kick) {

	double target_l, target_r, target_kick, target_yaw_rate;
	double yaw_rate_current = (double) ahrs->GetRawGyroZ()
			* (double) ((PI) / 180); //Right is positive angular velocity

	double axis_ratio = 0.0; //ratio between x and y axes

	if (JoyThrottle->GetX() != 0) { //if x is 0, it is undefined (division)
		axis_ratio = std::abs(JoyThrottle->GetY() / JoyThrottle->GetX()); //dont use regular abs, that returns an int
		DYN_MAX_Y_RPM = MAX_X_RPM * (double) axis_ratio;
		DYN_MAX_Y_RPM = DYN_MAX_Y_RPM > MAX_Y_RPM ? MAX_Y_RPM : DYN_MAX_Y_RPM; //if DYN_max_Y is bigger than MAX_Y then set to MAX_Y, otherwise keep DYN_MAX_Y
	} else {
		DYN_MAX_Y_RPM = MAX_Y_RPM; //deals with special case tht x = 0 (ratio cant divide by zero)
	}

	target_l = -1.0 * (JoyThrottle->GetY() < 0 ? -1 : 1)
			* (JoyThrottle->GetY() * JoyThrottle->GetY()) * DYN_MAX_Y_RPM; //if joy value is less than 0 set to -1, otherwise to 1
	target_r = target_l;

	target_kick = (JoyThrottle->GetX() < 0 ? -1 : 1)
			* (JoyThrottle->GetX() * JoyThrottle->GetX()) * MAX_X_RPM
			* (bool) is_kick; //if joy value is less than 0 set to -1, otherwise to 1

	target_yaw_rate = (JoyWheel->GetX()) * MAX_YAW_RATE;

	target_l = target_l + (target_yaw_rate * (MAX_Y_RPM / MAX_YAW_RATE));
	target_r = target_r - (target_yaw_rate * (MAX_Y_RPM / MAX_YAW_RATE));

	double yaw_error = target_yaw_rate - yaw_rate_current;

	if (std::abs(yaw_error) < acceptable_yaw_error) {

		yaw_error = 0;

	}

	double yaw_output = K_P_YAW_T * yaw_error;

	target_l += yaw_output;
	target_r -= yaw_output;

	if (abs(target_kick) < 35) {
		target_kick = 0;
	}

	if (target_l > MAX_Y_RPM) {
		target_l = MAX_Y_RPM;
	} else if (target_l < -MAX_Y_RPM) {
		target_l = -MAX_Y_RPM;
	}

	if (target_r > MAX_Y_RPM) {
		target_r = MAX_Y_RPM;
	} else if (target_r < -MAX_Y_RPM) {
		target_r = -MAX_Y_RPM;
	}

	Drive(target_kick, target_r, target_l, K_P_RIGHT_VEL, K_P_LEFT_VEL,
			K_P_KICK_VEL);

}

/*
 * Param: Feet forward, + forward
 */
void DriveController::DrivePID(double ref) {

	//conversion to feet
	double r_dis = (((double) canTalonFrontRight->GetEncPosition()
			/ TICKS_PER_ROT) //(negative)
	* (WHEEL_RADIUS * PI) / 12);
	double l_dis = -(((double) canTalonFrontRight->GetEncPosition()
			/ TICKS_PER_ROT) * (WHEEL_RADIUS * PI) / 12);

	l_error_dis_au = ref + r_dis;
	r_error_dis_au = ref - l_dis;

	std::cout << "Error L: " << l_error_dis_au;

	i_right += (r_error_dis_au);
	d_right = (r_error_dis_au - r_last_error);

	i_left += (l_error_dis_au);
	d_left = (l_error_dis_au - l_last_error);

	P_RIGHT_DIS = K_P_RIGHT_DIS * r_error_dis_au;
	I_RIGHT_DIS = K_I_RIGHT_DIS * i_right;
	D_RIGHT_DIS = K_D_RIGHT_DIS * d_right;

	P_LEFT_DIS = K_P_LEFT_DIS * l_error_dis_au;
	I_LEFT_DIS = K_I_LEFT_DIS * i_left;
	D_LEFT_DIS = K_D_LEFT_DIS * d_left;

	double total_right = P_RIGHT_DIS + I_RIGHT_DIS + D_RIGHT_DIS;
	double total_left = P_LEFT_DIS + I_LEFT_DIS + D_LEFT_DIS;

	double target_rpm_right = total_right * MAX_Y_RPM;
	double target_rpm_left = total_left * MAX_Y_RPM;

	double yaw_rate_current = (double) ahrs->GetRawGyroZ()
			* (double) ((PI) / 180);

	double target_yaw_rate = 0.0;

	target_rpm_left = target_rpm_left
			+ (target_yaw_rate * (MAX_Y_RPM / MAX_YAW_RATE));
	target_rpm_right = target_rpm_right
			- (target_yaw_rate * (MAX_Y_RPM / MAX_YAW_RATE));

	double yaw_error = target_yaw_rate - yaw_rate_current;

	if (std::abs(yaw_error) < acceptable_yaw_error) {

		yaw_error = 0;

	}

	d_yaw_dis = yaw_error - yaw_last_error;

	double yaw_output = ((K_P_YAW_AU * yaw_error) + (K_D_YAW_AU * d_yaw_dis));

	target_rpm_right -= yaw_output;
	target_rpm_left += yaw_output;

	if (target_rpm_left > MAX_Y_RPM) {
		target_rpm_left = MAX_Y_RPM;
	} else if (target_rpm_left < -MAX_Y_RPM) {
		target_rpm_left = -MAX_Y_RPM;
	}

	if (target_rpm_right > MAX_Y_RPM) {
		target_rpm_right = MAX_Y_RPM;
	} else if (target_rpm_right < -MAX_Y_RPM) {
		target_rpm_right = -MAX_Y_RPM;
	}

	Drive(0, target_rpm_right, target_rpm_left, K_P_RIGHT_VEL, K_P_LEFT_VEL,
			K_P_KICK_VEL);

	l_last_error = l_error_dis_au;
	r_last_error = r_error_dis_au;

	yaw_last_error = yaw_error;

}

/*
 * Param: radian value, + is right
 */
void DriveController::HeadingPID(Joystick *joyWheel) {

	double target_heading = joyWheel->GetX() * 90;

	double current_heading = (ahrs->GetAngle() < 0 ? -ahrs->GetAngle() : ahrs->GetAngle());

	double error_heading = target_heading - current_heading;

}

void DriveController::Drive(double ref_kick, double ref_right, double ref_left,
		double k_p_right, double k_p_left, double k_p_kick) {

	if (abs(ref_kick) < 35) {
		ref_kick = 0;
	}

	if (ref_left > MAX_Y_RPM) {
		ref_left = MAX_Y_RPM;
	} else if (ref_left < -MAX_Y_RPM) {
		ref_left = -MAX_Y_RPM;
	}

	if (ref_right > MAX_Y_RPM) {
		ref_right = MAX_Y_RPM;
	} else if (ref_right < -MAX_Y_RPM) {
		ref_right = -MAX_Y_RPM;
	}

	double feed_forward_r = K_F_RIGHT_VEL * ref_right;
	double feed_forward_l = K_F_LEFT_VEL * ref_left;
	double feed_forward_k = K_F_KICK_VEL * ref_kick;

	//conversion to RPM from native unit
	double l_current = ((double) canTalonFrontLeft->GetEncVel()
			/ (double) CONVERSION_DIVISION) * CONVERSION_MULTIPLICATION;
	double r_current = -((double) canTalonFrontRight->GetEncVel()
			/ (double) CONVERSION_DIVISION) * CONVERSION_MULTIPLICATION;
	double kick_current = ((double) canTalonKicker->GetEncVel()
			/ (double) CONVERSION_DIVISION) * CONVERSION_MULTIPLICATION; //going right is positive

	l_error_vel_t = ref_left - l_current;
	r_error_vel_t = ref_right - r_current;
	kick_error_vel = ref_kick - kick_current;

	P_LEFT_VEL_T = k_p_left * l_error_vel_t;
	P_RIGHT_VEL_T = k_p_right * r_error_vel_t;
	P_KICK_VEL_T = k_p_kick * kick_error_vel;

	double total_right = P_RIGHT_VEL_T + feed_forward_r;
	double total_left = P_LEFT_VEL_T + feed_forward_l;
	double total_kick = P_KICK_VEL_T + feed_forward_k;

	canTalonFrontLeft->Set(-total_left);
	canTalonBackLeft->Set(-total_left);
	canTalonFrontRight->Set(total_right);
	canTalonBackRight->Set(total_right);
	canTalonKicker->Set(-total_kick);

	std::cout << "Error: " << l_error_vel_t;
	std::cout << " Ref: " << ref_left;
	std::cout << " Current: " << l_current << std::endl;

}

void DriveController::StopAll() {

	canTalonFrontLeft->Set(0);
	canTalonBackLeft->Set(0);

	canTalonFrontRight->Set(0);
	canTalonBackRight->Set(0);

	canTalonKicker->Set(0);
}

void DriveController::KickerUp() {

	kickerPiston->Set(DoubleSolenoid::Value::kReverse);

}

void DriveController::KickerDown() {

	kickerPiston->Set(DoubleSolenoid::Value::kForward);
}

void DriveController::ZeroEncs() {

	canTalonFrontRight->SetEncPosition(0);
	canTalonFrontLeft->SetEncPosition(0);
	canTalonBackRight->SetEncPosition(0);
	canTalonBackLeft->SetEncPosition(0);
	canTalonKicker->SetEncPosition(0);

}

void DriveController::ZeroI() {

	i_right = 0;
	i_left = 0;

}

void DriveController::HDriveWrapper(Joystick *JoyThrottle, Joystick *JoyWheel,
bool *is_kick, DriveController *driveController) {

	timerTeleop->Start();

	while (!frc::RobotState::IsAutonomous()) {

		std::this_thread::sleep_for(std::chrono::milliseconds(DC_SLEEP_TIME));

		if (timerTeleop->HasPeriodPassed(timetokeep)) {

			driveController->HDrive(JoyThrottle, JoyWheel, *is_kick);

		}
	}
}

void DriveController::DrivePIDWrapper(DriveController *driveController) {

	timerAuton->Start();

	while (frc::RobotState::IsAutonomous()) {

		std::this_thread::sleep_for(std::chrono::milliseconds(DC_SLEEP_TIME));

		if (timerAuton->HasPeriodPassed(timetokeep)) {

			driveController->DrivePID(driveController->GetRef());

		}
	}

}

void DriveController::StartTeleopThreads(Joystick *JoyThrottle,
		Joystick *JoyWheel,
		bool *is_kick) {

	DriveController *dc = this;

	std::thread HDriveThread(&DriveController::HDriveWrapper, JoyThrottle,
			JoyWheel, is_kick, dc);
	HDriveThread.detach();

}

void DriveController::StartAutonThreads() {

	DriveController *dc = this;

	std::thread DrivePIDThread(&DriveController::DrivePIDWrapper, dc);
	DrivePIDThread.detach();

}

void DriveController::SetRef(double ref) {

	drive_ref = ref;

}

double DriveController::GetRef() {

	return drive_ref;

}
