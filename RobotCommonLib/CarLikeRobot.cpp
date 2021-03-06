#include "CarLikeRobot.h"
#include <math.h>
#include <vector>
#include <algorithm>

CarLikeRobot::CarLikeRobot()
{
	axisDistance = 295.0f;
	wheelDiameter = 105.0f;
	wheelDistance = 265.0f;
	fiMax = 0.3167f;

	motoraMax = 100.0f;
	motorvMax = 500.0f;
	motorSmoothFactor = 1.0f;
	motorMultFactor = 1.0f;
	steervMax = 1000.0f;
	timeStep = 0.1f;

	robotSpeed = 0.0f;
	robotSteer = 0.0f;
}


CarLikeRobot::~CarLikeRobot()
{
}

float CarLikeRobot::getMaxRadiusRatio(float kappa)
{
	kappa = abs(kappa);
	if (kappa < CAR_EPS)
		return 1.0f;

	float p = 1 + wheelDistance * 0.5f * kappa;
	return p / cos(atan(axisDistance / (1 / kappa + 0.5f * wheelDistance)));
}

void CarLikeRobot::modelRobot(float v, float fi)
{
	float prevSpeed = robotSpeed;

	/* Be�ll�si id� */
	robotSpeed += (v - robotSpeed)*motorSmoothFactor;
	robotSpeed *= motorMultFactor;

	/* Sebess�g tel�t�s */
	if(robotSpeed > motorvMax)
		robotSpeed = motorvMax;
	if(robotSpeed < -motorvMax)
		robotSpeed = -motorvMax;

	/* Gyorsul�s tel�t�s */
	float accel = (robotSpeed - prevSpeed) / timeStep;
	if(accel > motoraMax)
		robotSpeed = prevSpeed + motoraMax * timeStep;
	if(accel < -motoraMax)
		robotSpeed = prevSpeed - motoraMax * timeStep;

	/* Korm�ny sebess�g tel�t�s */
	float steerSpeed = (fi - robotSteer) / timeStep;
	if(steerSpeed > steervMax)
		robotSteer = robotSteer + steervMax * timeStep;
	else if(steerSpeed < -steervMax)
		robotSteer = robotSteer - steervMax * timeStep;
	else
		robotSteer = fi;

	/* Korm�ny tel�t�s */
	if(robotSteer > fiMax)
		robotSteer = fiMax;
	else if(robotSteer < -fiMax)
		robotSteer = -fiMax;

	/* Poz�ci� sz�m�t�s */
	position.phi += 0.5f * robotSpeed * timeStep * tan(robotSteer) / axisDistance;
	position.p.x += robotSpeed * cos(position.phi) * timeStep;
	position.p.y += robotSpeed * sin(position.phi) * timeStep;
	position.phi += 0.5f * robotSpeed * timeStep * tan(robotSteer) / axisDistance;
}