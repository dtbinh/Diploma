#include "PathShifter.h"

PathShifter::PathShifter(PathSegment &pathSegment, CarLikeRobot &car) :
pathSegment(pathSegment),
car(car)
{}


PathShifter::~PathShifter()
{}

PathSegment PathShifter::getShiftedPath(float predict)
{
	PathSegment shiftedPath;
	shiftedPath.direction = pathSegment.direction;
	for(auto &config : pathSegment.path) {
		shiftedPath.path.push_back(transformPosition(config, predict));
	}
	return shiftedPath;
}

Config PathShifter::transformPosition(Config &pos, float predict)
{
	Config newPos;
	float sinF = sin(pos.phi);
	float cosF = cos(pos.phi);
	newPos.p.x = (pathSegment.direction ? 1.0f : -1.0f) * (car.getAxisDistance() + predict) * cosF + pos.p.x;
	newPos.p.y = (pathSegment.direction ? 1.0f : -1.0f) * (car.getAxisDistance() + predict) * sinF + pos.p.y;
	newPos.phi = pos.phi;
	return newPos;
}