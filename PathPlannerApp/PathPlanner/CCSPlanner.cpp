#include "Environment.h"
#include "OccupancyGrid.h"
#include "misc.h"
#include "ARMBuilder.h"
#include "LocalPlanner.h"
#include "AlternativePlanner.h"
#include "Point.h"
#include "Scene.h"
#include "CCSPlanner.h"

#include <memory>

void AddCCSToPath(vector<shared_ptr<Path>> ccs, vector<PathPlanner::PathSegment> &path, float ds);

Shape ShapeToShape(PathPlanner::Polygon poly)
{
	pointList pl;
	for(auto &p : poly.ps)
	{
		pl.push_back(Point(p.x, p.y));
	}

	return Shape(pl);
}


std::ofstream logFile;

void _writeLog(string s)
{
	logFile << s << endl;
}

bool CCSWrapper(PathPlanner::Scene &s, vector<PathPlanner::PathSegment> &path)
{
	bool ret;
	ccsStateTypedef state = ccsStateTypedef::arm;
	logFile.open("CCS_log.txt");

	//Environment
	Environment env(s.GetFieldXLength(), s.GetFieldYLength());

	//Obstacles
	for(auto &e : s.GetEnv())
		env.addObstacle(ShapeToShape(e));

	//Robot
	Robot robot;
	Configuration start(s.GetStartConfig().p.x, s.GetStartConfig().p.y, s.GetStartConfig().phi);
	Configuration goal(s.GetGoalConfig().p.x, s.GetGoalConfig().p.y, s.GetGoalConfig().phi);

	robot.setStart(start);
	robot.setGoal(goal);
	robot.setBody(ShapeToShape(s.GetRobotShape()));

	robot.setAxleDistance(s.GetRobotMinimumRadius());
	robot.setWheelBase(s.GetRobotWheelBase());
	robot.setPhiMax(M_PI_4); //No phiMax, only Rmin

	//Get path from RTR
	configurationList config;
	for(auto &e : s.GetCIPath())
	{
		if(e.type == PathPlanner::TranslationCI)
			config.push_back(Configuration(e.q0.p.x, e.q0.p.y, e.q0.phi));
	}
	config.push_back(Configuration(s.GetCIPath().back().q1.p.x, s.GetCIPath().back().q1.p.y, s.GetCIPath().back().q1.phi));

	Scene sc(env, robot);
	sc.setReversePenalty(s.GetReversePenaltyFactor());
	double ds = s.GetDx();
	sc.setDx(ds);
	sc.setDy(ds);
	OccupancyGrid oG(sc);

	/* Initialize path planner */
	vector<shared_ptr<Path>> ccsPath;
	ARM arm;
	Configuration tempConfig;
	unsigned startIndex = 0;
	unsigned endIndex = config.size() - 1;
	unsigned insertCount = 0;
	double nextDist = 0;
	bool isConfigLeft = true;
	while(isConfigLeft)
	{
		switch(state)
		{
			case ccsStateTypedef::arm:
				arm = ARMBuilder(config[startIndex], oG, sc).getARM();
				logFile << "ARM created from configuration #" << startIndex << "." << endl;
				cout << "ARM created from configuration #" << startIndex << "." << endl;
				state = ccsStateTypedef::localPlanner;
				break;

			case ccsStateTypedef::localPlanner:
				if(endIndex != config.size() - 1)
					nextDist = Point::distance(config[endIndex].position, config[endIndex + 1].position);
				else
					nextDist = 0.0;

				{
					LocalPlanner lp = LocalPlanner(arm, config[endIndex], nextDist, sc);
					logFile << "Local Planner between configurations #" << startIndex << " and #" << endIndex << "." << endl;
					cout << "Local Planner between configurations #" << startIndex << " and #" << endIndex << "." << endl;
					if(lp.hasSolution())
					{
						logFile << "Found!" << endl;
						cout << "Found!" << endl;
						state = ccsStateTypedef::solution;
						//TODO itt lehene ki�rni a CCS eredm�nyt ha �rdekel valakit
						CCS ccs = lp.getShortest();
						ccsPath.push_back(shared_ptr<Path>(new Arc(ccs.getFirst())));
						ccsPath.push_back(shared_ptr<Path>(new Arc(ccs.getMiddle())));
						if(s.IsUseIntermediateS() || (endIndex == config.size() - 1))
						{
							ccsPath.push_back(shared_ptr<Path>(new Segment(ccs.getLast())));
						}
						tempConfig = ccs.getMiddle().getEndConfig();
					}
					else
					{
						state = ccsStateTypedef::fail;
					}
				}
				break;

			case ccsStateTypedef::solution:
				startIndex = endIndex;
				/* Check if this was the last segment */
				if(startIndex != config.size() - 1)
				{
					/* Insert intermediate configuration */
					//TODO itt el k�ne gondolkozni rajta, hogy az S szakaszt kihagyjuk-e
					configurationList::iterator it = config.begin();
					it += ++startIndex;
					config.insert(it, tempConfig);

					state = ccsStateTypedef::arm;
					endIndex = config.size() - 1;
				}
				else
				{
					isConfigLeft = false;
				}
				break;

			case ccsStateTypedef::fail:
				if(endIndex - startIndex == 1)
				{
					if(++insertCount > s.GetInsertCount())
					{
						isConfigLeft = false;
					}
					else
					{
						if(insertCount == 1 && config[endIndex].position != config[startIndex].position)
						{
							/* Insert straigth segment */
							tempConfig.position = config[endIndex].position;
							tempConfig.orientation = config[startIndex].orientation;
							ccsPath.push_back(shared_ptr<Path>(new Segment(config[startIndex], tempConfig.position)));
							startIndex = endIndex;

							/* Insert new start config */
							configurationList::iterator it = config.begin();
							it += startIndex;
							config.insert(it, tempConfig);
							endIndex++;
						}
						else
						{
							/* Insert new end config */
							tempConfig.position = config[endIndex].position;
							tempConfig.orientation = wrapAngle((config[endIndex].orientation + config[startIndex].orientation) * 0.5f);

							configurationList::iterator it = config.begin();
							it += endIndex;
							config.insert(it, tempConfig);
						}
						state = ccsStateTypedef::alternativePlanner;
					}
				}
				else
				{
					/* Select new configuration from predefined path */
					endIndex = (endIndex - startIndex - 1) / 2 + 1 + startIndex;
					state = ccsStateTypedef::localPlanner;
				}
				break;

			case ccsStateTypedef::alternativePlanner:
				{
					AlternativePlanner ap = AlternativePlanner(config[startIndex], config[endIndex], sc);
					logFile << "Alternative Planner between configurations #" << startIndex << " and #" << endIndex << "." << endl;
					cout << "Alternative Planner between configurations #" << startIndex << " and #" << endIndex << "." << endl;
					if(ap.hasSolution())
					{
						logFile << "Found!" << endl;
						cout << "Found!" << endl;
						startIndex = endIndex;
						CCS ccs(ap.getShortest());
						ccsPath.push_back(shared_ptr<Path>(new Arc(ccs.getFirst())));
						ccsPath.push_back(shared_ptr<Path>(new Arc(ccs.getMiddle())));
						if((s.IsUseIntermediateS() || (endIndex == config.size() - 1)) && ccs.getLast().getLength() > 0.0f)
						{
							ccsPath.push_back(shared_ptr<Path>(new Segment(ccs.getLast())));
						}
						else
						{
							configurationList::iterator it = config.begin();
							it += ++startIndex;
							config.insert(it, ccs.getMiddle().getEndConfig());
							endIndex++;
						}
						insertCount--;
						if(insertCount == 0)
						{
							if(startIndex != config.size() - 1)
							{
								endIndex = config.size() - 1;
								state = ccsStateTypedef::arm;
							}
							else
							{
								isConfigLeft = false;
							}
						}
						else
						{
							endIndex++;
						}
					}
					else
					{
						state = ccsStateTypedef::fail;
					}
				}
				break;

			default:
				isConfigLeft = false;
				break;
		}
	}

	if(startIndex != config.size() - 1)
	{
		logFile << "Fail!" << endl;
		cout << "Fail!" << endl;
		ret = false;
	}
	else
	{
		logFile << "Success!" << endl;
		cout << "Success!" << endl;
		startIndex = 0;
		AddCCSToPath(ccsPath, path, s.GetPathDeltaS());
		ret = true;
	}

	logFile.close();
	return ret;
}

