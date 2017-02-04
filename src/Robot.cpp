#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <time.h>
#include <WPILib.h>
#include <IterativeRobot.h>
#include <LiveWindow/LiveWindow.h>
#include <SmartDashboard/SendableChooser.h>
#include <SmartDashboard/SmartDashboard.h>
#include <DriveController.h>
#include <Flywheel.h>
#include <Elevator.h>
#include <GearRail.h>
#include <Conveyor.h>
#include <Climber.h>
#include <TeleopStateMachine.h>
#include <Autonomous.h>
#include <LEDLightStrip.h>



class Robot: public frc::IterativeRobot {
public:

	DriveController *drive_controller;
	Joystick *joyOp, *joyThrottle, *joyWheel;
	Flywheel *fly_wheel;
	Elevator *elevator_;
	GearRail *gear_rail;
	Conveyor *conveyor_;
	Vision *vision_;
	TeleopStateMachine *teleop_state_machine;
	Autonomous *autonomous_;
	Climber *climber_;
	LEDLightStrip *light_;

	const int JOY_OP = 1;
	const int JOY_THROTTLE = 0;
	const int JOY_WHEEL = 2;

	const int CAN_LIGHT = 31;
	const int GEAR_LIGHT_BUTTON = 8;
	const int BALL_LIGHT_BUTTON = 9;
	const int GEAR_AND_BALL_LIGHT_BUTTON = 10;

	const int CLIMB_BUTTON = 6;
	const int GEAR_BUTTON = 5;
	const int FIRE_BUTTON = 7;
	const int RETURN_BUTTON = 3;
	const int POPCORN_BUTTON = 4;
	const int FIRE_BUTTON_2 = 10;

	frc::SendableChooser<std::string> autonChooser;
	frc::SendableChooser<std::string> allianceChooser;

	const std::string gearPlacementUsualAuton = "Gear Placement Usual"; //gear to middle position
	const std::string gearPlacementAlternateAuton = "Gear Placement Alternate"; //gear to side position
	const std::string shootAuton = "Shoot"; //shoot with pre-loaded balls
	const std::string shootAndLoadAuton = "Shoot and Load"; //get balls from hopper, then shoot

	const std::string redAlliance = "Red Alliance";
	const std::string blueAlliance = "Blue Alliance";

	std::string autoSelected;
	std::string allianceSelected;

	const std::string version = "1.0";

	const int HDrive = 0;
	const int Split = 1;
	int driveMode = HDrive; //0 = HDRIVE 1 = split

	bool is_kick;

	double total = 0;

	void RobotInit() {

		joyOp = new Joystick(JOY_OP);
		joyThrottle = new Joystick(JOY_THROTTLE);
		joyWheel = new Joystick(JOY_WHEEL);

		drive_controller = new DriveController();

		fly_wheel = new Flywheel();

		elevator_ = new Elevator();

		gear_rail = new GearRail();

		conveyor_ = new Conveyor();

		vision_ = new Vision();

		autonomous_ = new Autonomous();

		climber_ = new Climber();

		light_ = new LEDLightStrip();

		teleop_state_machine = new TeleopStateMachine(fly_wheel, conveyor_,
				gear_rail, elevator_, drive_controller, vision_, climber_);

		autonChooser.AddDefault(gearPlacementUsualAuton,
				gearPlacementUsualAuton);
		autonChooser.AddObject(gearPlacementAlternateAuton,
				gearPlacementAlternateAuton);
		autonChooser.AddObject(shootAuton, shootAuton);
		autonChooser.AddObject(shootAndLoadAuton, shootAndLoadAuton);

		frc::SmartDashboard::PutData("Auto Modes", &autonChooser);

		allianceChooser.AddDefault(blueAlliance, blueAlliance);
		allianceChooser.AddObject(redAlliance, redAlliance);

		frc::SmartDashboard::PutData("Alliance", &allianceChooser);

		is_kick = true;

	}// RobotInit

	void AutonomousInit() override {

		autoSelected = autonChooser.GetSelected();

		allianceSelected = allianceChooser.GetSelected();

		drive_controller->ZeroEncs();
		drive_controller->ZeroI();

		drive_controller->SetRef(20.0);
		drive_controller->StartAutonThreads();

	}

	void AutonomousPeriodic() {
//
//		if (autoSelected == gearPlacementUsualAuton) {
//
//			std::cout << "Auto 1" << std::endl;
//
//		} else if (autoSelected == gearPlacementAlternateAuton) {
//
//			std::cout << "Auto 2" << std::endl;
//
//		} else if (autoSelected == shootAuton) {
//
//			if (allianceSelected == redAlliance) {
//
//				std::cout << "Red" << std::endl;
//
//			} else {
//
//				std::cout << "Blue" << std::endl;
//
//			}
//
//		} else if (autoSelected == shootAndLoadAuton) {
//
//			if (allianceSelected == redAlliance) {
//
//				std::cout << "Red" << std::endl;
//
//			} else {
//
//				std::cout << "Blue" << std::endl;
//
//			}
//
//		}



	}

	void TeleopInit() {

		fly_wheel->StartThread(); //starts the speed controller

		drive_controller->StartTeleopThreads(joyThrottle, joyWheel, &is_kick); //starts the drive code
		drive_controller->KickerDown();

	}

	void TeleopPeriodic() {

#if 0
		bool gear_button = joyOp->GetRawButton(GEAR_BUTTON);
		bool fire_button = joyOp->GetRawButton(FIRE_BUTTON);
		bool climb_button = joyOp->GetRawButton(CLIMB_BUTTON);
		bool return_button = joyOp->GetRawButton(RETURN_BUTTON);
		bool gear_light_button = joyOp->GetRawButton(GEAR_LIGHT_BUTTON);
		bool ball_light_button = joyOp->GetRawButton(BALL_LIGHT_BUTTON);
		bool gear_and_ball_light_button = joyOp->GetRawButton(GEAR_AND_BALL_LIGHT_BUTTON);
		bool popcorn_button = joyOp->GetRawButton(POPCORN_BUTTON);
		bool second_fire_button = joyOp->GetRawButton(FIRE_BUTTON_2);

		teleop_state_machine->StateMachine(gear_button, fire_button, climb_button, return_button, popcorn_button, second_fire_button);

		light_->LEDStateMachine(gear_light_button, ball_light_button, gear_and_ball_light_button);
		conveyor_->ConStateMachine();
		elevator_->ElevatorStateMachine();
		fly_wheel->FlywheelStateMachine();
		gear_rail->GearRailStateMachine();
		climber_->ClimberStateMachine();
#endif


		//START DRIVE CODE
		const int HDrive = 0;
		const int Split = 1;


		bool hDrive = joyThrottle->GetRawButton(1);
		bool arcadeDrive = joyThrottle->GetRawButton(2);

		switch (driveMode) {

		case HDrive:

			is_kick = true;

			if (arcadeDrive) {

				drive_controller->StopAll();

				drive_controller->KickerDown(); //Kicker to stay down the whole time

				driveMode = Split;
			}

			break;

		case Split: //same as HDrive for now, set is_kick to false in this state and use KickerUp() in the if of HDrive state to use split arcade

			is_kick = true;  //false /New plan to only use HDrive

			if (hDrive) {

				drive_controller->StopAll();

				drive_controller->KickerDown();

				driveMode = HDrive;

			}

			break;

		}
		//END DRIVECODE


	} // TeleopPeriodic


	void TestPeriodic() {


	}

private:

};

START_ROBOT_CLASS(Robot)