PathPlanner::Config ConfigToConfig(Configuration c)
{
	return PathPlanner::Config((float) c.position.x, (float) c.position.y, (float) c.orientation);
}

vector<PathPlanner::Config> ConfigListToConfigVector(configurationList c)
{
	vector<PathPlanner::Config> cc;
	for(auto e : c)
		cc.push_back(ConfigToConfig(e));
	return cc;
}

void AddCCSToPath(vector<shared_ptr<Path>> ccs, vector<PathPlanner::PathSegment> &path, float ds)
{
	bool lastDir = ccs.front()->getDirection();

	//Clear path
	path.clear();

	//First segment
	PathPlanner::PathSegment p;
	p.direction = lastDir;
	path.push_back(p);

	for(auto c : ccs)
	{
		vector<PathPlanner::Config> c1 = ConfigListToConfigVector(c->getPoints(ds));
		if(c->getDirection() != lastDir)
		{
			//Close last segment
			path.back().path.push_back(c1.front());
			path.back().curvature.push_back(path.back().curvature.back());

			//Direction changed -> new segment
			PathPlanner::PathSegment p;
			p.direction = c->getDirection();
			path.push_back(p);

			//Set last direction
			lastDir = c->getDirection();
		}

		path.back().path.insert(path.back().path.end(), c1.begin(), c1.end() - 1);
		path.back().curvature.insert(path.back().curvature.end(), c1.size() - 1, (float) (1 / c->getRadius()));
	}
}